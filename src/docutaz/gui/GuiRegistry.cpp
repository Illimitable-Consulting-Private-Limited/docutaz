#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/gui/GlyphIcons.h"
#include "docutaz/gui/Theme.h"

#include <QApplication>
#include <QStyle>
#include <QIcon>

namespace Docutaz
{
    namespace
    {
        // Theme-tinted glyph icons. The icon resolves its colour from the active
        // theme at paint time (see GlyphIcons::Tint), so a cached icon follows
        // live light/dark switches; in the Selected mode the engine swaps to the
        // highlighted-text colour for rows painted over the brand highlight.
        QIcon glyph(const QString &name)
        {
            return GlyphIcons::icon(name, GlyphIcons::Tint::Text);
        }

        QIcon glyphMuted(const QString &name)
        {
            return GlyphIcons::icon(name, GlyphIcons::Tint::Muted);
        }

        QIcon glyphTint(const QString &name, GlyphIcons::Tint tint)
        {
            return GlyphIcons::icon(name, tint);
        }
    }

    /**
     * @brief This is a private constructor, because GuiRegistry is a singleton
     */
    GuiRegistry::GuiRegistry()
    {
    }

    /**
     * @brief Functions that provide access to various icons
     */
    void GuiRegistry::setAlternatingColor(QAbstractItemView *view)
    {
        // Zebra striping is intentionally not used in the flat theme; the views
        // keep a single base background. (Kept as a no-op so existing call sites
        // don't need touching.)
        Q_UNUSED(view);
    }

    const QIcon &GuiRegistry::serverIcon() const
    {
        static const QIcon icon = glyph("server");
        return icon;
    }

    const QIcon &GuiRegistry::serverImportedIcon() const
    {
        static const QIcon icon = glyph("server");
        return icon;
    }

    const QIcon &GuiRegistry::serverPrimaryIcon() const
    {
        static const QIcon icon = glyph("server");
        return icon;
    }

    const QIcon &GuiRegistry::serverSecondaryIcon() const
    {
        static const QIcon icon = glyphMuted("server");
        return icon;
    }

    const QIcon &GuiRegistry::replicaSetIcon() const
    {
        static const QIcon icon = glyph("server");
        return icon;
    }

    const QIcon &GuiRegistry::replicaSetOfflineIcon() const
    {
        static const QIcon icon = glyphMuted("server");
        return icon;
    }

    const QIcon &GuiRegistry::openIcon() const
    {
        static const QIcon icon = glyph("open");
        return icon;
    }

    const QIcon &GuiRegistry::saveIcon() const
    {
        static const QIcon icon = glyph("save");
        return icon;
    }

    const QIcon &GuiRegistry::databaseIcon() const
    {
        static const QIcon icon = glyph("db");
        return icon;
    }

    const QIcon &GuiRegistry::collectionIcon() const
    {
        static const QIcon icon = glyph("coll");
        return icon;
    }

    const QIcon &GuiRegistry::indexIcon() const
    {
        static const QIcon icon = glyph("index");
        return icon;
    }

    const QIcon &GuiRegistry::userIcon() const
    {
        static const QIcon icon = glyph("user");
        return icon;
    }

    const QIcon &GuiRegistry::functionIcon() const
    {
        static const QIcon icon = glyph("function");
        return icon;
    }

    const QIcon &GuiRegistry::maximizeIcon() const
    {
        static const QIcon icon = glyph("maximize");
        return icon;
    }

    const QIcon &GuiRegistry::minimizeIcon() const
    {
        static const QIcon icon = glyph("minimize");
        return icon;
    }

    const QIcon &GuiRegistry::undockIcon() const
    {
        static const QIcon icon = glyph("undock");
        return icon;
    }

    const QIcon &GuiRegistry::dockIcon() const
    {
        static const QIcon icon = glyph("dock");
        return icon;
    }

    const QIcon &GuiRegistry::textIcon() const
    {
        static const QIcon icon = glyph("view-text");
        return icon;
    }

    const QIcon &GuiRegistry::textHighlightedIcon() const
    {
        static const QIcon icon = glyphTint("view-text", GlyphIcons::Tint::Highlight);
        return icon;
    }

    const QIcon &GuiRegistry::treeIcon() const
    {
        static const QIcon icon = glyph("view-tree");
        return icon;
    }

    const QIcon &GuiRegistry::treeHighlightedIcon() const
    {
        static const QIcon icon = glyphTint("view-tree", GlyphIcons::Tint::Highlight);
        return icon;
    }

    const QIcon &GuiRegistry::tableIcon() const
    {
        static const QIcon icon = glyph("view-table");
        return icon;
    }

    const QIcon &GuiRegistry::tableHighlightedIcon() const
    {
        static const QIcon icon = glyphTint("view-table", GlyphIcons::Tint::Highlight);
        return icon;
    }

    const QIcon &GuiRegistry::customIcon() const
    {
        static const QIcon icon = glyph("view-custom");
        return icon;
    }

    const QIcon &GuiRegistry::customHighlightedIcon() const
    {
        static const QIcon icon = glyphTint("view-custom", GlyphIcons::Tint::Highlight);
        return icon;
    }

    const QIcon &GuiRegistry::rotateIcon() const
    {
        static const QIcon icon = glyph("refresh");
        return icon;
    }

    const QIcon &GuiRegistry::visualIcon() const
    {
        static const QIcon icon = glyph("view-custom");
        return icon;
    }

    const QIcon &GuiRegistry::circleIcon() const
    {
        static const QIcon icon = glyphMuted("circle");
        return icon;
    }

    const QIcon &GuiRegistry::bsonArrayIcon() const
    {
        static const QIcon icon = glyphMuted("bson-array");
        return icon;
    }


    const QIcon &GuiRegistry::bsonObjectIcon() const
    {
        static const QIcon icon = glyphMuted("bson-object");
        return icon;
    }

    const QIcon &GuiRegistry::bsonStringIcon() const
    {
        static const QIcon icon = glyphMuted("bson-string");
        return icon;
    }

    const QIcon &GuiRegistry::folderIcon() const
    {
        static const QIcon icon = glyph("folder");
        return icon;
    }

    const QIcon &GuiRegistry::bsonIntegerIcon() const
    {
        static const QIcon icon = glyphMuted("bson-number");
        return icon;
    }

    const QIcon &GuiRegistry::bsonDoubleIcon() const
    {
        static const QIcon icon = glyphMuted("bson-number");
        return icon;
    }

	const QIcon &GuiRegistry::bsonNumberDecimalIcon() const
	{
		static const QIcon icon = glyphMuted("bson-number");
		return icon;
	}

    const QIcon &GuiRegistry::bsonDateTimeIcon() const
    {
        static const QIcon icon = glyphMuted("bson-datetime");
        return icon;
    }

    const QIcon &GuiRegistry::bsonBinaryIcon() const
    {
        static const QIcon icon = glyphMuted("bson-binary");
        return icon;
    }

    const QIcon &GuiRegistry::bsonNullIcon() const
    {
        static const QIcon icon = glyphMuted("bson-null");
        return icon;
    }

    const QIcon &GuiRegistry::bsonBooleanIcon() const
    {
        static const QIcon icon = glyphMuted("bson-bool");
        return icon;
    }

    const QIcon &GuiRegistry::noMarkIcon() const
    {
        static const QIcon icon = glyphTint("x", GlyphIcons::Tint::Danger);
        return icon;
    }

    const QIcon &GuiRegistry::yesMarkIcon() const
    {
        static const QIcon icon = glyphTint("check", GlyphIcons::Tint::Highlight);
        return icon;
    }

    const QIcon &GuiRegistry::skipMarkIcon() const
    {
        static const QIcon icon = glyphMuted("dash");
        return icon;
    }

    const QIcon &GuiRegistry::questionMarkIcon() const
    {
        static const QIcon icon = glyphMuted("question");
        return icon;
    }

    const QIcon &GuiRegistry::timeIcon() const
    {
        static const QIcon icon = glyph("history");
        return icon;
    }

    const QIcon &GuiRegistry::keyIcon() const
    {
        static const QIcon icon = glyph("key");
        return icon;
    }

    const QIcon &GuiRegistry::showIcon() const
    {
        static const QIcon icon = glyph("eye");
        return icon;
    }

    const QIcon &GuiRegistry::hideIcon() const
    {
        static const QIcon icon = glyph("eye-off");
        return icon;
    }

    const QIcon &GuiRegistry::plusIcon() const
    {
        static const QIcon icon = glyph("plus");
        return icon;
    }

    const QIcon &GuiRegistry::minusIcon() const
    {
        static const QIcon icon = glyph("minus");
        return icon;
    }

    const QBrush &GuiRegistry::typeBrush() const
    {
        static const QBrush typeBrush = QBrush(QColor(150, 150, 150));
        return typeBrush;
    }

    const QIcon &GuiRegistry::leftIcon() const
    {
        static const QIcon icon = glyph("chev-l");
        return icon;
    }

    const QIcon &GuiRegistry::rightIcon() const
    {
        static const QIcon icon = glyph("chev-r");
        return icon;
    }

    const QIcon &GuiRegistry::mongodbIcon() const
    {
        static const QIcon icon = glyphMuted("doc");
        return icon;
    }

    const QIcon &GuiRegistry::mongodbIconForMAC() const
    {
        static const QIcon icon = glyphMuted("doc");
        return icon;
    }

    const QIcon &GuiRegistry::connectIcon() const
    {
        static const QIcon icon = glyph("connect");
        return icon;
    }

    const QIcon &GuiRegistry::executeIcon() const
    {
        // Monochrome per the mockup; brand green stays reserved for selection.
        static const QIcon icon = glyph("run");
        return icon;
    }

    const QIcon &GuiRegistry::stopIcon() const
    {
        static const QIcon icon = glyph("stop");
        return icon;
    }

    const QIcon &GuiRegistry::exportIcon() const
    {
        static const QIcon icon = glyph("export");
        return icon;
    }

    const QIcon &GuiRegistry::importIcon() const
    {
        static const QIcon icon = glyph("import");
        return icon;
    }

    const QIcon &GuiRegistry::deleteIcon() const
    {
        static const QIcon icon = glyph("trash");
        return icon;
    }

    const QIcon &GuiRegistry::deleteIconRed() const
    {
        static const QIcon icon = glyphTint("trash", GlyphIcons::Tint::Danger);
        return icon;
    }

    const QIcon &GuiRegistry::deleteIconMouseHovered() const
    {
        static const QIcon icon = glyphTint("trash", GlyphIcons::Tint::Danger);
        return icon;
    }

    const QIcon &GuiRegistry::mainWindowIcon() const
    {
        static const QIcon mainWindowIc = [] {
            QIcon icon(":/docutaz/icons/logo-256x256.png");
            // Provide a small-size variant so the launcher/taskbar/title-bar
            // icon stays crisp instead of downscaling the 256px image.
            icon.addFile(":/docutaz/icons/logo-20x20.png");
            return icon;
        }();
        return mainWindowIc;
    }

    const QIcon& GuiRegistry::welcomeTabIcon() const
    {
        static const QIcon icon(":/docutaz/icons/welcome_tab_icon.png");
        return icon;
    }

    const QFont &GuiRegistry::font() const
    {
        // Default the editor and result views to the bundled UI font (Inter) so
        // the whole application shares one typeface; an explicit user-chosen
        // textFontFamily still wins.
        QString family = AppRegistry::instance().settingsManager()->textFontFamily();
        if (family.isEmpty())
            family = Theme::uiFontFamily();

        int pointSize = AppRegistry::instance().settingsManager()->textFontPointSize();
        if (pointSize < 1) {
#if defined(Q_OS_MAC)
            pointSize = 12;
#elif defined(Q_OS_UNIX)
            pointSize = -1;
#elif defined(Q_OS_WIN)
            pointSize = 10;
#endif
        }


        static QFont textFont = QFont(family, pointSize);
        return textFont;
    }
}
