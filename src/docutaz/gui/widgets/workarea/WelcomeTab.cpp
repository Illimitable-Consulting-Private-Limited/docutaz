#include "docutaz/gui/widgets/workarea/WelcomeTab.h"

#include <QLabel>
#include <QPalette>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShowEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>

#include "docutaz/core/engine/MongoshEngine.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/EventBus.h"
#include "docutaz/core/events/MongoEvents.h"
#include "docutaz/gui/dialogs/PreferencesDialog.h"
#include "docutaz/gui/Theme.h"

namespace Docutaz
{

// Body copy as a template; the %MUTED%/%INTRO%/%HR%/%STRONG%/%BODY% colour
// tokens are filled in at runtime from the palette so the text stays legible in
// both light and dark themes (the app follows the OS palette, it has no theme of
// its own).
static const char* BODY_HTML = R"(
<table width='100%' cellspacing='0' cellpadding='0'>
<tr><td>

<p style='margin:0 0 4px 0; font-size:11pt; color:%MUTED%;'>
  <i>Pronounced &nbsp;<b>dok &middot; you &middot; taz</b></i>
</p>

<p style='margin:0 0 18px 0; font-size:13pt; color:%INTRO%;'>
  A cross-platform MongoDB management tool<br>
  built for developers who work with documents.
</p>

<hr style='border:none; border-top:1px solid %HR%; margin:0 0 18px 0;'/>

<p style='margin:0 0 6px 0; font-size:11pt; color:%STRONG%;'><b>About the name</b></p>
<p style='margin:0 0 18px 0; font-size:10pt; color:%BODY%; line-height:1.6;'>
  <i>Dotaz</i> is the Czech word for <i>query</i>.
  Combine it with <i>Document</i> and you get <b>Docutaz</b> &mdash;
  a tool that puts document querying at the centre of your workflow.
  The name captures both what MongoDB stores (documents) and what you
  do with them (query), in one word that crosses language boundaries.
</p>

<p style='margin:0 0 6px 0; font-size:11pt; color:%STRONG%;'><b>What you can do</b></p>
<ul style='margin:0 0 18px 0; padding-left:20px; font-size:10pt; color:%BODY%; line-height:1.8;'>
  <li>Connect to local or remote MongoDB instances, replica sets and authenticated deployments</li>
  <li>Browse databases, collections and documents in tree, table or raw text view</li>
  <li>Run multi-statement JavaScript queries using the built-in mongosh shell</li>
  <li>Insert, edit and delete documents with a full-featured visual editor</li>
  <li>Manage indexes, users and collection structure</li>
  <li>Page through large result sets with configurable batch sizes</li>
  <li>Use <code>use &lt;database&gt;</code> directly in the shell to switch contexts</li>
</ul>

<p style='margin:0 0 6px 0; font-size:11pt; color:%STRONG%;'><b>Getting started</b></p>
<p style='margin:0; font-size:10pt; color:%BODY%; line-height:1.6;'>
  Click <b>Connect</b> in the toolbar or press
  <b>Ctrl+,</b> to open the connection manager and add your first MongoDB server.
  Docutaz will remember your connections across sessions.
</p>

</td></tr>
</table>
)";

WelcomeTab::WelcomeTab(QScrollArea* parent)
    : QWidget(parent), _parent(parent)
{
    setContentsMargins(0, 0, 0, 0);

    _logoPx.load(":/docutaz/docutaz-branding-trans.png");

    _logo = new QLabel(this);
    _logo->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    _logo->setContentsMargins(0, 0, 0, 0);

    _body = new QLabel(this);
    _body->setTextFormat(Qt::RichText);
    _body->setWordWrap(true);
    _body->setOpenExternalLinks(true);
    _body->setContentsMargins(0, 0, 0, 0);
    _body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Proactive "mongosh not detected" card — shown only when mongosh can't be
    // found, so the unzip-and-run crowd is nudged before they hit a failed query.
    // Amber warning styling, toned down for dark palettes; the message text
    // colour is pinned too, otherwise it inherits the (light) palette text and
    // washes out against the card. (Colours applied in applyTheme.)
    _mongoshCard = new QFrame(this);
    _mongoshCard->setObjectName("mongoshCard");
    {
        auto* cardLayout = new QVBoxLayout(_mongoshCard);
        cardLayout->setContentsMargins(14, 12, 14, 12);
        cardLayout->setSpacing(8);

        auto* msg = new QLabel(
            "<b>⚠ mongosh not detected.</b>  Docutaz runs your queries through "
            "MongoDB's <code>mongosh</code> shell, which isn't bundled. Install it "
            "and make sure it's on your <code>PATH</code> — or point Docutaz at it "
            "in Preferences.", _mongoshCard);
        msg->setTextFormat(Qt::RichText);
        msg->setWordWrap(true);

        auto* btnRow = new QHBoxLayout;
        auto* download = new QPushButton("Download mongosh", _mongoshCard);
        VERIFY(connect(download, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl("https://www.mongodb.com/try/download/shell"));
        }));
        auto* setPath = new QPushButton("Set path…", _mongoshCard);
        VERIFY(connect(setPath, &QPushButton::clicked, this, [this] {
            PreferencesDialog dlg(this);
            dlg.exec();
            refreshMongoshCard();   // configuring a path here should clear the card
        }));
        btnRow->addWidget(download);
        btnRow->addWidget(setPath);
        btnRow->addStretch();

        cardLayout->addWidget(msg);
        cardLayout->addLayout(btnRow);
    }

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(16);
    layout->addWidget(_logo, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(_mongoshCard);
    layout->addWidget(_body);
    layout->addStretch();
    setLayout(layout);

    // Fill the theme-dependent colours now, and re-fill on a live colour-scheme
    // change (the body HTML and card styling bake in light/dark colours).
    applyTheme();
    VERIFY(connect(Theme::Notifier::instance(), &Theme::Notifier::changed,
                   this, &WelcomeTab::applyTheme));

    AppRegistry::instance().bus()->subscribe(this, MongoshSettingsChangedEvent::Type);
    refreshMongoshCard();
}

void WelcomeTab::applyTheme()
{
    const bool dark = Theme::isDark();

    QString bodyHtml = QString::fromUtf8(BODY_HTML);
    bodyHtml.replace("%MUTED%",  dark ? "#9aa0a6" : "#666");
    bodyHtml.replace("%INTRO%",  dark ? "#e8eaed" : "#222");
    bodyHtml.replace("%HR%",     dark ? "#3c4043" : "#ddd");
    bodyHtml.replace("%STRONG%", dark ? "#f1f3f4" : "#111");
    bodyHtml.replace("%BODY%",   dark ? "#c0c4c9" : "#444");
    _body->setText(bodyHtml);

    _mongoshCard->setStyleSheet(
        dark
        ? "QFrame#mongoshCard { background-color: #3a2f15; border: 1px solid #a17a2c;"
          " border-radius: 6px; }"
          " QFrame#mongoshCard QLabel { color: #f3d9a6; background: transparent; }"
        : "QFrame#mongoshCard { background-color: #FFF4E5; border: 1px solid #E6A23C;"
          " border-radius: 6px; }"
          " QFrame#mongoshCard QLabel { color: #5c4612; background: transparent; }");
}

void WelcomeTab::handle(MongoshSettingsChangedEvent*)
{
    refreshMongoshCard();
}

void WelcomeTab::refreshMongoshCard()
{
    if (_mongoshCard)
        _mongoshCard->setVisible(!MongoshEngine::isMongoshAvailable());
}

void WelcomeTab::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshMongoshCard();
    resize();
}

void WelcomeTab::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    resize();
}

void WelcomeTab::resize()
{
    if (_logoPx.isNull()) return;

    // On the first showEvent the parent scroll area may not be laid out yet, so
    // its width can be 0/tiny. Clamp to a sane minimum so we never compute a
    // non-positive target width (which would scale the logo down to nothing).
    const int availW = (_parent ? _parent->width() - 64 : 480);
    const int maxW   = qBound(240, availW, 520);
    const int h      = (_logoPx.height() * maxW) / _logoPx.width();
    _logo->setPixmap(_logoPx.scaled(maxW, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

} // namespace Docutaz
