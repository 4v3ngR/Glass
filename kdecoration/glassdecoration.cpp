/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 * Copyright 2014  Hugo Pereira Da Costa <hugo.pereira@free.fr>
 * Copyright 2018  Vlad Zahorodnii <vlad.zahorodnii@kde.org>
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

#include "glassdecoration.h"

#include "config-glass.h"
#include "glass.h"
#include "glasssettingsprovider.h"

#include "glassbutton.h"

#include "glassboxshadowrenderer.h"

#include <KDecoration3/DecorationButtonGroup>
#include <KDecoration3/DecorationShadow>
#include <KDecoration3/ScaleHelpers>

#include <KColorUtils>
#include <KConfigGroup>
#include <KPluginFactory>
#include <KSharedConfig>

#include <QPainter>
#include <QTextStream>
#include <QTimer>
#include <QVariantAnimation>

#if GLASS_HAVE_X11
#include <QX11Info>
#endif

#include <cmath>

K_PLUGIN_FACTORY_WITH_JSON(GlassDecoFactory, "glass.json", registerPlugin<Glass::Decoration>(); registerPlugin<Glass::Button>();)

namespace
{
struct ShadowParams {
    ShadowParams()
        : offset(QPoint(0, 0))
        , radius(0)
        , opacity(0)
    {
    }

    ShadowParams(const QPoint &offset, int radius, qreal opacity)
        : offset(offset)
        , radius(radius)
        , opacity(opacity)
    {
    }

    QPoint offset;
    int radius;
    qreal opacity;
};

struct CompositeShadowParams {
    CompositeShadowParams() = default;

    CompositeShadowParams(const QPoint &offset, const ShadowParams &shadow1, const ShadowParams &shadow2)
        : offset(offset)
        , shadow1(shadow1)
        , shadow2(shadow2)
    {
    }

    bool isNone() const
    {
        return qMax(shadow1.radius, shadow2.radius) == 0;
    }

    QPoint offset;
    ShadowParams shadow1;
    ShadowParams shadow2;
};

const CompositeShadowParams s_shadowParams[] = {
    // None
    CompositeShadowParams(),
    // Small
    CompositeShadowParams(QPoint(0, 4), ShadowParams(QPoint(0, 0), 16, 1), ShadowParams(QPoint(0, -2), 8, 0.4)),
    // Medium
    CompositeShadowParams(QPoint(0, 8), ShadowParams(QPoint(0, 0), 32, 0.9), ShadowParams(QPoint(0, -4), 16, 0.3)),
    // Large
    CompositeShadowParams(QPoint(0, 12), ShadowParams(QPoint(0, 0), 48, 0.8), ShadowParams(QPoint(0, -6), 24, 0.2)),
    // Very large
    CompositeShadowParams(QPoint(0, 16), ShadowParams(QPoint(0, 0), 64, 0.7), ShadowParams(QPoint(0, -8), 32, 0.1)),
};

inline CompositeShadowParams lookupShadowParams(int size)
{
    switch (size) {
    case Glass::InternalSettings::ShadowNone:
        return s_shadowParams[0];
    case Glass::InternalSettings::ShadowSmall:
        return s_shadowParams[1];
    case Glass::InternalSettings::ShadowMedium:
        return s_shadowParams[2];
    case Glass::InternalSettings::ShadowLarge:
        return s_shadowParams[3];
    case Glass::InternalSettings::ShadowVeryLarge:
        return s_shadowParams[4];
    default:
        // Fallback to the Large size.
        return s_shadowParams[3];
    }
}
}

namespace Glass
{

using KDecoration3::ColorGroup;
using KDecoration3::ColorRole;

//________________________________________________________________
static QColor g_shadowColor = Qt::black;
static std::shared_ptr<KDecoration3::DecorationShadow> g_sShadow;

//________________________________________________________________
Decoration::Decoration(QObject *parent, const QVariantList &args)
    : KDecoration3::Decoration(parent, args)
    , m_animation(new QVariantAnimation(this))
{
}

//________________________________________________________________
Decoration::~Decoration()
{
}

//________________________________________________________________
void Decoration::setOpacity(qreal value)
{
    if (m_opacity == value)
        return;
    m_opacity = value;
    update();
}

//________________________________________________________________
QColor Decoration::titleBarColor() const
{
    auto c = window();
    if (hideTitleBar())
        return c->color(ColorGroup::Inactive, ColorRole::TitleBar);
    else if (m_animation->state() == QAbstractAnimation::Running) {
        return KColorUtils::mix(c->color(ColorGroup::Inactive, ColorRole::TitleBar), c->color(ColorGroup::Active, ColorRole::TitleBar), m_opacity);
    } else
        return c->color(c->isActive() ? ColorGroup::Active : ColorGroup::Inactive, ColorRole::TitleBar);
}
//________________________________________________________________
QColor Decoration::outlineColor() const
{
    auto c(window());
    if (!m_internalSettings->drawTitleBarSeparator())
        return QColor();
    if (m_animation->state() == QAbstractAnimation::Running) {
        QColor color(c->palette().color(QPalette::Highlight));
        color.setAlpha(color.alpha() * m_opacity);
        return color;
    } else if (c->isActive())
        return c->palette().color(QPalette::Highlight);
    else
        return QColor();
}

//________________________________________________________________
QColor Decoration::fontColor() const
{
    auto c = window();
    return c->color(c->isActive() ? ColorGroup::Active : ColorGroup::Inactive, ColorRole::Foreground);
}

//________________________________________________________________
bool Decoration::init()
{
    auto c = window();

    // active state change animation
    // It is important start and end value are of the same type, hence 0.0 and not just 0
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        setOpacity(value.toReal());
    });

    reconfigure();
    updateTitleBar();
    updateBlur();
    auto s = settings();
    connect(s.get(), &KDecoration3::DecorationSettings::borderSizeChanged, this, &Decoration::recalculateBorders);

    // a change in font might cause the borders to change
    connect(s.get(), &KDecoration3::DecorationSettings::fontChanged, this, &Decoration::recalculateBorders);
    connect(s.get(), &KDecoration3::DecorationSettings::spacingChanged, this, &Decoration::recalculateBorders);

    // buttons
    connect(s.get(), &KDecoration3::DecorationSettings::spacingChanged, this, &Decoration::updateButtonsGeometryDelayed);
    connect(s.get(), &KDecoration3::DecorationSettings::decorationButtonsLeftChanged, this, &Decoration::updateButtonsGeometryDelayed);
    connect(s.get(), &KDecoration3::DecorationSettings::decorationButtonsRightChanged, this, &Decoration::updateButtonsGeometryDelayed);

    // full reconfiguration
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, this, &Decoration::reconfigure);
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, SettingsProvider::self(), &SettingsProvider::reconfigure, Qt::UniqueConnection);
    connect(s.get(), &KDecoration3::DecorationSettings::reconfigured, this, &Decoration::updateButtonsGeometryDelayed);

    connect(c, &KDecoration3::DecoratedWindow::adjacentScreenEdgesChanged, this, &Decoration::recalculateBorders);
    connect(c, &KDecoration3::DecoratedWindow::maximizedHorizontallyChanged, this, &Decoration::recalculateBorders);
    connect(c, &KDecoration3::DecoratedWindow::maximizedVerticallyChanged, this, &Decoration::recalculateBorders);
    connect(c, &KDecoration3::DecoratedWindow::shadedChanged, this, &Decoration::recalculateBorders);
    connect(c, &KDecoration3::DecoratedWindow::captionChanged, this, [this]() {
        // update the caption area
        update(titleBar());
    });

    connect(c, &KDecoration3::DecoratedWindow::activeChanged, this, &Decoration::updateAnimationState);
    connect(c, &KDecoration3::DecoratedWindow::adjacentScreenEdgesChanged, this, &Decoration::updateTitleBar);
    connect(this, &KDecoration3::Decoration::bordersChanged, this, &Decoration::updateTitleBar);

    connect(c, &KDecoration3::DecoratedWindow::activeChanged, this, &Decoration::updateBlur);
    connect(c, &KDecoration3::DecoratedWindow::widthChanged, this, &Decoration::updateTitleBar);
    connect(c, &KDecoration3::DecoratedWindow::maximizedChanged, this, &Decoration::updateTitleBar);

    connect(c, &KDecoration3::DecoratedWindow::sizeChanged, this, &Decoration::updateBlur); // recalculate blur region on resize

    connect(c, &KDecoration3::DecoratedWindow::widthChanged, this, &Decoration::updateButtonsGeometry);
    connect(c, &KDecoration3::DecoratedWindow::maximizedChanged, this, &Decoration::updateButtonsGeometry);
    connect(c, &KDecoration3::DecoratedWindow::adjacentScreenEdgesChanged, this, &Decoration::updateButtonsGeometry);
    connect(c, &KDecoration3::DecoratedWindow::shadedChanged, this, &Decoration::updateButtonsGeometry);

    // shade button doesn't resize properly so this is now required
    connect(this, &KDecoration3::Decoration::bordersChanged, this, &Decoration::updateButtonsGeometryDelayed);

    createButtons();
    return true;
}

//________________________________________________________________
void Decoration::updateBlur()
{
}

//________________________________________________________________
void Decoration::calculateWindowAndTitleBarShapes(const bool windowShapeOnly)
{
    auto c = window();
    auto s = settings();

    if (!windowShapeOnly || c->isShaded()) {
        // set titleBar geometry and path
        m_titleRect = QRect(QPoint(0, 0), QSize(size().width(), borderTop()));
        m_titleBarPath->clear(); // clear the path for subsequent calls to this function
        if (isMaximized() || !s->isAlphaChannelSupported()) {
            m_titleBarPath->addRect(m_titleRect);

        } else if (c->isShaded()) {
            m_titleBarPath->addRoundedRect(m_titleRect, m_internalSettings->cornerRadius(), m_internalSettings->cornerRadius());

        } else {
            QPainterPath clipRect;
            clipRect.addRect(m_titleRect);

            // the rect is made a little bit larger to be able to clip away the rounded corners at the bottom and sides
            m_titleBarPath->addRoundedRect(m_titleRect.adjusted(isLeftEdge() ? -m_internalSettings->cornerRadius() : 0,
                                                                isTopEdge() ? -m_internalSettings->cornerRadius() : 0,
                                                                isRightEdge() ? m_internalSettings->cornerRadius() : 0,
                                                                m_internalSettings->cornerRadius()),
                                           m_internalSettings->cornerRadius(),
                                           m_internalSettings->cornerRadius());

            *m_titleBarPath = m_titleBarPath->intersected(clipRect);
        }
    }

    // set windowPath
    m_windowPath->clear(); // clear the path for subsequent calls to this function
    if (!c->isShaded()) {
        if (s->isAlphaChannelSupported() && !isMaximized())
            m_windowPath->addRoundedRect(rect(), m_internalSettings->cornerRadius(), m_internalSettings->cornerRadius());
        else
            m_windowPath->addRect(rect());

    } else {
        *m_windowPath = *m_titleBarPath;
    }
}

//________________________________________________________________
void Decoration::updateTitleBar()
{
    // The titlebar rect has margins around it so the window can be resized by dragging a decoration edge.
    auto s = settings();
    const bool maximized = isMaximized();
    const qreal width = maximized ? window()->width() : window()->width() - 2 * s->smallSpacing() * Metrics::TitleBar_SideMargin;
    const qreal height = (maximized || isTopEdge()) ? borderTop() : borderTop() - s->smallSpacing() * Metrics::TitleBar_TopMargin;
    const qreal x = maximized ? 0 : s->smallSpacing() * Metrics::TitleBar_SideMargin;
    const qreal y = (maximized || isTopEdge()) ? 0 : s->smallSpacing() * Metrics::TitleBar_TopMargin;
    setTitleBar(QRectF(x, y, width, height));
}

//________________________________________________________________
void Decoration::updateAnimationState()
{
    if (m_internalSettings->animationsEnabled()) {
        auto c = window();
        m_animation->setDirection(c->isActive() ? QAbstractAnimation::Forward : QAbstractAnimation::Backward);
        if (m_animation->state() != QAbstractAnimation::Running)
            m_animation->start();

    } else {
        update();
    }
}

//________________________________________________________________
qreal Decoration::borderSize(bool bottom, qreal scale) const
{
    const qreal pixelSize = KDecoration3::pixelSize(scale);
    const qreal baseSize = std::max<qreal>(pixelSize, KDecoration3::snapToPixelGrid(settings()->smallSpacing(), scale));

    if (m_internalSettings && (m_internalSettings->mask() & BorderSize)) {
        switch (m_internalSettings->borderSize()) {
        case InternalSettings::BorderNone:
            return 0;
        case InternalSettings::BorderNoSides:
            return bottom ? KDecoration3::snapToPixelGrid(std::max(4.0, baseSize), scale) : 0;
        default:
        case InternalSettings::BorderTiny:
            return bottom ? KDecoration3::snapToPixelGrid(std::max(4.0, baseSize), scale) : baseSize;
        case InternalSettings::BorderNormal:
            return baseSize * 2;
        case InternalSettings::BorderLarge:
            return baseSize * 3;
        case InternalSettings::BorderVeryLarge:
            return baseSize * 4;
        case InternalSettings::BorderHuge:
            return baseSize * 5;
        case InternalSettings::BorderVeryHuge:
            return baseSize * 6;
        case InternalSettings::BorderOversized:
            return baseSize * 10;
        }

    } else {
        switch (settings()->borderSize()) {
        case KDecoration3::BorderSize::None:
            return 0;
        case KDecoration3::BorderSize::NoSides:
            return bottom ? KDecoration3::snapToPixelGrid(std::max(4.0, baseSize), scale) : 0;
        default:
        case KDecoration3::BorderSize::Tiny:
            return bottom ? KDecoration3::snapToPixelGrid(std::max(4.0, baseSize), scale) : baseSize;
        case KDecoration3::BorderSize::Normal:
            return baseSize * 2;
        case KDecoration3::BorderSize::Large:
            return baseSize * 3;
        case KDecoration3::BorderSize::VeryLarge:
            return baseSize * 4;
        case KDecoration3::BorderSize::Huge:
            return baseSize * 5;
        case KDecoration3::BorderSize::VeryHuge:
            return baseSize * 6;
        case KDecoration3::BorderSize::Oversized:
            return baseSize * 10;
        }
    }
}

//________________________________________________________________
void Decoration::reconfigure()
{
    m_internalSettings = SettingsProvider::self()->internalSettings(this);

    // animation
    m_animation->setDuration(m_internalSettings->animationsDuration());

    // borders
    recalculateBorders();

    updateBlur();
}

//________________________________________________________________
void Decoration::recalculateBorders()
{
    auto c = window();
    auto s = settings();
    auto scale = c->nextScale();

    // left, right and bottom borders
    const qreal left = borderSize(false, scale);
    const qreal right = borderSize(false, scale);
    const qreal bottom = borderSize(true, scale);

    qreal top = 0;
    if (hideTitleBar())
        top = bottom;
    else {
        QFontMetrics fm(s->font());
        top += KDecoration3::snapToPixelGrid(std::max(fm.height(), buttonSize()), scale);

        // padding below
        const int baseSize = s->smallSpacing() * 2;
        top += KDecoration3::snapToPixelGrid(baseSize * Metrics::TitleBar_BottomMargin, scale);
    }

    setBorders(QMarginsF(left, top, right, bottom));

    // extended sizes
    const qreal extSize = KDecoration3::snapToPixelGrid(settings()->largeSpacing(), window()->nextScale());
    qreal extSides = 0;
    qreal extBottom = 0;
    if (hasNoBorders()) {
        if (!isMaximizedHorizontally())
            extSides = extSize;
        if (!isMaximizedVertically())
            extBottom = extSize;

    } else if (hasNoSideBorders() && !isMaximizedHorizontally()) {
        extSides = extSize;
    }

    setResizeOnlyBorders(QMarginsF(extSides, 0, extSides, extBottom));
}

//________________________________________________________________
void Decoration::createButtons()
{
    m_leftButtons = new KDecoration3::DecorationButtonGroup(KDecoration3::DecorationButtonGroup::Position::Left, this, &Button::create);
    m_rightButtons = new KDecoration3::DecorationButtonGroup(KDecoration3::DecorationButtonGroup::Position::Right, this, &Button::create);
    updateButtonsGeometry();
}

//________________________________________________________________
void Decoration::updateButtonsGeometryDelayed()
{
    QTimer::singleShot(0, this, &Decoration::updateButtonsGeometry);
}

//________________________________________________________________
void Decoration::updateButtonsGeometry()
{
    const auto s = settings();

    // adjust button position
    const auto buttonList = m_leftButtons->buttons() + m_rightButtons->buttons();
    for (KDecoration3::DecorationButton *button : buttonList) {
        auto btn = static_cast<Button *>(button);

        const int verticalOffset = (isTopEdge() ? s->smallSpacing() * Metrics::TitleBar_TopMargin : 0);

        const QSizeF preferredSize = btn->preferredSize();
        const int bHeight = preferredSize.height() + verticalOffset;
        const int bWidth = preferredSize.width();

        btn->setGeometry(QRectF(QPoint(0, 0), QSizeF(bWidth, bHeight)));
        btn->setPadding(QMargins(0, verticalOffset, 0, 0));
        btn->setOffset(QPointF(0, verticalOffset));
        btn->setIconSize(QSizeF(bHeight, bWidth));
    }

    // left buttons
    if (!m_leftButtons->buttons().isEmpty()) {
        // spacing
        m_leftButtons->setSpacing(s->smallSpacing() * Metrics::TitleBar_ButtonSpacing);

        // padding
        const int vPadding = isTopEdge() ? 0 : s->smallSpacing() * Metrics::TitleBar_TopMargin;
        const int hPadding = s->smallSpacing() * Metrics::TitleBar_SideMargin;
        if (isLeftEdge()) {
            // add offsets on the side buttons, to preserve padding, but satisfy Fitts law
            auto button = static_cast<Button *>(m_leftButtons->buttons().front());

            QRectF geometry = button->geometry();
            geometry.adjust(-hPadding, 0, 0, 0);
            button->setGeometry(geometry);
            button->setFlag(Button::FlagFirstInList);
            button->setLeftPadding(hPadding);
            button->setIconSize(button->preferredSize());

            m_leftButtons->setPos(QPointF(0, vPadding));

        } else {
            m_leftButtons->setPos(QPointF(hPadding + borderLeft(), vPadding));
        }
    }

    // right buttons
    if (!m_rightButtons->buttons().isEmpty()) {
        // spacing
        m_rightButtons->setSpacing(s->smallSpacing() * Metrics::TitleBar_ButtonSpacing);

        // padding
        const int vPadding = isTopEdge() ? 0 : s->smallSpacing() * Metrics::TitleBar_TopMargin;
        const int hPadding = s->smallSpacing() * Metrics::TitleBar_SideMargin;
        if (isRightEdge()) {
            auto button = static_cast<Button *>(m_rightButtons->buttons().back());

            QRectF geometry = button->geometry();
            geometry.adjust(0, 0, hPadding, 0);
            button->setGeometry(geometry);
            button->setFlag(Button::FlagFirstInList);
            button->setRightPadding(hPadding);
            button->setIconSize(button->preferredSize());

            m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width(), vPadding));

        } else {
            m_rightButtons->setPos(QPointF(size().width() - m_rightButtons->geometry().width() - hPadding - borderRight(), vPadding));
        }
    }

    update();
}

//________________________________________________________________
void Decoration::paint(QPainter *painter, const QRectF &repaintRegion)
{
    if (!hideTitleBar()) {
        painter->save();
        paintTitleBar(painter, repaintRegion);
        painter->restore();
    }
}

//________________________________________________________________
void Decoration::paintTitleBar(QPainter *painter, const QRectF &repaintRegion)
{
    const auto c = window();
    auto s = settings();

    // draw caption
    painter->translate(0, 1);
    painter->setFont(s->font());
    painter->setPen(fontColor());
    const auto cR = captionRect();
    const QString caption = painter->fontMetrics().elidedText(c->caption(), Qt::ElideMiddle, cR.first.width());
    painter->drawText(cR.first, cR.second | Qt::TextSingleLine, caption);

    // draw all buttons
    m_leftButtons->paint(painter, repaintRegion);
    m_rightButtons->paint(painter, repaintRegion);
}

//________________________________________________________________
int Decoration::buttonSize() const
{
    const int baseSize = settings()->gridUnit();
    switch (m_internalSettings->buttonSize()) {
    case InternalSettings::ButtonTiny:
        return baseSize;
    case InternalSettings::ButtonSmall:
        return baseSize * 1.5;
    default:
    case InternalSettings::ButtonDefault:
        return baseSize * 2;
    case InternalSettings::ButtonLarge:
        return baseSize * 2.5;
    case InternalSettings::ButtonVeryLarge:
        return baseSize * 3.5;
    }
}

//________________________________________________________________
int Decoration::captionHeight() const
{
    return hideTitleBar() ? borderTop() : buttonSize();
}

//________________________________________________________________
QPair<QRectF, Qt::Alignment> Decoration::captionRect() const
{
    if (hideTitleBar())
        return qMakePair(QRect(), Qt::AlignCenter);
    else {
        auto c = window();
        const qreal leftOffset =
            KDecoration3::snapToPixelGrid(m_leftButtons->buttons().isEmpty() ? Metrics::TitleBar_SideMargin * settings()->smallSpacing()
                                                                             : m_leftButtons->geometry().x() + m_leftButtons->geometry().width()
                                                  + Metrics::TitleBar_SideMargin * settings()->smallSpacing(),
                                          window()->scale());

        const qreal rightOffset = KDecoration3::snapToPixelGrid(m_rightButtons->buttons().isEmpty() ? Metrics::TitleBar_SideMargin * settings()->smallSpacing()
                                                                                                    : size().width() - m_rightButtons->geometry().x()
                                                                        + Metrics::TitleBar_SideMargin * settings()->smallSpacing(),
                                                                window()->scale());

        const qreal yOffset = KDecoration3::snapToPixelGrid(settings()->smallSpacing() * Metrics::TitleBar_TopMargin, window()->scale());
        const QRectF maxRect(leftOffset, yOffset, size().width() - leftOffset - rightOffset, captionHeight());

        switch (m_internalSettings->titleAlignment()) {
        case InternalSettings::AlignLeft:
            return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignLeft);

        case InternalSettings::AlignRight:
            return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignRight);

        case InternalSettings::AlignCenter:
            return qMakePair(maxRect, Qt::AlignCenter);

        default:
        case InternalSettings::AlignCenterFullWidth: {
            // full caption rect
            const QRectF fullRect = QRect(0, yOffset, size().width(), captionHeight());
            QRectF boundingRect(settings()->fontMetrics().boundingRect(c->caption()));

            // text bounding rect
            boundingRect.setTop(yOffset);
            boundingRect.setHeight(captionHeight());
            boundingRect.moveLeft((size().width() - boundingRect.width()) / 2);

            if (boundingRect.left() < leftOffset)
                return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignLeft);
            else if (boundingRect.right() > size().width() - rightOffset)
                return qMakePair(maxRect, Qt::AlignVCenter | Qt::AlignRight);
            else
                return qMakePair(fullRect, Qt::AlignCenter);
        }
        }
    }
}

} // namespace

#include "glassdecoration.moc"
