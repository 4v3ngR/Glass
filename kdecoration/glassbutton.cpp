/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 * Copyright 2014  Hugo Pereira Da Costa <hugo.pereira@free.fr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "glassbutton.h"

#include <KColorUtils>
#include <KDecoration3/DecoratedWindow>
#include <KIconLoader>

#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace Glass
{

using KDecoration3::ColorGroup;
using KDecoration3::ColorRole;
using KDecoration3::DecorationButtonType;

//__________________________________________________________________
Button::Button(DecorationButtonType type, Decoration *decoration, QObject *parent)
    : DecorationButton(type, decoration, parent)
    , m_animation(new QVariantAnimation(this))
{
    // setup animation
    // It is important start and end value are of the same type, hence 0.0 and not just 0
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    // connections
    connect(decoration->window(), SIGNAL(iconChanged(QIcon)), this, SLOT(update()));
    connect(decoration->settings().get(), &KDecoration3::DecorationSettings::reconfigured, this, &Button::reconfigure);
    connect(this, &KDecoration3::DecorationButton::hoveredChanged, this, &Button::updateAnimationState);

    reconfigure();
}

//__________________________________________________________________
Button::Button(QObject *parent, const QVariantList &args)
    : Button(args.at(0).value<DecorationButtonType>(), args.at(1).value<Decoration *>(), parent)
{
    setGeometry(QRectF(QPointF(0, 0), preferredSize()));
}

//__________________________________________________________________
Button *Button::create(DecorationButtonType type, KDecoration3::Decoration *decoration, QObject *parent)
{
    if (auto d = qobject_cast<Decoration *>(decoration)) {
        Button *b = new Button(type, d, parent);
        const auto c = d->window();
        switch (type) {
        case DecorationButtonType::Close:
            b->setVisible(c->isCloseable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::closeableChanged, b, &Glass::Button::setVisible);
            break;

        case DecorationButtonType::Maximize:
            b->setVisible(c->isMaximizeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::maximizeableChanged, b, &Glass::Button::setVisible);
            break;

        case DecorationButtonType::Minimize:
            b->setVisible(c->isMinimizeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::minimizeableChanged, b, &Glass::Button::setVisible);
            break;

        case DecorationButtonType::ContextHelp:
            b->setVisible(c->providesContextHelp());
            QObject::connect(c, &KDecoration3::DecoratedWindow::providesContextHelpChanged, b, &Glass::Button::setVisible);
            break;

        case DecorationButtonType::Shade:
            b->setVisible(c->isShadeable());
            QObject::connect(c, &KDecoration3::DecoratedWindow::shadeableChanged, b, &Glass::Button::setVisible);
            break;

        case DecorationButtonType::Menu:
            QObject::connect(c, &KDecoration3::DecoratedWindow::iconChanged, b, [b]() {
                b->update();
            });
            break;

        default:
            break;
        }

        return b;
    }

    return nullptr;
}

//__________________________________________________________________
void Button::paint(QPainter *painter, const QRectF &repaintRegion)
{
    Q_UNUSED(repaintRegion)

    if (!decoration())
        return;

    // menu button
    switch (type()) {
    case KDecoration3::DecorationButtonType::Menu: {
        const QRectF iconRect = geometry().marginsRemoved(m_padding);
        const auto c = decoration()->window();
        if (auto deco = qobject_cast<Decoration *>(decoration())) {
            const QPalette activePalette = KIconLoader::global()->customPalette();
            QPalette palette = c->palette();
            palette.setColor(QPalette::WindowText, deco->fontColor());
            KIconLoader::global()->setCustomPalette(palette);
            c->icon().paint(painter, iconRect.toRect());
            if (activePalette == QPalette()) {
                KIconLoader::global()->resetPalette();
            } else {
                KIconLoader::global()->setCustomPalette(palette);
            }
        } else {
            c->icon().paint(painter, iconRect.toRect());
        }
        break;
    }
    case KDecoration3::DecorationButtonType::Spacer:
        break;
    default:
        painter->save();
        drawIcon(painter);
        painter->restore();
        break;
    }
}

//__________________________________________________________________
void Button::drawIcon(QPainter *painter) const
{
    painter->setRenderHints(QPainter::Antialiasing);

    const QRectF rect = geometry().marginsRemoved(m_padding);
    const qreal width(rect.width());

    painter->translate(rect.topLeft());

    painter->scale(width / 40, width / 40);

    // render background
    const QColor backgroundColor(this->backgroundColor());
    if (backgroundColor.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(backgroundColor);
        painter->drawEllipse(QRectF(0, 0, 36, 36));
    }

    // render mark
    const QColor foregroundColor(this->foregroundColor());
    if (foregroundColor.isValid()) {
        // setup painter
        QPen pen(foregroundColor);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::MiterJoin);
        pen.setWidthF(PenWidth::Symbol * qMax((qreal)1.0, 40 / width));

        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        // TODO: create different forground images for the traffic lights
        switch (type()) {
        case DecorationButtonType::Close: {
            /*
painter->drawLine(QPointF(5, 5), QPointF(13, 13));
painter->drawLine(13, 5, 5, 13);
*/
            break;
        }

        case DecorationButtonType::Maximize: {
            /*
if (isChecked()) {
pen.setJoinStyle(Qt::RoundJoin);
painter->setPen(pen);

painter->drawPolygon(QVector<QPointF>{QPointF(4, 9), QPointF(9, 4), QPointF(14, 9), QPointF(9, 14)});

} else {
painter->drawPolyline(QVector<QPointF>{QPointF(4, 11), QPointF(9, 6), QPointF(14, 11)});
}
*/
            break;
        }

        case DecorationButtonType::Minimize: {
            // painter->drawPolyline(QVector<QPointF>{QPointF(4, 7), QPointF(9, 12), QPointF(14, 7)});
            break;
        }

        case DecorationButtonType::OnAllDesktops: {
            painter->setPen(Qt::NoPen);
            painter->setBrush(foregroundColor);

            if (isChecked()) {
                // outer ring
                painter->drawEllipse(QRectF(6, 6, 24, 24));

                // center dot
                QColor backgroundColor(this->backgroundColor());
                auto d = qobject_cast<Decoration *>(decoration());
                if (!backgroundColor.isValid() && d) {
                    backgroundColor = d->titleBarColor();
                }

                if (backgroundColor.isValid()) {
                    painter->setBrush(backgroundColor);
                    painter->drawEllipse(QRectF(16, 16, 4, 4));
                }

            } else {
                painter->drawPolygon(QVector<QPointF>{QPointF(13, 13), QPointF(24, 6), QPointF(30, 12), QPointF(19, 22)});

                painter->setPen(pen);
                painter->drawLine(QPointF(11, 15), QPointF(21, 25));
                painter->drawLine(QPointF(24, 12), QPointF(9, 27));
            }
            break;
        }

        case DecorationButtonType::Shade: {
            if (isChecked()) {
                painter->drawLine(QPointF(8, 11), QPointF(28, 11));
                painter->drawPolyline(QVector<QPointF>{QPointF(8, 16), QPointF(18, 26), QPointF(28, 16)});

            } else {
                painter->drawLine(QPointF(8, 11), QPointF(28, 11));
                painter->drawPolyline(QVector<QPointF>{QPointF(8, 26), QPointF(18, 16), QPointF(28, 26)});
            }

            break;
        }

        case DecorationButtonType::KeepBelow: {
            painter->drawPolyline(QVector<QPointF>{QPointF(8, 10), QPointF(18, 20), QPointF(28, 10)});

            painter->drawPolyline(QVector<QPointF>{QPointF(8, 18), QPointF(18, 28), QPointF(28, 18)});
            break;
        }

        case DecorationButtonType::KeepAbove: {
            painter->drawPolyline(QVector<QPointF>{QPointF(8, 18), QPointF(18, 8), QPointF(28, 18)});

            painter->drawPolyline(QVector<QPointF>{QPointF(8, 26), QPointF(18, 16), QPointF(28, 26)});
            break;
        }

        case DecorationButtonType::ApplicationMenu: {
            painter->drawRect(QRectF(7, 9, 22, 2));
            painter->drawRect(QRectF(7, 17, 22, 2));
            painter->drawRect(QRectF(7, 25, 22, 2));
            break;
        }

        case DecorationButtonType::ContextHelp: {
            QPainterPath path;
            path.moveTo(10, 12);
            path.arcTo(QRectF(10, 7, 16, 10), 180, -180);
            path.cubicTo(QPointF(25, 19), QPointF(18, 15), QPointF(18, 23));
            painter->drawPath(path);

            painter->drawRect(QRectF(18, 30, 1, 1));

            break;
        }

        default:
            break;
        }
    }
}

//__________________________________________________________________
QColor Button::foregroundColor() const
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return QColor();

    } else if (isPressed()) {
        return d->titleBarColor();

    } else if (type() == DecorationButtonType::Close && d->internalSettings()->outlineCloseButton()) {
        return d->titleBarColor();

    } else if ((type() == DecorationButtonType::KeepBelow || type() == DecorationButtonType::KeepAbove || type() == DecorationButtonType::Shade)
               && isChecked()) {
        return d->titleBarColor();

    } else if (m_animation->state() == QAbstractAnimation::Running) {
        return KColorUtils::mix(d->fontColor(), d->titleBarColor(), m_opacity);

    } else if (isHovered()) {
        return d->titleBarColor();

    } else {
        return d->fontColor();
    }
}

//__________________________________________________________________
QColor Button::backgroundColor() const
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return QColor();
    }

    auto c = d->window();

    if (c->isActive()) {
        if (isPressed()) {
            switch (type()) {
            case DecorationButtonType::Close:
                return QColorConstants::Red;
            case DecorationButtonType::Minimize:
                return QColorConstants::Yellow;
            case DecorationButtonType::Maximize:
                return QColorConstants::Green;
            default:
                return KColorUtils::mix(d->titleBarColor(), d->fontColor(), 0.3);
            }
        }

        QColor color;
        auto alpha = 128;

        if (isHovered()) {
            alpha = 192;
        }

        switch (type()) {
        case DecorationButtonType::Close:
            color = QColorConstants::Red;
            color.setAlpha(alpha);
            return color;
        case DecorationButtonType::Minimize:
            color = QColorConstants::Yellow;
            color.setAlpha(alpha);
            return color;
        case DecorationButtonType::Maximize:
            color = QColorConstants::Green;
            color.setAlpha(alpha);
            return color;
        default:
            break;
        }
    } else {
        QColor color = QColorConstants::LightGray;
        color.setAlpha(128);
        switch (type()) {
        case DecorationButtonType::Close:
        case DecorationButtonType::Minimize:
        case DecorationButtonType::Maximize:
            return color;
        default:
            break;
        }
    }

    return QColor();
}

//________________________________________________________________
void Button::reconfigure()
{
    // animation
    auto d = qobject_cast<Decoration *>(decoration());
    if (!d) {
        return;
    }

    switch (type()) {
    case KDecoration3::DecorationButtonType::Spacer:
        setPreferredSize(QSizeF(d->buttonSize() * 0.5, d->buttonSize()));
        break;
    default:
        setPreferredSize(QSizeF(d->buttonSize(), d->buttonSize()));
        break;
    }

    m_animation->setDuration(d->internalSettings()->animationsDuration());
}

//__________________________________________________________________
void Button::updateAnimationState(bool hovered)
{
    auto d = qobject_cast<Decoration *>(decoration());
    if (!(d && d->internalSettings()->animationsEnabled()))
        return;

    m_animation->setDirection(hovered ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
    if (m_animation->state() != QAbstractAnimation::Running)
        m_animation->start();
}

} // namespace
