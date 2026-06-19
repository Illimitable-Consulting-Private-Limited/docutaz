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
#define INSTALL_URL \
    "https://github.com/Illimitable-Consulting-Private-Limited/docutaz/blob/main/docs/INSTALL.md#linux-x86_64-and-arm64"

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

/* Run argv[0], capturing up to n-1 bytes of its stdout into out (NUL-terminated).
 * Same return convention as run_cmd (127 == not found / failed). */
static int run_capture(char *const argv[], char *out, size_t n)
{
    int fds[2];
    if (pipe(fds) != 0)
        return -1;
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    ssize_t r = read(fds[0], out, n - 1);
    out[r > 0 ? (size_t)r : 0] = '\0';
    close(fds[0]);
    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
        return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

/* Open the install guide in the user's browser, detached; best effort. */
static void open_url(const char *url)
{
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { "xdg-open", (char *)url, NULL };
        execvp(argv[0], argv);
        _exit(127);
    }
    if (pid > 0)
        waitpid(pid, NULL, WNOHANG);
}

/* Show msg graphically via whichever dialog tool is installed, with an
 * "Open install guide" button that launches INSTALL_URL in a browser (plain
 * URL text inside the box keeps it copy-pasteable too). zenity -> kdialog ->
 * xmessage; a tool that isn't installed (exit 127) falls through to the next. */
static void show_gui(const char *msg)
{
    static const char *OPEN_LABEL = "Open install guide";

    /* zenity: extra-button is reported on stdout (exit code can't distinguish
     * it from a normal close), so capture stdout to detect the click. */
    char *zenity[] = { "zenity", "--error", "--no-wrap",
                       "--title", (char *)DIALOG_TITLE, "--text", (char *)msg,
                       "--ok-label", "Close",
                       "--extra-button", (char *)OPEN_LABEL, NULL };
    char out[64];
    if (run_capture(zenity, out, sizeof(out)) != 127) {
        if (strncmp(out, OPEN_LABEL, strlen(OPEN_LABEL)) == 0)
            open_url(INSTALL_URL);
        return;
    }

    /* kdialog: yes/no with relabelled buttons; "yes" (exit 0) == open guide. */
    char *kdialog[] = { "kdialog", "--title", (char *)DIALOG_TITLE,
                        "--warningyesno", (char *)msg,
                        "--yes-label", (char *)OPEN_LABEL,
                        "--no-label", "Close", NULL };
    int rc = run_cmd(kdialog);
    if (rc != 127) {
        if (rc == 0)
            open_url(INSTALL_URL);
        return;
    }

    /* xmessage: each button maps to its declared exit code. */
    char *xmessage[] = { "xmessage", "-center", "-title", (char *)DIALOG_TITLE,
                         "-buttons", "Open install guide:10,Close:0",
                         (char *)msg, NULL };
    if (run_cmd(xmessage) == 10)
        open_url(INSTALL_URL);
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

        char body[8192];
        snprintf(body, sizeof(body),
                 "Docutaz can't start: required system libraries are missing.\n\n"
                 "Missing:\n%s\n"
                 "These come from Qt 6, QScintilla (Qt 6), OpenSSL 3 and libssh2.\n"
                 "Install them with your package manager, then run Docutaz again\n"
                 "(\">\" marks your detected distribution):\n\n"
                 "%s\n"
                 "Full instructions (also in README.txt / docs/INSTALL.md):\n",
                 missing, cmds);

        if (isatty(STDERR_FILENO)) {
            fputs(body, stderr);
            /* OSC 8 hyperlink: clickable in modern terminals, shown as a plain
             * URL on those that don't understand the escape. */
            fprintf(stderr, "\033]8;;%s\033\\%s\033]8;;\033\\\n",
                    INSTALL_URL, INSTALL_URL);
        } else {
            /* GUI box: keep the URL as readable/copyable text; the dialog adds
             * an "Open install guide" button that launches it. */
            char gmsg[8192 + 512];
            snprintf(gmsg, sizeof(gmsg), "%s%s\n", body, INSTALL_URL);
            show_gui(gmsg);
        }
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
