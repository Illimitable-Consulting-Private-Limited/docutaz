#include "docutaz/gui/widgets/explorer/ExplorerUserTreeItem.h"

#include <QAction>
#include <QMenu>

#include "docutaz/gui/dialogs/CreateUserDialog.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/utils/DialogUtils.h"

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/domain/MongoDatabase.h"
#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/utils/QtUtils.h"

namespace Docutaz
{
    ExplorerUserTreeItem::ExplorerUserTreeItem(
        QTreeWidgetItem *parent, MongoDatabase *const database, const MongoUser &user) 
        : BaseClass(parent), _user(user), _database(database)
    {
        auto const dropUser { new QAction("Drop User", this) };
        VERIFY(connect(dropUser, SIGNAL(triggered()), SLOT(ui_dropUser())));

        auto const viewUser { new QAction("View User", this) };
        VERIFY(connect(viewUser, SIGNAL(triggered()), SLOT(ui_viewUser())));

        BaseClass::_contextMenu->addAction(viewUser);
        BaseClass::_contextMenu->addAction(dropUser);

        setText(0, QtUtils::toQString(_user.name()));
        setIcon(0, GuiRegistry::instance().userIcon());
        setExpanded(false);
    }

    void ExplorerUserTreeItem::ui_dropUser()
    {
        // Ask user
        int const answer {
            utils::questionDialog(
                treeWidget(), "Drop", "User", QtUtils::toQString(_user.name())
            )
        };

        if (answer == QMessageBox::Yes)
            _database->dropUser(_user.name());
    }

    void ExplorerUserTreeItem::ui_viewUser()
    {
        auto const& app { Docutaz::AppRegistry::instance().app() };
        app->openShell(
            _database, QString::fromStdString("db.getUser(\"" + _user.name() + "\")"), true,
            QtUtils::toQString(_database->name()), CursorPosition()
        );
    }
}
