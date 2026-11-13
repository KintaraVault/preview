// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "sidebar.h"
#include "pdfdocument.h"

#include "ui/color_provider.h"
#include "ui/image_view.h"
#include "ui/label.h"
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
static constexpr double kNumberGap = 4;

// The Files sidebar's: one and a half small spacings, on whole device pixels.
static double plateInset(const ui::View* view)
{
    const double scale = view->window() ? view->window()->scale() : 1;
    return std::round(kSmallSpacing * 1.5 * scale) / scale;
}

static void paintPlate(ui::Painter& painter, const ui::View* view, bool pressed, bool selected)
{
    ui::Color fill {0, 0, 0, 0};
    if (pressed)
        fill = ui::color(ui::ColorId::ItemPressed);
    else if (selected)
        fill = ui::Theme::current().text.withAlpha(0.05);
    if (fill.a == 0)
        return;
    const double inset = plateInset(view);
    painter.drawRoundedRect({inset, 0, view->geometry().width - inset * 2, view->geometry().height},
                            {ui::Theme::current().mediumRadius, 0, ui::Corners::Circular}, fill);
}

// One thumbnail on the selection's rounded plate: a file's, or a page's
// with its number under it.
class Sidebar::Row : public ui::View
{
public:
    explicit Row(Sidebar& sidebar)
        : m_sidebar(sidebar)
    {
        m_thumbnail = addChild<ui::ImageView>();
        m_thumbnail->setThumbnail(true);
        m_thumbnail->setFillMode(ui::ImageView::FillMode::PreserveAspectFit);
        m_number = addChild<ui::Label>();
        m_number->setFontSize(11);
        m_number->setTextAlignment(ui::Label::TextAlignment::Center);
    }

    void bind(size_t row)
    {
        m_row = int(row);
        const bool page = m_sidebar.m_document;
        m_thumbnail->setVisible(!page);
        m_number->setVisible(page);
        m_thumbnail->setSource(page ? std::string() : m_sidebar.m_paths[row]);
        m_number->setText(page ? std::to_string(row + 1) : std::string());
        requestLayout();
        requestPaint();
    }

    int row() const { return m_row; }
    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const double inset = plateInset(this);
        const ui::RectF area = thumbnailArea();
        m_thumbnail->setGeometry(area);
        const ui::SizeF number = m_number->sizeHint();
        m_number->setColor(ui::ColorId::DisabledText);
        m_number->setGeometry({inset, geometry().height - kThumbnailGap - number.height,
                               std::max(0.0, geometry().width - 2 * inset), number.height});
        if (m_sidebar.m_document)
            requestThumbnail();
    }

    void paint(ui::Painter& painter) override
    {
        paintPlate(painter, this, m_pressed, m_row == m_sidebar.m_current);
        PdfDocument* document = m_sidebar.m_document;
        if (!document || m_row < 0 || m_row >= document->pageCount())
            return;
        const ui::SizeF page = document->pageSizes()[size_t(m_row)];
        const ui::RectF area = thumbnailArea();
        const double scale = std::min(area.width / page.width, area.height / page.height);
        const double width = page.width * scale;
        const double height = page.height * scale;
        const double ratio = painter.scale();
        const ui::RectF rect {std::round((area.x + (area.width - width) / 2) * ratio) / ratio,
                              std::round((area.y + (area.height - height) / 2) * ratio) / ratio, width, height};
        const double line = 1 / ratio;
        const ui::Theme& theme = ui::Theme::current();
        painter.fillRect(rect.adjusted(-line, -line, line, line), theme.text.withAlpha(theme.darkMode ? 0.2 : 0.15));
        painter.fillRect(rect, {255, 255, 255, 255});
        if (const ui::RasterImage* image = document->anyThumbnail(m_row))
            painter.drawImage(image->skImage(), rect);
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
    ui::RectF thumbnailArea() const
    {
        const double inset = plateInset(this) + kThumbnailGap;
        const double bottom = m_sidebar.m_document ? kThumbnailGap + m_number->sizeHint().height + kNumberGap
                                                   : kThumbnailGap;
        return {inset, kThumbnailGap, std::max(0.0, geometry().width - 2 * inset),
                std::max(0.0, geometry().height - kThumbnailGap - bottom)};
    }

    void requestThumbnail()
    {
        const ui::RectF area = thumbnailArea();
        const double ratio = window() && window()->scale() > 0 ? window()->scale() : 1;
        m_sidebar.m_document->thumbnail(m_row, base::Size(int(std::ceil(area.width * ratio)),
                                                          int(std::ceil(area.height * ratio))));
    }

    Sidebar& m_sidebar;
    ui::ImageView* m_thumbnail;
    ui::Label* m_number;
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
    m_subscriptions.push_back(m_list->scrolled.connect([this] { keepVisibleThumbnails(); }));
}

void Sidebar::setItems(const std::vector<std::string>& paths)
{
    if (paths == m_paths && !m_document)
        return;
    setDocument(nullptr);
    m_paths = paths;
    m_list->setRowCount(m_paths.size());
    m_list->reload();
}

void Sidebar::setDocument(PdfDocument* document)
{
    if (document == m_document)
        return;
    m_subscriptions.resize(1);
    m_document = document;
    m_paths.clear();
    if (document) {
        m_subscriptions.push_back(document->thumbnailChanged.connect([this](int page) {
            if (ui::View* row = m_list->itemForRow(size_t(page)))
                row->requestPaint();
        }));
    }
    m_list->setRowCount(document ? size_t(document->pageCount()) : 0);
    m_list->reload();
}

size_t Sidebar::count() const
{
    return m_list->rowCount();
}

void Sidebar::setCurrent(int index)
{
    if (m_current >= 0) {
        if (ui::View* row = m_list->itemForRow(size_t(m_current)))
            row->requestPaint();
    }
    m_current = index;
    if (index < 0)
        return;
    if (ui::View* row = m_list->itemForRow(size_t(index)))
        row->requestPaint();
    m_list->scrollToRow(size_t(index));
}

void Sidebar::refresh()
{
    m_list->reload();
}

void Sidebar::keepVisibleThumbnails()
{
    if (!m_document)
        return;
    const double step = kRowHeight + kRowSpacing;
    const int first = int(m_list->scrollPosition().y / step);
    const int last = int((m_list->scrollPosition().y + m_list->geometry().height) / step);
    m_document->keepThumbnailRequests(first - 1, last + 1);
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
