#include "docutaz/gui/dialogs/ConnectionsDialog.h"

#include <memory>

#include <QPushButton>
#include <QToolButton>
#include <QHBoxLayout>
#include <QAction>
#include <QMessageBox>
#include <QLabel>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QTreeWidgetItem>
#include <QKeyEvent>
#include <QApplication>
#include <QSettings>
#include <QUuid>
#include <QPainter>
#include <QPixmap>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/settings/ReplicaSetSettings.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/settings/SslSettings.h"
#include "docutaz/core/settings/SshSettings.h"
#include "docutaz/core/settings/CredentialSettings.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/GlyphIcons.h"
#include "docutaz/gui/utils/DialogUtils.h"
#include "docutaz/gui/Theme.h"
#include "docutaz/gui/ConnectionEnvironment.h"
#include "docutaz/gui/dialogs/ConnectionDialog.h"
#include "docutaz/gui/MainWindow.h"
#include "docutaz/gui/widgets/workarea/WelcomeTab.h"
#include "docutaz/utils/common.h"

namespace Docutaz
{
    namespace
    {
        // Compose the row glyph with the connection's environment colour dot to
        // its right (prod=red / staging=amber / dev=green …), matching the
        // connection-list mockup. Bakes a Normal and a Selected (white glyph)
        // variant so the glyph stays legible on the green selected row.
        QIcon iconWithEnvironmentDot(const QIcon &base, const QColor &dot)
        {
            const int s = 16, d = 8, gap = 5;
            const int w = s + (dot.isValid() ? gap + d : 0);
            const qreal dpr = qApp->devicePixelRatio();

            auto render = [&](QIcon::Mode mode) {
                QPixmap pm(QSize(w, s) * dpr);
                pm.setDevicePixelRatio(dpr);
                pm.fill(Qt::transparent);
                QPainter p(&pm);
                base.paint(&p, QRect(0, 0, s, s), Qt::AlignCenter, mode);
                if (dot.isValid()) {
                    p.setRenderHint(QPainter::Antialiasing, true);
                    p.setBrush(dot);
                    p.setPen(Qt::NoPen);
                    p.drawEllipse(QRect(s + gap, (s - d) / 2, d, d));
                }
                return pm;
            };

            QIcon ic;
            ic.addPixmap(render(QIcon::Normal), QIcon::Normal);
            ic.addPixmap(render(QIcon::Selected), QIcon::Selected);
            return ic;
        }
    }

    /* ------------------------ ConnectionListWidgetItem ------------------------ */

    /**
     * @brief Simple ListWidgetItem that has several convenience methods.
     */
    class ConnectionListWidgetItem : public QTreeWidgetItem
    {
    public:
        /**
         * @brief Creates ConnectionListWidgetItem with specified ConnectionSettings
         */
        ConnectionListWidgetItem(ConnectionSettings *connection) { setConnection(connection); }

        /**
         * @brief Returns attached ConnectionSettings.
         */
        ConnectionSettings *connection() { return _connection; }

        /**
         * @brief Attach ConnectionSettings to this item
         */
        void setConnection(ConnectionSettings *connection)
        {
            _connection = connection;

            QIcon baseIcon = GuiRegistry::instance().serverIcon();
            if (_connection->isReplicaSet()) {
                baseIcon = GuiRegistry::instance().replicaSetIcon();
                setText(0, QtUtils::toQString(_connection->connectionName()));
                auto const repSetSize = _connection->replicaSetSettings()->members().size();
                auto addrText = QString::number(repSetSize) + ((repSetSize > 1) ? " nodes" : " node");
                if (!_connection->replicaSetSettings()->members().empty()) {
                    addrText += QString::fromStdString(" (" + _connection->replicaSetSettings()->members().front() + ")");
                }
                setText(1, addrText);
            }
            else {
                setText(0, QtUtils::toQString(_connection->connectionName()));
                setText(1, QtUtils::toQString(_connection->getFullAddress()));
            }

            if (_connection->imported())
                baseIcon = GuiRegistry::instance().serverImportedIcon();

            // Environment colour dot on the row glyph (prod/staging/dev/…).
            const QColor envDot = ConnectionEnvironment::color(_connection->environment());
            setIcon(0, iconWithEnvironmentDot(baseIcon, envDot));

            // Header "Attributes" (column[2])
            setText(2, _connection->isReplicaSet() ? "Replica Set" : "");
            
            if (_connection->sslSettings()->sslEnabled())
                setText(2, text(2) + (text(2).isEmpty() ? "TLS" : ", TLS"));

            if (!_connection->isReplicaSet() && _connection->sshSettings()->enabled())
                setText(2, text(2) + (text(2).isEmpty() ? "SSH" : ", SSH"));

            // Header "Auth. Database/User" (column[3])
            if (_connection->hasEnabledPrimaryCredential()) {
                auto primaryCredential { _connection->primaryCredential() };
                auto const authString = 
                    QString("%1 / %2").arg(QtUtils::toQString(primaryCredential->databaseName()))
                                      .arg(QtUtils::toQString(primaryCredential->userName()));
                setText(3, authString + "    ");
                setIcon(3, GuiRegistry::instance().keyIcon());
            }
            else {
                setIcon(3, QIcon());
                setText(3, "");
            }

        }

    private:
        ConnectionSettings *_connection;
    };



    /* ------------------------ ConnectionsDialog ------------------------ */

    /**
     * @brief Creates dialog
     */
    ConnectionsDialog::ConnectionsDialog(SettingsManager *settingsManager, bool checkForImported, QWidget *parent)
        : QDialog(parent), _settingsManager(settingsManager), _checkForImported(checkForImported)
    {
        setWindowIcon(GuiRegistry::instance().connectIcon());
        setWindowTitle("MongoDB Connections");

        // Remove help button (?)
        setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);

        QAction *addAction = new QAction("&Add...", this);
        VERIFY(connect(addAction, SIGNAL(triggered()), this, SLOT(add())));

        QAction *editAction = new QAction("&Edit...", this);
        VERIFY(connect(editAction, SIGNAL(triggered()), this, SLOT(edit())));

        QAction *cloneAction = new QAction("&Clone...", this);
        VERIFY(connect(cloneAction, SIGNAL(triggered()), this, SLOT(clone())));

        QAction *removeAction = new QAction("&Remove...", this);
        VERIFY(connect(removeAction, SIGNAL(triggered()), this, SLOT(remove())));

        _listWidget = new ConnectionsTreeWidget;
        // Framed list to match the mockup's .clist (1px border, rounded, base bg).
        _listWidget->setStyleSheet(QString(
            "QTreeWidget { background: %1; border: 1px solid %2; border-radius: 8px; }")
            .arg(Theme::current().base.name(), Theme::current().mid.name()));
        GuiRegistry::instance().setAlternatingColor(_listWidget);
#if defined(Q_OS_MAC)
        _listWidget->setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
        _listWidget->setIndentation(5);
        // Wide enough for the composite "glyph + environment dot" icon so it is
        // not shrunk to the default 16px slot.
        _listWidget->setIconSize(QSize(30, 18));

        QStringList colums;
        colums << "Name" << "Address" << "Attributes" << "Auth. Database / User";
        _listWidget->setHeaderLabels(colums);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        _listWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        _listWidget->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        _listWidget->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        _listWidget->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
#endif
        //_listWidget->setViewMode(QListView::ListMode);
        _listWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
        _listWidget->addAction(addAction);
        _listWidget->addAction(editAction);
        _listWidget->addAction(cloneAction);
        _listWidget->addAction(removeAction);
        _listWidget->setSelectionMode(QAbstractItemView::SingleSelection); // single item can be draged or droped
        _listWidget->setDragEnabled(true);
        _listWidget->setDragDropMode(QAbstractItemView::InternalMove);
        _listWidget->setMinimumHeight(290);
        _listWidget->setMinimumWidth(630);
        VERIFY(connect(_listWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(accept())));
        VERIFY(connect(_listWidget, SIGNAL(layoutChanged()), this, SLOT(listWidget_layoutChanged())));

        QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Save);
        buttonBox->button(QDialogButtonBox::Save)->setIcon(GuiRegistry::instance().connectIcon());
        buttonBox->button(QDialogButtonBox::Save)->setText("C&onnect");
        Theme::markPrimary(buttonBox->button(QDialogButtonBox::Save));  // brand-green action button
        VERIFY(connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept())));
        VERIFY(connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject())));

        QHBoxLayout *bottomLayout = new QHBoxLayout;

        // Information message is shown when connection
        // settings are imported from previous version of Robomongo
        int importedCount = _settingsManager->importedConnectionsCount();
        if (_checkForImported && importedCount > 0) {
            QIcon importIcon = qApp->style()->standardIcon(QStyle::SP_MessageBoxInformation);
            QPixmap importPixmap = importIcon.pixmap(20, 20);
            QLabel *importLabelIcon = new QLabel;
            importLabelIcon->setPixmap(importPixmap);
            QString importedRecords = importedCount > 1 ? "records" : "record";
            QLabel *importLabelMessage = new QLabel(QString(
                "<span style='color: %3;'>"
                "Connection settings have been imported (%1 %2)"
                "</span>").arg(importedCount).arg(importedRecords).arg(Theme::current().muted.name()));

            bottomLayout->addWidget(importLabelIcon, 0, Qt::AlignLeft);
            bottomLayout->addWidget(importLabelMessage, 1, Qt::AlignLeft);
        }

        bottomLayout->addWidget(buttonBox, 0, Qt::AlignRight);

        // Prominent action buttons (New / Edit / Clone / Remove) instead of the
        // easy-to-miss text links — flat glyph buttons wired to the same slots.
        auto makeActionBtn = [this](const QString &text, const QString &glyph, const char *slot) {
            auto *b = new QToolButton;
            b->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            b->setText(text);
            b->setIcon(GlyphIcons::icon(glyph, Theme::current().text, Theme::current().highlightedText));
            b->setAutoRaise(true);
            b->setCursor(Qt::PointingHandCursor);
            VERIFY(connect(b, SIGNAL(clicked()), this, slot));
            return b;
        };

        auto *actionsRow = new QHBoxLayout;
        actionsRow->setSpacing(4);
        actionsRow->addWidget(makeActionBtn("New",    "plus",  SLOT(add())));
        actionsRow->addWidget(makeActionBtn("Edit",   "edit",  SLOT(edit())));
        actionsRow->addWidget(makeActionBtn("Clone",  "copy",  SLOT(clone())));
        actionsRow->addWidget(makeActionBtn("Remove", "trash", SLOT(remove())));
        actionsRow->addStretch(1);
        auto *dragHint = new QLabel("or reorder via drag'n'drop");
        dragHint->setStyleSheet(QString("color:%1;").arg(Theme::current().muted.name()));
        actionsRow->addWidget(dragHint, 0, Qt::AlignVCenter);

        QVBoxLayout *firstColumnLayout = new QVBoxLayout;
        firstColumnLayout->setSpacing(12);
        firstColumnLayout->addLayout(actionsRow);
        firstColumnLayout->addWidget(_listWidget, 1);
        firstColumnLayout->addLayout(bottomLayout);

        QHBoxLayout *mainLayout = new QHBoxLayout(this);
        mainLayout->setContentsMargins(16, 16, 16, 16);
        mainLayout->addLayout(firstColumnLayout, 1);

        // Populate list with connections
        std::vector<ConnectionSettings*> connectionSettings = _settingsManager->connections();
        for (auto const& connSetting : connectionSettings) {
            ConnectionSettings *connectionModel { connSetting };
            add(connectionModel);
        }

        // Highlight last item
        if (_listWidget->topLevelItemCount() > 0)
            _listWidget->setCurrentItem(_listWidget->topLevelItem(_listWidget->topLevelItemCount()-1));

        _listWidget->setFocus();
        resize(getSetting("ConnectionsDialog/size").toSize());
    }

    /**
     * @brief This function is called when user clicks on "Connect" button.
     */
    void ConnectionsDialog::accept()
    {
        auto currentItem = dynamic_cast<ConnectionListWidgetItem*>(_listWidget->currentItem());

        // Do nothing if no item selected
        if (!currentItem)
            return;

        _selectedConnection = currentItem->connection();

        QDialog::accept();
    }

    void ConnectionsDialog::reject()
    {
        QDialog::reject();
    }

    ConnectionsDialog::~ConnectionsDialog() 
    {
        saveSetting("ConnectionsDialog/size", size());
    }

    void ConnectionsDialog::linkActivated(const QString &link)
    {
        if (link == "create")
            add();
        else if (link == "edit")
            edit();
        else if (link == "remove")
            remove();
        else if (link == "clone")
            clone();
    }

    /**
     * @brief Initiate 'add' action, usually when user clicked on Add button
     */
    void ConnectionsDialog::add()
    {       
        auto newConnSettings = std::unique_ptr<ConnectionSettings>(new ConnectionSettings(false));
        ConnectionDialog editDialog(newConnSettings.get(), this);

        // Do nothing if not accepted
        if (editDialog.exec() != QDialog::Accepted) {
            return;
        }

        add(newConnSettings.get());
        _settingsManager->addConnection(newConnSettings.release());
        
        _listWidget->setFocus();
    }

    /**
     * @brief Initiate 'edit' action, usually when user clicked on Edit button
     */
    void ConnectionsDialog::edit()
    {
        auto currentItem = dynamic_cast<ConnectionListWidgetItem*>(_listWidget->currentItem());

        // Do nothing if no item selected
        if (!currentItem)
            return;

        auto connection = currentItem->connection();
        std::unique_ptr<ConnectionSettings> clonedConnection(connection->clone());
        ConnectionDialog editDialog(clonedConnection.get(), this);

        // Do nothing if not accepted
        if (editDialog.exec() != QDialog::Accepted) {
            // on linux focus is lost - we need to activate connections dialog
            activateWindow();
            return;
        }

        connection->apply(editDialog.connection());       

        // on linux focus is lost - we need to activate connections dialog
        activateWindow();

        int size = _connectionItems.size();
        for (int i = 0; i<size; ++i)
        {
            ConnectionListWidgetItem *item = _connectionItems[i];
            if (_connectionItems[i]->connection() == connection) {
                item->setConnection(connection);
                break;
            }
        }        
    }

    /**
     * @brief Initiate 'remove' action, usually when user clicked on Remove button
     */
    void ConnectionsDialog::remove()
    {
        auto currentItem = dynamic_cast<ConnectionListWidgetItem*>(_listWidget->currentItem());

        // Do nothing if no item selected
        if (!currentItem)
            return;

        ConnectionSettings *connSettings = currentItem->connection();

        // Ask user
        QString const question { "Are you sure you want to remove the <b>%1</b> connection?" };
        if (!utils::destructiveConfirm(this, "Remove Connection",
                question.arg(QtUtils::toQString(connSettings->getReadableName())), "Remove"))
            return;

        /* Temporarily disabling Recent Connections feature
        _settingsManager->deleteRecentConnection(connSettings);
        // Remove from WelcomeTab
        for (auto widget : QApplication::topLevelWidgets()) {
            if (auto mainWin = dynamic_cast<MainWindow*>(widget))
                mainWin->getWelcomeTab()->removeRecentConnectionItem(connSettings);
        }
        */

        _settingsManager->removeConnection(connSettings);

        delete currentItem;
    }

    void ConnectionsDialog::clone()
    {
        auto currentItem = dynamic_cast<ConnectionListWidgetItem*>(_listWidget->currentItem());

        // Do nothing if no item selected
        if (!currentItem)
            return;

        // Clone connection
        ConnectionSettings *connection = currentItem->connection()->clone();
        // This is a special clone which will actually be a new connection and must have unique UUID
        connection->setUuid(QUuid::createUuid().toString());    
        std::string newConnectionName = "Copy of " + connection->connectionName();

        connection->setConnectionName(newConnectionName);
        connection->replicaSetSettings()->setCachedSetName("");

        ConnectionDialog editDialog(connection, this);

        // Cleanup newly created connection and return, if not accepted.
        if (editDialog.exec() != QDialog::Accepted) {
            delete connection;
            return;
        }

        // Now connection will be owned by SettingsManager
        _settingsManager->addConnection(connection);
        add(connection);
    }

    /**
     * @brief Handles ListWidget layoutChanged() signal
     */
    void ConnectionsDialog::listWidget_layoutChanged()
    {
        // Make childrens toplevel again. This is a bad, but quickiest item reordering
        // implementation.
        for (int i = 0; i < _listWidget->topLevelItemCount(); i++)
        {
            auto item = (ConnectionListWidgetItem *) _listWidget->topLevelItem(i);
            if (item->childCount() > 0) {
                auto childItem = (ConnectionListWidgetItem *) item->child(0);
                item->removeChild(childItem);
                _listWidget->insertTopLevelItem(++i, childItem);
                _listWidget->setCurrentItem(childItem);
                break;
            }
        }

        SettingsManager::ConnectionSettingsContainerType items;
        for (int i = 0; i < _listWidget->topLevelItemCount(); i++)
        {
            auto item = (ConnectionListWidgetItem *) _listWidget->topLevelItem(i);
            items.push_back(item->connection());
        }

        _settingsManager->reorderConnections(items);
    }

    /**
     * @brief Add connection to the list widget
     */
    void ConnectionsDialog::add(ConnectionSettings *connection)
    {
        auto item = new ConnectionListWidgetItem(connection);
        _listWidget->addTopLevelItem(item);
        _listWidget->setCurrentItem(item);
        _connectionItems.push_back(item);
    }

    void ConnectionsDialog::keyPressEvent(QKeyEvent *event) {

        if (event->key() == Qt::Key_E && (event->modifiers() & Qt::ControlModifier)) {
            edit();
            return;
        }

        if (event->key() == Qt::Key_W && (event->modifiers() & Qt::ControlModifier)) {
            close();
            return;
        }

        // Shift + Return also accepts connection (this shortcut is handled
        // to support DEBUG level logging)
        if (event->key() == Qt::Key_Return && (event->modifiers() & Qt::ShiftModifier)) {
            accept();
            return;
        }

        QDialog::keyPressEvent(event);
    }

    ConnectionsTreeWidget::ConnectionsTreeWidget()
    {
        setDragDropMode(QAbstractItemView::InternalMove);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setDragEnabled(true);
        setAcceptDrops(true);
    }

    void ConnectionsTreeWidget::dropEvent(QDropEvent *event)
    {
#ifdef __APPLE__
        if(_dragDropCount > 0)
            return;
#endif
        QTreeWidget::dropEvent(event);
        emit layoutChanged();
#ifdef __APPLE__
        ++_dragDropCount;
#endif
    }
}