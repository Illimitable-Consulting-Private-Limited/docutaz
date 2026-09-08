#pragma once

#include <QCheckBox>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

#include "docutaz/gui/Theme.h"

namespace Docutaz
{
    // A drop-in replacement for QCheckBox drawn as an iOS-style pill switch.
    //
    // Rationale: a stylesheet'd QCheckBox indicator is easy to lose in the dark
    // palette (the check glyph and the 1px border both sit close to the surface
    // colour). This widget paints the control itself from the Theme tokens, so
    // the on/off state is unmistakable in both the light and dark schemes.
    //
    // It derives from QCheckBox and adds no new API: isChecked()/setChecked(),
    // checkState(), toggled()/stateChanged() and label-click toggling all behave
    // exactly as before, so call sites only swap the class name.
    class ToggleSwitch : public QCheckBox
    {
    public:
        explicit ToggleSwitch(QWidget *parent = nullptr) : QCheckBox(parent) { init(); }
        explicit ToggleSwitch(const QString &text, QWidget *parent = nullptr)
            : QCheckBox(text, parent) { init(); }

        QSize sizeHint() const override
        {
            const QFontMetrics fm(font());
            const int h = qMax(kTrackH + 2, fm.height());
            int w = kTrackW;
            const QString t = text();
            if (!t.isEmpty())
                w += kGap + fm.horizontalAdvance(t);
            return QSize(w, qMax(h, kTrackH + 2));
        }

        QSize minimumSizeHint() const override { return sizeHint(); }

    protected:
        // Toggle when the click lands anywhere on the widget (track or label),
        // matching QCheckBox behaviour.
        bool hitButton(const QPoint &pos) const override { return rect().contains(pos); }

        void paintEvent(QPaintEvent *) override
        {
            const Theme::Tokens &t = Theme::current();
            const bool on = isChecked();
            const bool enabled = isEnabled();

            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing, true);

            const int top = (height() - kTrackH) / 2;
            QRectF track(0.5, top + 0.5, kTrackW - 1.0, kTrackH - 1.0);

            QColor trackColor = on ? t.highlight : t.mid;
            QColor knobColor(255, 255, 255);
            QColor knobBorder = on ? t.highlight.darker(115) : t.muted;
            if (!enabled)
            {
                trackColor.setAlpha(110);
                knobColor.setAlpha(150);
                knobBorder.setAlpha(110);
            }

            // Track.
            p.setPen(Qt::NoPen);
            p.setBrush(trackColor);
            p.drawRoundedRect(track, kTrackH / 2.0, kTrackH / 2.0);

            // Focus ring around the track.
            if (hasFocus() && enabled)
            {
                QPen fp(t.highlight);
                fp.setWidthF(1.5);
                p.setPen(fp);
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(track.adjusted(-1.5, -1.5, 1.5, 1.5),
                                  (kTrackH + 3) / 2.0, (kTrackH + 3) / 2.0);
            }

            // Knob.
            const qreal d = kTrackH - 2 * kKnobInset;
            const qreal kx = on ? (kTrackW - kKnobInset - d) : kKnobInset;
            const QRectF knob(kx, top + kKnobInset, d, d);
            p.setBrush(knobColor);
            QPen kp(knobBorder);
            kp.setWidthF(1.0);
            p.setPen(kp);
            p.drawEllipse(knob);

            // Label.
            const QString label = text();
            if (!label.isEmpty())
            {
                p.setPen(palette().color(enabled ? QPalette::Active : QPalette::Disabled,
                                         QPalette::WindowText));
                const QRect tr(kTrackW + kGap, 0, width() - kTrackW - kGap, height());
                p.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
                           p.fontMetrics().elidedText(label, Qt::ElideRight, tr.width()));
            }
        }

    private:
        static constexpr int kTrackW = 38;
        static constexpr int kTrackH = 20;
        static constexpr int kKnobInset = 2;
        static constexpr int kGap = 8;

        void init()
        {
            setCursor(Qt::PointingHandCursor);
            setAttribute(Qt::WA_Hover, true);
            connect(Theme::Notifier::instance(), &Theme::Notifier::changed,
                    this, [this] { update(); });
        }
    };
}
