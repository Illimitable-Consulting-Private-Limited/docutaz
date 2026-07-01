#include "docutaz/gui/widgets/explorer/ExplorerServerTreeItem.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QBrush>
#include <QPainter>
#include <QPixmap>
#include <QTreeWidget>

#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/domain/MongoDatabase.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/settings/ReplicaSetSettings.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/EventBus.h"
#include "docutaz/core/utils/Logger.h"
#include "docutaz/gui/widgets/explorer/ExplorerDatabaseTreeItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerReplicaSetFolderItem.h"
#include "docutaz/gui/widgets/explorer/ExplorerReplicaSetTreeItem.h"
#include "docutaz/gui/dialogs/CreateDatabaseDialog.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/ConnectionEnvironment.h"


namespace
{
     void openCurrentServerShell(Docutaz::MongoServer *const server, const QString &script, bool execute = true,
                                 const Docutaz::CursorPosition &cursor = Docutaz::CursorPosition())
     {
         QString const connName = Docutaz::QtUtils::toQString(server->connectionRecord()->getReadableName());
         Docutaz::AppRegistry::instance().app()->openShell(server, script, std::string(), execute, connName, cursor);
     }

     // Badge the base server glyph with a solid environment-colour dot in the
     // bottom-left corner. The composited icon stays SQUARE and the same size as
     // sibling rows' icons (a wider dot-before-glyph icon would be scaled down by
     // the view to fit the square decoration slot, shrinking the glyph). A thin
     // ring in the view background colour keeps the dot legible over the glyph.
     // Rendered at the tree's device-pixel ratio to stay crisp on HiDPI.
     QIcon withEnvironmentDot(const QIcon &base, const QColor &color, const QColor &ring,
                              int sz, qreal dpr)
     {
         const qreal d = sz * 9.0 / 16.0;   // badge diameter, scaled with the glyph
         QPixmap canvas(QSize(sz, sz) * dpr);
         canvas.setDevicePixelRatio(dpr);
         canvas.fill(Qt::transparent);
         QPainter p(&canvas);
         p.setRenderHint(QPainter::Antialiasing, true);
         p.drawPixmap(0, 0, base.pixmap(QSize(sz, sz), dpr));
         const QPointF c(d / 2.0, sz - d / 2.0);   // bottom-left corner
         p.setPen(Qt::NoPen);
         p.setBrush(ring);
         p.drawEllipse(c, d / 2.0 + 1.0, d / 2.0 + 1.0);
         p.setBrush(color);
         p.drawEllipse(c, d / 2.0, d / 2.0);
         p.end();
         return QIcon(canvas);
     }
}

namespace Docutaz
{
    ExplorerServerTreeItem::ExplorerServerTreeItem(QTreeWidget *view, MongoServer *const server, ConnectionInfo connInfo)
        : BaseClass(view), _server(server), _bus(AppRegistry::instance().bus()), _replicaSetFolder(nullptr),
        _primaryWasUnreachable(false), _systemFolder(nullptr)
    {
        auto openShellAction = new QAction("Open Shell", this);        
#ifdef __APPLE__
        openShellAction->setIcon(GuiRegistry::instance().mongodbIconForMAC());
#else
        openShellAction->setIcon(GuiRegistry::instance().mongodbIcon());
#endif
        VERIFY(connect(openShellAction, SIGNAL(triggered()), SLOT(ui_openShell())));

        QAction *refreshServer = new QAction("Refresh", this);
        VERIFY(connect(refreshServer, SIGNAL(triggered()), SLOT(ui_refreshServer())));

        QAction *createDatabase = new QAction("Create Database", this);
        VERIFY(connect(createDatabase, SIGNAL(triggered()), SLOT(ui_createDatabase())));

        QAction *serverStatus = new QAction("Server Status", this);
        VERIFY(connect(serverStatus, SIGNAL(triggered()), SLOT(ui_serverStatus())));

        QAction *serverVersion = new QAction("MongoDB Version", this);
        VERIFY(connect(serverVersion, SIGNAL(triggered()), SLOT(ui_serverVersion())));

        QAction *serverHostInfo = new QAction("Host Info", this);
        VERIFY(connect(serverHostInfo, SIGNAL(triggered()), SLOT(ui_serverHostInfo())));

        QAction *showLog = new QAction("Show Log", this);
        VERIFY(connect(showLog, SIGNAL(triggered()), SLOT(ui_showLog())));

        QAction *disconnectAction = new QAction("Disconnect", this);
        disconnectAction->setIconText("Disconnect");
        VERIFY(connect(disconnectAction, SIGNAL(triggered()), SLOT(ui_disconnectServer())));

        BaseClass::_contextMenu->addAction(openShellAction);
        BaseClass::_contextMenu->addAction(refreshServer);
        BaseClass::_contextMenu->addSeparator();
        BaseClass::_contextMenu->addAction(createDatabase);
        BaseClass::_contextMenu->addAction(serverStatus);
        BaseClass::_contextMenu->addAction(serverHostInfo);
        BaseClass::_contextMenu->addAction(serverVersion);
        BaseClass::_contextMenu->addSeparator();
        BaseClass::_contextMenu->addAction(showLog);
        BaseClass::_contextMenu->addAction(disconnectAction);

        _bus->subscribe(this, DatabaseListLoadedEvent::Type, _server);
        _bus->subscribe(this, MongoServerLoadingDatabasesEvent::Type, _server);
        _bus->subscribe(this, ReplicaSetFolderRefreshed::Type, _server);
        _bus->subscribe(this, ConnectionEstablishedEvent::Type, _server);
        _bus->subscribe(this, ConnectionFailedEvent::Type, _server);

        setText(0, buildServerName());
        // Flag the environment (prod/staging/…) with a solid colour dot badge on
        // the server glyph, rather than tinting the whole row: the row text then
        // stays in the normal foreground and the environment can't be confused
        // with the (green) selection highlight. No tag → plain glyph, no dot.
        {
            const QIcon baseIcon = _server->connectionRecord()->isReplicaSet()
                ? GuiRegistry::instance().replicaSetIcon()
                : GuiRegistry::instance().serverIcon();
            const QColor envColor =
                ConnectionEnvironment::color(_server->connectionRecord()->environment());
            const qreal dpr = treeWidget() ? treeWidget()->devicePixelRatioF() : 1.0;
            const QSize isz = treeWidget() ? treeWidget()->iconSize() : QSize();
            const int sz = (isz.isValid() && isz.width() > 0) ? isz.width() : 16;
            const QColor ring = treeWidget() ? treeWidget()->palette().base().color()
                                             : QColor(Qt::white);
            setIcon(0, envColor.isValid() ? withEnvironmentDot(baseIcon, envColor, ring, sz, dpr)
                                          : baseIcon);
        }
        setExpanded(true);
        setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

        if (_server->connectionRecord()->isReplicaSet()) {
            buildReplicaSetFolder(false);
            buildDatabaseItems();
        }
    }

    void ExplorerServerTreeItem::expand()
    {
        if (_server->connectionRecord()->isReplicaSet()) {
            // do nothing since replica set refresh/reload can take very long times.
        }
        else {  // single server
            _server->loadDatabases();
        }
    }

    void ExplorerServerTreeItem::disableSomeContextMenuActions(bool disable)
    {
        if (BaseClass::_contextMenu->actions().size() < 10 || 
            !_server->connectionRecord()->isReplicaSet())
            return;

        // [1]:Refresh and [9]:Disconnect are always enabled
        BaseClass::_contextMenu->actions().at(0)->setDisabled(disable);
        BaseClass::_contextMenu->actions().at(2)->setDisabled(disable);
        BaseClass::_contextMenu->actions().at(3)->setDisabled(disable);
        BaseClass::_contextMenu->actions().at(4)->setDisabled(disable);
        BaseClass::_contextMenu->actions().at(5)->setDisabled(disable);
        BaseClass::_contextMenu->actions().at(6)->setDisabled(disable);
        BaseClass::_contextMenu->actions().at(8)->setDisabled(disable);
    }

    void ExplorerServerTreeItem::databaseRefreshed(const QList<MongoDatabase *> &dbs)
    {
        int count = dbs.count();
        setText(0, buildServerName(&count));

        // Delete system folder and database items
        QtUtils::clearChildItems(this);

        // Add 'System' folder
        QIcon folderIcon = GuiRegistry::instance().folderIcon();
        _systemFolder = new ExplorerTreeItem(this);
        _systemFolder->setIcon(0, folderIcon);
        _systemFolder->setText(0, "System");
        addChild(_systemFolder);

        for (int i = 0; i < dbs.size(); i++)
        {
            MongoDatabase *database = dbs.at(i);

            if (database->isSystem()) {
                auto dbItem = new ExplorerDatabaseTreeItem(_systemFolder, database);
                _systemFolder->addChild(dbItem);
                continue;
            }

            auto dbItem = new ExplorerDatabaseTreeItem(this, database);
            addChild(dbItem);
        }

        // Show 'System' folder only if it has items
        _systemFolder->setHidden(_systemFolder->childCount() == 0);
    }

    void ExplorerServerTreeItem::handle(DatabaseListLoadedEvent *event)
    {
        if (event->isError()) {
            setText(0, buildServerName(0));     // Restore normal name (without "...")

            if (!_server->connectionRecord()->isReplicaSet())
                setExpanded(false);                 // Collapse

            // We should clear all child items, but we are not
            // doing this, because of incorrect "senders" for messages
            // that are going to Worker thread. Yes, some guy specified UI objects
            // as senders. If we delete these UI objects, response will crash the app
            // (because QObject::thread() is used to get thread of the QObject)
            // QtUtils::clearChildItems(this);

            std::stringstream ss;
            ss << "Cannot load list of databases.\n\nError:\n"
                << event->error().errorMessage();

            QMessageBox::information(NULL, "Error", QtUtils::toQString(ss.str()));
            return;
        }

        databaseRefreshed(event->list);
    }

    void ExplorerServerTreeItem::handle(MongoServerLoadingDatabasesEvent *event)
    {
        int count = -1;
        setText(0, buildServerName(&count));
    }

    void ExplorerServerTreeItem::handle(ReplicaSetFolderRefreshed *event)
    {
        buildReplicaSetFolder(event->expanded);    // Rebuild replica set folder in any case

        // ---Primary is unreachable
        if (event->isError()) {
            replicaSetPrimaryUnreachable();
            disableSomeContextMenuActions(true);

            if (event->error().showErrorWindow()) {
                std::string const errorStr = "Replica set's primary is unreachable.\n\nReason:\n"
                                             "Connection failure. " + event->error().errorMessage();
                QMessageBox::critical(nullptr, "Error", QString::fromStdString(errorStr));
            }
            return;
        }

        // --- Primary is reachable
        if (_primaryWasUnreachable)    // Rebuild db items only when primary was unreachable previously.
            _server->loadDatabases();

        replicaSetPrimaryReachable();
    }

    void ExplorerServerTreeItem::handle(ConnectionEstablishedEvent *event)
    {
        if (!_server->connectionRecord()->isReplicaSet() || 
            ConnectionType::ConnectionRefresh != event->connectionType)
            return;

        if (_primaryWasUnreachable)
            replicaSetPrimaryReachable();

        setIcon(0, GuiRegistry::instance().replicaSetIcon());
        buildReplicaSetServerItem();
    }

    void ExplorerServerTreeItem::handle(ConnectionFailedEvent *event)
    {
        if (!_server->connectionRecord()->isReplicaSet() ||
            ConnectionType::ConnectionRefresh != event->connectionType)
            return;

        buildReplicaSetFolder(true);
        replicaSetPrimaryUnreachable();

        QMessageBox::critical(nullptr, "Error", QString::fromStdString(event->message));
    }

    QString ExplorerServerTreeItem::buildServerName(int *count /* = NULL */, bool online /* = true*/)
    {
        QString name = QtUtils::toQString(_server->connectionRecord()->getReadableName());

        if (!count)
            return name;

        if (*count == -1)
            return name + " ...";

        return online ? QString("%1 (%2)").arg(name).arg(*count) 
                      : QString("%1").arg(name) + " [Offline]";
    }

    void ExplorerServerTreeItem::ui_serverHostInfo()
    {
        openCurrentServerShell(_server, "db.hostInfo()");
    }

    void ExplorerServerTreeItem::ui_serverStatus()
    {
        openCurrentServerShell(_server, "db.serverStatus()");
    }

    void ExplorerServerTreeItem::ui_serverVersion()
    {
        openCurrentServerShell(_server, "db.version()");
    }

    void ExplorerServerTreeItem::ui_showLog()
    {
        openCurrentServerShell(_server, "show log");
    }

    void ExplorerServerTreeItem::ui_openShell()
    {
        openCurrentServerShell(_server, "");
    }

    void ExplorerServerTreeItem::ui_disconnectServer()
    {
        auto *view = dynamic_cast<QTreeWidget *>(BaseClass::treeWidget());
        if (!view)
            return;

        int index = view->indexOfTopLevelItem(this);
        if (index == -1)
            return;

        QTreeWidgetItem *removedItem = view->takeTopLevelItem(index);
        if (!removedItem)
            return;

        AppRegistry::instance().app()->closeServer(_server);
        delete removedItem;
    }

    void ExplorerServerTreeItem::ui_refreshServer()
    {
        if (_server->connectionRecord()->isReplicaSet()) {
            int count = -1;
            setText(0, buildServerName(&count));    // Append "..." into root item text
            _server->tryRefreshReplicaSetConnection();
        }
        else {  // single server
            expand();
        }
    }

    void ExplorerServerTreeItem::ui_createDatabase()
    {
        CreateDatabaseDialog dlg(QString::fromStdString(_server->connectionRecord()->getFullAddress()),
                                 QString(), QString(), treeWidget());
        dlg.setOkButtonText("&Create");
        dlg.setInputLabelText("Database Name:");

        if (dlg.exec() == QDialog::Accepted) {
            if (_server->connectionRecord()->isReplicaSet()) {  // Replica Set
                auto newDb = new MongoDatabase(_server, dlg.databaseName().toStdString());
                _server->addDatabase(newDb);
                int dbCount = _server->databases().count();
                setText(0, buildServerName(&dbCount));
                auto dbItem = new ExplorerDatabaseTreeItem(this, newDb);
                addChild(dbItem);
                LOG_MSG("Database \'" + newDb->name() + "\' created.", mongo::logger::LogSeverity::Info());
            }
            else {  // single server
                _server->createDatabase(QtUtils::toStdStringSafe(dlg.databaseName()));
            }
        }
    }
    
    void ExplorerServerTreeItem::buildReplicaSetServerItem()
    {
        // Delete all children (replica set folder, system folder and database items)
        QtUtils::clearChildItems(this);  
        _replicaSetFolder = nullptr;

        buildReplicaSetFolder(false);
        buildDatabaseItems();
    }
    
    void ExplorerServerTreeItem::buildReplicaSetFolder(bool expanded)
    {
        if (_replicaSetFolder) {    // delete and rebuild existing folder child items
            if (_replicaSetFolder->childCount() > 0)
                QtUtils::clearChildItems(_replicaSetFolder);

            _replicaSetFolder->updateText();
        }
        else {                      // build folder from scratch
            _replicaSetFolder = new ExplorerReplicaSetFolderItem(this, _server);
            addChild(_replicaSetFolder);
        }

        // Add replica set members
        bool isPrimary;
        for (auto const& memberAndHealth : _server->replicaSetInfo()->membersAndHealths)
        {
            isPrimary = _server->replicaSetInfo()->primary.toString() == memberAndHealth.first;
            auto const& hostAndPort = mongo::HostAndPort(mongo::StringData(memberAndHealth.first));
            _replicaSetFolder->addChild(new ExplorerReplicaSetTreeItem(_replicaSetFolder, _server, hostAndPort,
                                                                        isPrimary, memberAndHealth.second));
        }

        _replicaSetFolder->setRefreshFlag(false);
        _replicaSetFolder->setExpanded(expanded);
        _replicaSetFolder->setRefreshFlag(true);
    }

    void ExplorerServerTreeItem::buildDatabaseItems()
    {
        int dbCount = _server->databases().count();
        setText(0, buildServerName(&dbCount));

        // Add 'System' folder
        _systemFolder = new ExplorerTreeItem(this);
        _systemFolder->setIcon(0, GuiRegistry::instance().folderIcon());
        _systemFolder->setText(0, "System");
        addChild(_systemFolder);

        for (auto const& database : _server->databases()) {
            if (database->isSystem()) {
                auto dbItem = new ExplorerDatabaseTreeItem(_systemFolder, database);
                _systemFolder->addChild(dbItem);
                continue;
            }

            auto dbItem = new ExplorerDatabaseTreeItem(this, database);
            addChild(dbItem);
        }

        // Show 'System' folder only if it has items
        _systemFolder->setHidden(_systemFolder->childCount() == 0);
    }

    void ExplorerServerTreeItem::replicaSetPrimaryReachable()
    {
        _primaryWasUnreachable = false;
        disableSomeContextMenuActions(false);
        _replicaSetFolder->disableSomeContextMenuActions();
    }

    void ExplorerServerTreeItem::replicaSetPrimaryUnreachable()
    {
        _primaryWasUnreachable = true;
        disableSomeContextMenuActions(true);
        _replicaSetFolder->disableSomeContextMenuActions();

        int dbCount = 0;
        setText(0, buildServerName(&dbCount, false));
        setIcon(0, GuiRegistry::instance().replicaSetOfflineIcon());

        // For system folder and database items - delete children then set disable
        QtUtils::clearChildItems(_systemFolder);
        _systemFolder->setDisabled(true);
        for (int i = 0; i < childCount(); ++i)  {
            auto dbItem = dynamic_cast<ExplorerDatabaseTreeItem*>(child(i));
            if (dbItem) {
                QtUtils::clearChildItems(dbItem);
                dbItem->setDisabled(true);
            }
        }
    }
}
