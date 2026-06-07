#include "robomongo/gui/widgets/workarea/WelcomeTab.h"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

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

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 28, 32, 28);
    layout->setSpacing(16);
    layout->addWidget(_logo, 0, Qt::AlignLeft | Qt::AlignTop);
    layout->addWidget(body);
    layout->addStretch();
    setLayout(layout);
}

void WelcomeTab::resize()
{
    if (_logoPx.isNull()) return;

    const int availW = _parent ? _parent->width() - 64 : 480;
    const int maxW   = qMin(availW, 520);
    const int h      = (_logoPx.height() * maxW) / _logoPx.width();
    _logo->setPixmap(_logoPx.scaled(maxW, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

} // namespace Docutaz
