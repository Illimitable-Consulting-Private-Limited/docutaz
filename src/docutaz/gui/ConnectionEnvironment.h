#pragma once

#include <QColor>
#include <QString>
#include <string>
#include <vector>

// Maps a ConnectionSettings::environment() key to a display name and an accent
// colour, used to colour-code a connection across the UI (explorer row, shell
// tab, an accent strip above the editor) as a "don't run a delete on prod"
// safeguard. Header-only so the dialog, explorer and work-area can all share it.

namespace Docutaz
{
    namespace ConnectionEnvironment
    {
        struct Preset {
            const char *key;     // stored in ConnectionSettings::environment()
            const char *name;    // shown in the dropdown / accent label
            QColor      color;   // invalid (default QColor) == no tint
        };

        // First entry ("None") is the default / no-tint option.
        inline const std::vector<Preset> &presets()
        {
            static const std::vector<Preset> kPresets = {
                { "",            "None",        QColor()              },
                { "production",  "Production",  QColor(0xC0, 0x39, 0x2B) }, // red
                { "staging",     "Staging",     QColor(0xE6, 0x7E, 0x22) }, // amber
                { "development", "Development", QColor(0x27, 0xAE, 0x60) }, // green
                { "other",       "Other",       QColor(0x29, 0x80, 0xB9) }, // blue
            };
            return kPresets;
        }

        // Accent colour for a key; invalid QColor when none/unknown (= no tint).
        inline QColor color(const std::string &key)
        {
            for (const auto &p : presets())
                if (key == p.key)
                    return p.color;
            return QColor();
        }

        inline QString displayName(const std::string &key)
        {
            for (const auto &p : presets())
                if (key == p.key)
                    return QString::fromLatin1(p.name);
            return QStringLiteral("None");
        }
    }
}
