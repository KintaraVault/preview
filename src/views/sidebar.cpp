// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "sidebar.h"

#include "ui/color_provider.h"
#include "ui/image_view.h"
#include "ui/list_view.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace cutefish;

static constexpr double kSmallSpacing = 6;
static constexpr double kRowHeight = 120;
static constexpr double kRowSpacing = 3;
// From the selection's edge to the thumbnail.
static constexpr double kThumbnailGap = 8;

// One thumbnail on the selection's rounded plate.
class Sidebar::Row : public ui::View
{
public:
    explicit Row(Sidebar& sidebar)
        : m_sidebar(sidebar)
    {
        m_thumbnail = addChild<ui::ImageView>();
        m_thumbnail->setThumbnail(true);
        m_thumbnail->setFillMode(ui::ImageView::FillMode::PreserveAspectFit);
    }

    void bind(size_t row)
    {
        m_row = int(row);
        m_thumbnail->setSource(m_sidebar.m_paths[row]);
        requestPaint();
    }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const double inset = plateInset();
        m_thumbnail->setGeometry({inset + kThumbnailGap, kThumbnailGap,
                                  std::max(0.0, geometry().width - 2 * (inset + kThumbnailGap)),
                                  std::max(0.0, geometry().height - 2 * kThumbnailGap)});
    }

    void paint(ui::Painter& painter) override
    {
        ui::Color fill {0, 0, 0, 0};
        if (m_pressed)
            fill = ui::color(ui::ColorId::ItemPressed);
        else if (m_row == m_sidebar.m_current)
            fill = ui::Theme::current().text.withAlpha(0.05);
        if (fill.a == 0)
            return;
        const double inset = plateInset();
        painter.drawRoundedRect({inset, 0, geometry().width - inset * 2, geometry().height},
                                {ui::Theme::current().mediumRadius, 0, ui::Corners::Circular}, fill);
    }

    void pointerPress(const ui::PointerEvent& event) override
    {
        if (event.button != ui::MouseButton::Left)
            return;
        m_pressed = true;
        requestPaint();
    }

    void pointerRelease(const ui::PointerEvent& event) override
    {
        if (!m_pressed)
            return;
        m_pressed = false;
        requestPaint();
        if (localRect().contains(event.position) && m_sidebar.onActivated)
            m_sidebar.onActivated(m_row);
    }

private:
    // The Files sidebar's: one and a half small spacings, on whole device pixels.
    double plateInset() const
    {
        const double scale = window() ? window()->scale() : 1;
        return std::round(kSmallSpacing * 1.5 * scale) / scale;
    }

    Sidebar& m_sidebar;
    ui::ImageView* m_thumbnail;
    int m_row = -1;
    bool m_pressed = false;
};

Sidebar::Sidebar(double headerHeight)
    : m_headerHeight(headerHeight)
{
    m_list = addChild<ui::ListView>();
    m_list->setRowHeight(kRowHeight);
    m_list->setSpacing(kRowSpacing);
    m_list->createItem = [this] { return std::make_unique<Row>(*this); };
    m_list->bindItem = [](ui::View* item, size_t row) { static_cast<Row*>(item)->bind(row); };
}

void Sidebar::setItems(const std::vector<std::string>& paths)
{
    if (paths == m_paths)
        return;
    m_paths = paths;
    m_list->setRowCount(m_paths.size());
    m_list->reload();
}

void Sidebar::setCurrent(int index)
{
    m_current = index;
    m_list->reload();
    if (index >= 0)
        m_list->scrollToRow(size_t(index));
}

void Sidebar::refresh()
{
    m_list->reload();
}

void Sidebar::layout()
{
    const double top = m_headerHeight + kSmallSpacing;
    m_list->setGeometry({0, top, geometry().width, std::max(0.0, geometry().height - top)});
}

void Sidebar::paint(ui::Painter& painter)
{
    // As the Files sidebar: a step below the window's colour in both modes,
    // and the header backdrop's hairline down the edge, 0.5 px, below 2x a
    // device pixel with its alpha scaled to the same weight.
    painter.fillRect(localRect(), ui::Theme::current().background.withAlpha(0.7));
    const double scale = painter.scale();
    const double devicePixels = std::max(1.0, std::round(0.5 * scale));
    const double lineWidth = devicePixels / scale;
    const ui::Color separator = ui::color(ui::ColorId::HeaderSeparator);
    painter.drawRoundedRect({geometry().width - lineWidth, 0, lineWidth, geometry().height}, {0},
                            separator.withAlpha(separator.a / 255.0 * 0.5 * scale / devicePixels));
}
