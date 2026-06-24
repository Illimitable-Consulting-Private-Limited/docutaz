#include "docutaz/gui/widgets/workarea/QueryHistoryTab.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QSplitter>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QPainter>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <algorithm>

#include "docutaz/core/QueryHistory.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/utils/QtUtils.h"

#include "docutaz/gui/editors/PlainJavaScriptEditor.h"
#include "docutaz/gui/editors/JSLexer.h"
#include "docutaz/gui/Theme.h"

namespace Docutaz
{
    namespace
    {
        constexpr int RoleIndex     = Qt::UserRole;       // index into entries()
        constexpr int RoleMeta      = Qt::UserRole + 1;   // dimmed second line
        constexpr int RolePinned    = Qt::UserRole + 2;   // bool
        constexpr int RoleTypeColor = Qt::UserRole + 3;   // QColor for the type chip
        constexpr int RoleTypeLabel = Qt::UserRole + 4;   // op-type label for the chip

        struct TypeInfo { QString label; QColor color; };

        // Classify an entry for the type indicator / Type filter. A composite
        // script takes precedence over its dominant verb.
        TypeInfo typeInfo(const QueryHistoryEntry &e)
        {
            if (QueryHistoryManager::isScript(e.query))
                return { QStringLiteral("script"), QColor(0x16, 0xa0, 0x85) };
            const QString k = e.kind.isEmpty()
                ? QueryHistoryManager::deriveKind(e.query) : e.kind;
            if (k == "aggregate")                       return { k, QColor(0x8e, 0x44, 0xad) };
            if (k == "find" || k == "count")            return { QStringLiteral("find"),  QColor(0x2e, 0x86, 0xde) };
            if (k == "insert" || k == "update" || k == "delete")
                                                        return { QStringLiteral("write"), QColor(0xe6, 0x7e, 0x22) };
            return { QStringLiteral("other"), QColor(0x90, 0x90, 0x90) };
        }

        // Tokenized-search matcher: tokens like `conn:`, `db:`, `coll:`, `kind:`,
        // `pinned:`; any other word must appear in the query text. All terms AND.
        bool matchesTokens(const QueryHistoryEntry &e, const QString &text)
        {
            const QStringList toks = text.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            for (const QString &tok : toks) {
                const int c = tok.indexOf(':');
                const QString key = c > 0 ? tok.left(c).toLower() : QString();
                const QString val = c > 0 ? tok.mid(c + 1) : QString();
                if (key == "conn") {
                    if (!e.connection.contains(val, Qt::CaseInsensitive)) return false;
                } else if (key == "db") {
                    if (!e.database.contains(val, Qt::CaseInsensitive)) return false;
                } else if (key == "coll") {
                    bool ok = false;
                    for (const QString &name : QueryHistoryManager::collectionsOf(e.query))
                        if (name.contains(val, Qt::CaseInsensitive)) { ok = true; break; }
                    if (!ok) return false;
                } else if (key == "kind") {
                    if (!typeInfo(e).label.contains(val, Qt::CaseInsensitive)
                        && !e.kind.contains(val, Qt::CaseInsensitive)) return false;
                } else if (key == "pinned") {
                    const bool want = val.compare("true", Qt::CaseInsensitive) == 0
                                      || val == "1" || val.compare("yes", Qt::CaseInsensitive) == 0;
                    if (e.pinned != want) return false;
                } else {
                    // Free text (or an unknown key): match the whole token in the query.
                    if (!e.query.contains(tok, Qt::CaseInsensitive)) return false;
                }
            }
            return true;
        }

        // Two-line row with a type-colored left bar.
        class RowDelegate : public QStyledItemDelegate
        {
        public:
            using QStyledItemDelegate::QStyledItemDelegate;

            QSize sizeHint(const QStyleOptionViewItem &opt, const QModelIndex &) const override
            {
                QFontMetrics fm(opt.font);
                return QSize(0, fm.height() * 2 + 16);
            }

            void paint(QPainter *p, const QStyleOptionViewItem &option,
                       const QModelIndex &index) const override
            {
                QStyleOptionViewItem opt(option);
                initStyleOption(&opt, index);
                const QWidget *w = opt.widget;
                QStyle *style = w ? w->style() : QApplication::style();
                const bool    sel    = opt.state & QStyle::State_Selected;

                // Suppress the style's full brand-green selection fill (too loud);
                // we paint a subtle tint + a left accent bar instead.
                opt.text.clear();
                opt.state &= ~(QStyle::State_Selected | QStyle::State_HasFocus);
                style->drawControl(QStyle::CE_ItemViewItem, &opt, p, w);

                // Thin separator between rows (replaces zebra striping, which does
                // not fit the flat UI).
                p->setPen(Theme::current().mid);
                p->drawLine(option.rect.left(), option.rect.bottom(),
                            option.rect.right(), option.rect.bottom());

                // Selection: a 3px brand-green left bar only (no background fill).
                if (sel) {
                    QRect barRect = option.rect;
                    barRect.setWidth(3);
                    p->fillRect(barRect, Theme::current().highlight);
                }

                const QString query  = index.data(Qt::DisplayRole).toString();
                const QString meta   = index.data(RoleMeta).toString();
                const bool    pinned = index.data(RolePinned).toBool();
                const QColor  type   = index.data(RoleTypeColor).value<QColor>();
                const QString typeLbl= index.data(RoleTypeLabel).toString().toUpper();

                p->setRenderHint(QPainter::Antialiasing, true);

                const QColor textColor = opt.palette.color(QPalette::Text);
                const QColor metaColor = opt.palette.color(QPalette::Disabled, QPalette::Text);

                // Op-type chip: a saturated pill with the uppercased type label,
                // right-aligned and vertically centred (a blessed colour
                // exception). White text on the type colour reads as a clear badge.
                QRect chipRect;
                if (!typeLbl.isEmpty()) {
                    QFont cf = opt.font;
                    if (cf.pointSizeF() > 0) cf.setPointSizeF(cf.pointSizeF() - 1.5);
                    cf.setLetterSpacing(QFont::PercentageSpacing, 104);
                    const QFontMetrics cfm(cf);
                    const int cw = cfm.horizontalAdvance(typeLbl) + 18;
                    const int ch = cfm.height() + 6;
                    chipRect = QRect(opt.rect.right() - 10 - cw,
                                     opt.rect.center().y() - ch / 2, cw, ch);
                    QColor bg = type;
                    bg.setAlpha(Theme::isDark() ? 235 : 220);
                    p->setBrush(bg);
                    p->setPen(Qt::NoPen);
                    p->drawRoundedRect(chipRect, ch / 2.0, ch / 2.0);
                    p->setFont(cf);
                    p->setPen(QColor(Qt::white));
                    p->drawText(chipRect, Qt::AlignCenter, typeLbl);
                }

                // Text block, kept clear of the chip.
                const int rightInset = chipRect.isNull() ? 8 : (opt.rect.right() - chipRect.left() + 8);
                const QRect r(opt.rect.left() + 12, opt.rect.top() + 5,
                              opt.rect.width() - 12 - rightInset, opt.rect.height() - 10);

                QFont qf = opt.font;
                p->setFont(qf);
                const QFontMetrics qfm(qf);
                p->setPen(textColor);
                const QString line1 = (pinned ? QString::fromUtf8("\xE2\x98\x85 ") : QString()) + query;
                p->drawText(QRect(r.left(), r.top(), r.width(), qfm.height()),
                            Qt::AlignLeft | Qt::AlignVCenter,
                            qfm.elidedText(line1, Qt::ElideRight, r.width()));

                QFont mf = opt.font;
                if (mf.pointSizeF() > 0) mf.setPointSizeF(mf.pointSizeF() - 1.0);
                p->setFont(mf);
                const QFontMetrics mfm(mf);
                p->setPen(metaColor);
                p->drawText(QRect(r.left(), r.top() + qfm.height() + 2, r.width(), mfm.height()),
                            Qt::AlignLeft | Qt::AlignVCenter,
                            mfm.elidedText(meta, Qt::ElideRight, r.width()));
            }
        };
    }

    QueryHistoryTab::QueryHistoryTab(QWidget *parent)
        : QWidget(parent)
    {
        // ── Filter bar ──────────────────────────────────────────────────────
        _modeCombo = new QComboBox;
        _modeCombo->addItems({ tr("Filters"), tr("Search") });
        _modeCombo->setToolTip(tr("Switch between field filters and a search box"));

        // Fields mode
        auto *fields = new QWidget;
        _search = new QLineEdit;
        _search->setPlaceholderText(tr("Search query…"));
        _search->setClearButtonEnabled(true);
        _connCombo = new QComboBox;
        _dbCombo   = new QComboBox;
        _collCombo = new QComboBox;
        _typeCombo = new QComboBox;
        _typeCombo->addItems({ tr("All types"), "find", "aggregate", "write", "script", "other" });
        _pinnedOnly = new QCheckBox(tr("Pinned"));
        auto *fl = new QHBoxLayout(fields);
        fl->setContentsMargins(0, 0, 0, 0);
        fl->addWidget(_search, 1);
        fl->addWidget(_connCombo);
        fl->addWidget(_dbCombo);
        fl->addWidget(_collCombo);
        fl->addWidget(_typeCombo);
        fl->addWidget(_pinnedOnly);

        // Search (tokenized) mode
        auto *searchPage = new QWidget;
        _tokenSearch = new QLineEdit;
        _tokenSearch->setClearButtonEnabled(true);
        _tokenSearch->setPlaceholderText(
            tr("e.g.  conn:localhost  db:teaerp  coll:user  kind:aggregate  $exists"));
        auto *sl = new QHBoxLayout(searchPage);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->addWidget(_tokenSearch, 1);

        _filterStack = new QStackedWidget;
        _filterStack->addWidget(fields);
        _filterStack->addWidget(searchPage);

        _sortCombo = new QComboBox;
        _sortCombo->addItems({ tr("Recent"), tr("Most used") });

        auto *clearBtn = new QPushButton(tr("Clear history"));

        auto *bar = new QHBoxLayout;
        bar->setContentsMargins(0, 0, 0, 0);
        bar->addWidget(_modeCombo);
        bar->addWidget(_filterStack, 1);
        bar->addWidget(new QLabel(tr("Sort:")));
        bar->addWidget(_sortCombo);
        bar->addWidget(clearBtn);

        // ── Master / detail ─────────────────────────────────────────────────
        _list = new QListWidget;
        // No zebra striping (the delegate draws thin row separators instead).
        _list->setUniformItemSizes(true);
        _list->setWordWrap(false);
        _list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        _list->setContextMenuPolicy(Qt::CustomContextMenu);
        _list->setItemDelegate(new RowDelegate(_list));

        _preview = new DocutazScintilla(this);
        _preview->setLexer(new JSLexer(_preview));
        _preview->setReadOnly(true);
        _preview->setUtf8(true);
        _preview->setWrapMode(QsciScintilla::WrapWord);

        auto *detail = new QWidget;
        _openBtn = new QPushButton(tr("Open in New Tab"));
        _copyBtn = new QPushButton(tr("Copy"));
        _pinBtn  = new QPushButton(tr("Pin"));
        _delBtn  = new QPushButton(tr("Delete"));
        _status  = new QLabel;
        _status->setWordWrap(true);
        auto *actions = new QHBoxLayout;
        actions->setContentsMargins(0, 0, 0, 0);
        actions->addWidget(_openBtn);
        actions->addWidget(_copyBtn);
        actions->addWidget(_pinBtn);
        actions->addWidget(_delBtn);
        actions->addStretch(1);
        auto *dl = new QVBoxLayout(detail);
        dl->setContentsMargins(0, 0, 0, 0);
        dl->addWidget(_preview, 1);
        dl->addLayout(actions);
        dl->addWidget(_status);

        auto *split = new QSplitter(Qt::Horizontal);
        split->addWidget(_list);
        split->addWidget(detail);
        split->setStretchFactor(0, 2);
        split->setStretchFactor(1, 3);

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(6, 6, 6, 6);
        root->addLayout(bar);
        root->addWidget(split, 1);

        // ── Wiring ──────────────────────────────────────────────────────────
        connect(_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &QueryHistoryTab::toggleFilterMode);
        connect(_search,      &QLineEdit::textChanged, this, &QueryHistoryTab::rebuild);
        connect(_tokenSearch, &QLineEdit::textChanged, this, &QueryHistoryTab::rebuild);
        for (QComboBox *c : { _connCombo, _dbCombo, _collCombo, _typeCombo, _sortCombo })
            connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &QueryHistoryTab::rebuild);
        connect(_pinnedOnly, &QCheckBox::toggled, this, &QueryHistoryTab::rebuild);

        connect(_list, &QListWidget::currentRowChanged, this, &QueryHistoryTab::onSelectionChanged);
        connect(_list, &QListWidget::itemActivated, this, &QueryHistoryTab::onActivated);
        connect(_list, &QListWidget::customContextMenuRequested, this, &QueryHistoryTab::showListMenu);

        connect(_openBtn, &QPushButton::clicked, this, &QueryHistoryTab::openSelected);
        connect(_copyBtn, &QPushButton::clicked, this, &QueryHistoryTab::copySelected);
        connect(_pinBtn,  &QPushButton::clicked, this, &QueryHistoryTab::pinSelected);
        connect(_delBtn,  &QPushButton::clicked, this, &QueryHistoryTab::deleteSelected);
        connect(clearBtn, &QPushButton::clicked, this, &QueryHistoryTab::clearAll);

        connect(&QueryHistoryManager::instance(), &QueryHistoryManager::changed,
                this, &QueryHistoryTab::refreshFilterValues);
        connect(&QueryHistoryManager::instance(), &QueryHistoryManager::changed,
                this, &QueryHistoryTab::rebuild);

        refreshFilterValues();
        rebuild();
        setActionsEnabled(false);
    }

    void QueryHistoryTab::toggleFilterMode(int index)
    {
        _filterStack->setCurrentIndex(index);
        rebuild();
    }

    void QueryHistoryTab::refreshFilterValues()
    {
        const auto &entries = QueryHistoryManager::instance().entries();
        QStringList conns, dbs, colls;
        for (const QueryHistoryEntry &e : entries) {
            if (!e.connection.isEmpty() && !conns.contains(e.connection)) conns << e.connection;
            if (!e.database.isEmpty()   && !dbs.contains(e.database))     dbs   << e.database;
            for (const QString &c : QueryHistoryManager::collectionsOf(e.query))
                if (!colls.contains(c)) colls << c;
        }
        conns.sort(Qt::CaseInsensitive);
        dbs.sort(Qt::CaseInsensitive);
        colls.sort(Qt::CaseInsensitive);

        const auto fill = [](QComboBox *c, const QString &allLabel, const QStringList &values) {
            const QSignalBlocker block(c);
            const QString prev = c->currentIndex() > 0 ? c->currentText() : QString();
            c->clear();
            c->addItem(allLabel);
            c->addItems(values);
            const int i = prev.isEmpty() ? 0 : c->findText(prev);
            c->setCurrentIndex(i < 0 ? 0 : i);
        };
        fill(_connCombo, tr("All connections"), conns);
        fill(_dbCombo,   tr("All databases"),   dbs);
        fill(_collCombo, tr("All collections"), colls);
    }

    bool QueryHistoryTab::entryMatches(int entryIndex) const
    {
        const auto &entries = QueryHistoryManager::instance().entries();
        if (entryIndex < 0 || entryIndex >= entries.size())
            return false;
        const QueryHistoryEntry &e = entries.at(entryIndex);

        if (_modeCombo->currentIndex() == 1)                       // tokenized search
            return matchesTokens(e, _tokenSearch->text().trimmed());

        const QString text = _search->text().trimmed();           // field filters
        if (!text.isEmpty()) {
            const bool inAny =
                e.query.contains(text, Qt::CaseInsensitive)
                || e.connection.contains(text, Qt::CaseInsensitive)
                || e.database.contains(text, Qt::CaseInsensitive)
                || QueryHistoryManager::collectionsOf(e.query).join(' ').contains(text, Qt::CaseInsensitive);
            if (!inAny) return false;
        }
        if (_connCombo->currentIndex() > 0 && e.connection != _connCombo->currentText()) return false;
        if (_dbCombo->currentIndex()   > 0 && e.database   != _dbCombo->currentText())   return false;
        if (_collCombo->currentIndex() > 0
            && !QueryHistoryManager::collectionsOf(e.query).contains(_collCombo->currentText())) return false;
        if (_typeCombo->currentIndex() > 0 && typeInfo(e).label != _typeCombo->currentText()) return false;
        if (_pinnedOnly->isChecked() && !e.pinned) return false;
        return true;
    }

    void QueryHistoryTab::rebuild()
    {
        const int keepIndex = selectedEntryIndex();   // preserve selection across rebuilds
        const auto &entries = QueryHistoryManager::instance().entries();

        QList<int> rows;
        for (int i = 0; i < entries.size(); ++i)
            if (entryMatches(i))
                rows << i;

        if (_sortCombo->currentIndex() == 1)          // Most used
            std::stable_sort(rows.begin(), rows.end(), [&](int a, int b) {
                return entries.at(a).runCount > entries.at(b).runCount;
            });
        // else "Recent": entries are already newest-first.

        const QSignalBlocker block(_list);
        _list->clear();
        int rowToSelect = -1;
        for (int r = 0; r < rows.size(); ++r) {
            const int i = rows.at(r);
            const QueryHistoryEntry &e = entries.at(i);
            const TypeInfo ti = typeInfo(e);

            QString meta = e.timestamp.toString("MMM d HH:mm");
            if (!e.connection.isEmpty()) meta += " · " + e.connection;
            if (!e.database.isEmpty())   meta += "/" + e.database;
            // The op-type is shown as a colour chip (see the delegate), not text.
            // Result count intentionally omitted: the engine only knows the first
            // page (batch size), not the total matched, so showing it would be
            // misleading. Re-run the query to see the live count.
            if (e.runCount > 1)
                meta += QString(" · ×%1").arg(e.runCount);

            auto *item = new QListWidgetItem(e.query.simplified());
            item->setToolTip(e.query);
            item->setData(RoleIndex, i);
            item->setData(RoleMeta, meta);
            item->setData(RolePinned, e.pinned);
            item->setData(RoleTypeColor, ti.color);
            item->setData(RoleTypeLabel, ti.label);
            _list->addItem(item);
            if (i == keepIndex) rowToSelect = r;
        }

        if (rowToSelect < 0 && _list->count() > 0) rowToSelect = 0;
        if (rowToSelect >= 0) _list->setCurrentRow(rowToSelect);

        if (_status)
            _status->setText(rows.isEmpty() && !entries.isEmpty()
                ? tr("No queries match the filter.") : QString());
        onSelectionChanged();
    }

    int QueryHistoryTab::selectedEntryIndex() const
    {
        QListWidgetItem *item = _list->currentItem();
        return item ? item->data(RoleIndex).toInt() : -1;
    }

    void QueryHistoryTab::onSelectionChanged()
    {
        const int i = selectedEntryIndex();
        const auto &entries = QueryHistoryManager::instance().entries();
        if (i < 0 || i >= entries.size()) {
            _preview->setReadOnly(false);
            _preview->clear();
            _preview->setReadOnly(true);
            setActionsEnabled(false);
            return;
        }
        const QueryHistoryEntry &e = entries.at(i);
        _preview->setReadOnly(false);
        _preview->setText(e.query);
        _preview->setReadOnly(true);
        setActionsEnabled(true);
        _pinBtn->setText(e.pinned ? tr("Unpin") : tr("Pin"));
    }

    void QueryHistoryTab::setActionsEnabled(bool on)
    {
        _openBtn->setEnabled(on);
        _copyBtn->setEnabled(on);
        _pinBtn->setEnabled(on);
        _delBtn->setEnabled(on);
    }

    void QueryHistoryTab::notify(const QString &text)
    {
        if (_status) _status->setText(text);
    }

    void QueryHistoryTab::onActivated(QListWidgetItem *)
    {
        openSelected();
    }

    void QueryHistoryTab::openSelected()
    {
        const int i = selectedEntryIndex();
        const auto &entries = QueryHistoryManager::instance().entries();
        if (i < 0 || i >= entries.size())
            return;
        const QueryHistoryEntry e = entries.at(i);   // copy: openShell may fire events

        MongoServer *target = nullptr;
        for (auto const &srv : AppRegistry::instance().app()->getServers()) {
            if (srv->connectionRecord()
                && QtUtils::toQString(srv->connectionRecord()->connectionName()) == e.connection) {
                target = srv.get();
                break;
            }
        }
        if (target) {
            AppRegistry::instance().app()->openShell(
                target, e.query, QtUtils::toStdString(e.database), false);
            notify(tr("Opened in a new tab."));
        } else {
            QApplication::clipboard()->setText(e.query);
            notify(tr("Connection \"%1\" isn't open — query copied to the clipboard.")
                       .arg(e.connection.isEmpty() ? tr("(unknown)") : e.connection));
        }
    }

    void QueryHistoryTab::copySelected()
    {
        const int i = selectedEntryIndex();
        const auto &entries = QueryHistoryManager::instance().entries();
        if (i < 0 || i >= entries.size())
            return;
        QApplication::clipboard()->setText(entries.at(i).query);
        notify(tr("Query copied to the clipboard."));
    }

    void QueryHistoryTab::pinSelected()
    {
        const int i = selectedEntryIndex();
        const auto &entries = QueryHistoryManager::instance().entries();
        if (i < 0 || i >= entries.size())
            return;
        QueryHistoryManager::instance().setPinned(i, !entries.at(i).pinned);
    }

    void QueryHistoryTab::deleteSelected()
    {
        const int i = selectedEntryIndex();
        if (i >= 0)
            QueryHistoryManager::instance().remove(i);
    }

    void QueryHistoryTab::showListMenu(const QPoint &pos)
    {
        QListWidgetItem *item = _list->itemAt(pos);
        if (!item)
            return;
        _list->setCurrentItem(item);

        QMenu menu(this);
        QAction *openAct = menu.addAction(tr("Open in New Tab"));
        QAction *copyAct = menu.addAction(tr("Copy"));
        menu.addSeparator();
        const int i = selectedEntryIndex();
        const auto &entries = QueryHistoryManager::instance().entries();
        const bool pinned = (i >= 0 && i < entries.size()) && entries.at(i).pinned;
        QAction *pinAct = menu.addAction(pinned ? tr("Unpin") : tr("Pin"));
        QAction *delAct = menu.addAction(tr("Delete"));

        QAction *chosen = menu.exec(_list->viewport()->mapToGlobal(pos));
        if      (chosen == openAct) openSelected();
        else if (chosen == copyAct) copySelected();
        else if (chosen == pinAct)  pinSelected();
        else if (chosen == delAct)  deleteSelected();
    }

    void QueryHistoryTab::clearAll()
    {
        if (QueryHistoryManager::instance().entries().isEmpty())
            return;
        if (QMessageBox::question(this, tr("Clear Query History"),
                                  tr("Remove all saved query history?"))
            == QMessageBox::Yes)
            QueryHistoryManager::instance().clear();
    }
}
