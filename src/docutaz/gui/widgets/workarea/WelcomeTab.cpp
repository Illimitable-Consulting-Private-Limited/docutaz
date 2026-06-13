#include "docutaz/gui/widgets/workarea/WelcomeTab.h"

#include <QLabel>
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
#include "docutaz/gui/dialogs/PreferencesDialog.h"

namespace Docutaz
{

static const char* BODY_HTML = R"(
<table width='100%' cellspacing='0' cellpadding='0'>
<tr><td>

<p style='margin:0 0 4px 0; font-size:11pt; color:#666;'>
  <i>Pronounced &nbsp;<b>dok &middot; you &middot; taz</b></i>
</p>

<p style='margin:0 0 18px 0; font-size:13pt; color:#222;'>
  A cross-platform MongoDB management tool<br>
  built for developers who work with documents.
</p>

<hr style='border:none; border-top:1px solid #ddd; margin:0 0 18px 0;'/>

<p style='margin:0 0 6px 0; font-size:11pt; color:#111;'><b>About the name</b></p>
<p style='margin:0 0 18px 0; font-size:10pt; color:#444; line-height:1.6;'>
  <i>Dotaz</i> is the Czech word for <i>query</i>.
  Combine it with <i>Document</i> and you get <b>Docutaz</b> &mdash;
  a tool that puts document querying at the centre of your workflow.
  The name captures both what MongoDB stores (documents) and what you
  do with them (query), in one word that crosses language boundaries.
</p>

<p style='margin:0 0 6px 0; font-size:11pt; color:#111;'><b>What you can do</b></p>
<ul style='margin:0 0 18px 0; padding-left:20px; font-size:10pt; color:#444; line-height:1.8;'>
  <li>Connect to local or remote MongoDB instances, replica sets and authenticated deployments</li>
  <li>Browse databases, collections and documents in tree, table or raw text view</li>
  <li>Run multi-statement JavaScript queries using the built-in mongosh shell</li>
  <li>Insert, edit and delete documents with a full-featured visual editor</li>
  <li>Manage indexes, users and collection structure</li>
  <li>Page through large result sets with configurable batch sizes</li>
  <li>Use <code>use &lt;database&gt;</code> directly in the shell to switch contexts</li>
</ul>

<p style='margin:0 0 6px 0; font-size:11pt; color:#111;'><b>Getting started</b></p>
<p style='margin:0; font-size:10pt; color:#444; line-height:1.6;'>
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

    auto* body = new QLabel(this);
    body->setTextFormat(Qt::RichText);
    body->setWordWrap(true);
    body->setOpenExternalLinks(true);
    body->setContentsMargins(0, 0, 0, 0);
    body->setText(QString::fromUtf8(BODY_HTML));
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    // Proactive "mongosh not detected" card — shown only when mongosh can't be
    // found, so the unzip-and-run crowd is nudged before they hit a failed query.
    _mongoshCard = new QFrame(this);
    _mongoshCard->setObjectName("mongoshCard");
    _mongoshCard->setStyleSheet(
        "QFrame#mongoshCard { background-color: #FFF4E5; border: 1px solid #E6A23C;"
        " border-radius: 6px; }");
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
    layout->addWidget(body);
    layout->addStretch();
    setLayout(layout);

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
