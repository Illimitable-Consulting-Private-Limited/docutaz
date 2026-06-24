#pragma once

#include <QComboBox>
#include <QPaintEvent>
#include <QRect>
#include <QStyleOptionComboBox>
#include <QStylePainter>

namespace Docutaz
{
    // A QComboBox that keeps its current-item icon visible under the application
    // stylesheet. A stylesheet'd non-editable QComboBox normally drops the icon of
    // the selected item; here we paint the flat frame/arrow via the style (with an
    // empty label so the style draws no text) and then draw the icon + text
    // ourselves. Used for the connection "Environment" picker so its colour dot
    // shows on a flat combo.
    class FlatComboBox : public QComboBox
    {
    public:
        using QComboBox::QComboBox;

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QStylePainter painter(this);
            QStyleOptionComboBox opt;
            initStyleOption(&opt);

            // Draw the frame + drop-down arrow only (blank label).
            opt.currentText.clear();
            opt.currentIcon = QIcon();
            painter.drawComplexControl(QStyle::CC_ComboBox, opt);

            QRect field = style()->subControlRect(QStyle::CC_ComboBox, &opt,
                                                  QStyle::SC_ComboBoxEditField, this);
            field.adjust(2, 0, -2, 0);

            int x = field.left();
            const QIcon ic = itemIcon(currentIndex());
            if (!ic.isNull())
            {
                const QSize is = iconSize();
                const QRect ir(x, field.center().y() - is.height() / 2,
                               is.width(), is.height());
                ic.paint(&painter, ir, Qt::AlignCenter);
                x += is.width() + 6;
            }

            const QRect tr(x, field.top(), field.right() - x, field.height());
            painter.setPen(palette().color(isEnabled() ? QPalette::Active
                                                        : QPalette::Disabled,
                                           QPalette::Text));
            painter.drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
                             painter.fontMetrics().elidedText(currentText(),
                                                              Qt::ElideRight, tr.width()));
        }
    };
}
