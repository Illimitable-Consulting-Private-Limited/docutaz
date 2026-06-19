/*
 * Docutaz Linux preflight launcher.
 *
 * Shipped as the top-level `docutaz` in the Linux tarball. The real GUI binary
 * lives in libexec/docutaz-bin and hard-links Qt6, QScintilla, OpenSSL, libssh2
 * (and GL/zlib) from the host. If any of those are missing, the dynamic loader
 * kills the GUI *before* main() runs, with a cryptic "error while loading shared
 * libraries" message on the terminal — and nothing at all when it is launched
 * from the desktop menu. A check inside the GUI binary can't help: it never gets
 * to run.
 *
 * This launcher depends only on glibc (+ libdl), so it always starts. It
 * dlopen-probes the system libraries the GUI needs — the exact soname list is
 * generated at packaging time into libexec/required-libs.txt — and, if any are
 * missing, shows how to install them (on the terminal, or via a GUI dialog when
 * there is no terminal) instead of letting the app crash. When everything is
 * present it points LD_LIBRARY_PATH at the bundled mongo driver libs and exec()s
 * the real GUI binary.
 *
 * It is intentionally NOT used inside the Flatpak: there the runtime/manifest
 * provides every library, so the manifest runs libexec/docutaz-bin directly.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define DIALOG_TITLE "Docutaz \xE2\x80\x94 missing dependencies"

/* Per-distro install commands. The Debian/Fedora package names are kept in sync
 * with docs/INSTALL.md; Arch/openSUSE are best-effort. We always print all of
 * them (marking the detected one) so an unrecognized distro is never handed a
 * command that can't work. */
static const char *DEBIAN_CMD =
    "sudo apt install libqt6widgets6 libqt6network6 libqt6xml6 libqt6printsupport6 "
    "libqt6svg6 libqscintilla2-qt6-15 libssh2-1 libssl3";
static const char *FEDORA_CMD =
    "sudo dnf install qt6-qtbase qt6-qtbase-gui qt6-qtsvg qscintilla-qt6 libssh2 openssl";
static const char *ARCH_CMD =
    "sudo pacman -S qt6-base qt6-svg qscintilla-qt6 libssh2 openssl";
static const char *SUSE_CMD =
    "sudo zypper install libQt6Widgets6 libQt6Network6 libQt6Xml6 libQt6PrintSupport6 "
    "libQt6Svg6 libqscintilla2_qt6-15 libssh2-1 libopenssl3";

typedef enum { DISTRO_UNKNOWN, DISTRO_DEBIAN, DISTRO_FEDORA, DISTRO_ARCH, DISTRO_SUSE } distro_t;

/* Directory containing this executable (the bundle root), via /proc/self/exe. */
static int self_dir(char *out, size_t n)
{
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0)
        return -1;
    buf[len] = '\0';
    char *slash = strrchr(buf, '/');
    if (!slash)
        return -1;
    *slash = '\0';
    if ((size_t)snprintf(out, n, "%s", buf) >= n)
        return -1;
    return 0;
}

/* Best-effort host distribution family, from /etc/os-release ID / ID_LIKE. */
static distro_t detect_distro(void)
{
    FILE *f = fopen("/etc/os-release", "r");
    if (!f)
        return DISTRO_UNKNOWN;
    char line[256];
    distro_t d = DISTRO_UNKNOWN;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "ID=", 3) != 0 && strncmp(line, "ID_LIKE=", 8) != 0)
            continue;
        if (strstr(line, "debian") || strstr(line, "ubuntu")) { d = DISTRO_DEBIAN; break; }
        if (strstr(line, "fedora") || strstr(line, "rhel") ||
            strstr(line, "centos")) { d = DISTRO_FEDORA; break; }
        if (strstr(line, "arch")) { d = DISTRO_ARCH; break; }
        if (strstr(line, "suse")) { d = DISTRO_SUSE; break; }
    }
    fclose(f);
    return d;
}

/* Run argv[0] with its args, return its exit code (127 == not found / failed). */
static int run_cmd(char *const argv[])
{
    pid_t pid = fork();
    if (pid < 0)
        return -1;
    if (pid == 0) {
        execvp(argv[0], argv);
        _exit(127); /* command not on PATH */
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Show msg graphically via whichever dialog tool is installed. */
static void show_gui(const char *msg)
{
    char *zenity[] = { "zenity", "--error", "--no-wrap",
                       "--title", (char *)DIALOG_TITLE, "--text", (char *)msg, NULL };
    if (run_cmd(zenity) != 127)
        return;
    char *kdialog[] = { "kdialog", "--title", (char *)DIALOG_TITLE,
                        "--error", (char *)msg, NULL };
    if (run_cmd(kdialog) != 127)
        return;
    char *xmessage[] = { "xmessage", "-center", "-title", (char *)DIALOG_TITLE,
                         (char *)msg, NULL };
    run_cmd(xmessage);
}

int main(int argc, char *argv[])
{
    char dir[PATH_MAX];
    if (self_dir(dir, sizeof(dir)) != 0) {
        fprintf(stderr, "docutaz: cannot resolve own path\n");
        return 1;
    }

    char realbin[PATH_MAX], libdir[PATH_MAX], reqfile[PATH_MAX];
    snprintf(realbin, sizeof(realbin), "%s/libexec/docutaz-bin", dir);
    snprintf(libdir, sizeof(libdir), "%s/lib", dir);
    snprintf(reqfile, sizeof(reqfile), "%s/libexec/required-libs.txt", dir);

    /* ── Preflight: probe the system libraries the GUI binary needs ──────── */
    /* required-libs.txt is generated at packaging time and lists only the
     * host-provided sonames (the bundled mongo driver libs are excluded), so a
     * dlopen failure here means a genuinely missing system package. */
    char missing[4096];
    size_t missing_len = 0;
    missing[0] = '\0';

    FILE *f = fopen(reqfile, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\r\n")] = '\0';
            char *s = line;
            while (*s == ' ' || *s == '\t')
                s++;
            if (*s == '\0' || *s == '#')
                continue;
            void *h = dlopen(s, RTLD_LAZY | RTLD_LOCAL);
            if (h) {
                dlclose(h);
            } else {
                int wrote = snprintf(missing + missing_len, sizeof(missing) - missing_len,
                                     "  %s\n", s);
                if (wrote > 0 && (size_t)wrote < sizeof(missing) - missing_len)
                    missing_len += (size_t)wrote;
            }
        }
        fclose(f);
    }

    if (missing_len > 0) {
        /* Show every distro's command, marking the detected one with ">", so an
         * unrecognized distribution is never left with an unusable command. */
        distro_t d = detect_distro();
        #define SEL   "> "
        #define NOSEL "  "
        char cmds[2048];
        snprintf(cmds, sizeof(cmds),
                 "%sDebian/Ubuntu:  %s\n"
                 "%sFedora/RHEL:    %s\n"
                 "%sArch:           %s\n"
                 "%sopenSUSE:       %s\n",
                 d == DISTRO_DEBIAN ? SEL : NOSEL, DEBIAN_CMD,
                 d == DISTRO_FEDORA ? SEL : NOSEL, FEDORA_CMD,
                 d == DISTRO_ARCH   ? SEL : NOSEL, ARCH_CMD,
                 d == DISTRO_SUSE   ? SEL : NOSEL, SUSE_CMD);

        char msg[8192];
        snprintf(msg, sizeof(msg),
                 "Docutaz can't start: required system libraries are missing.\n\n"
                 "Missing:\n%s\n"
                 "These come from Qt 6, QScintilla (Qt 6), OpenSSL 3 and libssh2.\n"
                 "Install them with your package manager, then run Docutaz again\n"
                 "(\">\" marks your detected distribution):\n\n"
                 "%s\n"
                 "See README.txt / docs/INSTALL.md for details.\n",
                 missing, cmds);

        if (isatty(STDERR_FILENO))
            fputs(msg, stderr);
        else
            show_gui(msg);
        return 1;
    }

    /* ── All deps present: point the loader at the bundled mongo libs and run
     * the real GUI binary. ──────────────────────────────────────────────── */
    const char *prev = getenv("LD_LIBRARY_PATH");
    char ldpath[PATH_MAX * 2];
    if (prev && *prev)
        snprintf(ldpath, sizeof(ldpath), "%s:%s", libdir, prev);
    else
        snprintf(ldpath, sizeof(ldpath), "%s", libdir);
    setenv("LD_LIBRARY_PATH", ldpath, 1);

    argv[0] = realbin;
    execv(realbin, argv);

    /* execv only returns on failure. */
    fprintf(stderr, "docutaz: failed to launch %s: %s\n", realbin, strerror(errno));
    return 1;
}
