// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "sidebar.h"
#include "pdfdocument.h"

#include "base/i18n.h"
#include "ui/cell_layout.h"
#include "ui/color_provider.h"
#include "ui/icon.h"
#include "ui/image_view.h"
#include "ui/label.h"
#include "ui/list_view.h"
#include "ui/painter.h"
#include "ui/segmented_control.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>

using namespace cutefish;
using cutefish::i18n::tr;
using ui::rounding::centeredStart;

static constexpr double kSmallSpacing = 6;
static constexpr double kRowHeight = 120;
static constexpr double kRowSpacing = 3;
// From the selection's edge to the thumbnail.
static constexpr double kThumbnailGap = 8;
static constexpr double kNumberGap = 4;
static constexpr double kOutlineRowHeight = 28;
static constexpr double kOutlineIndent = 14;
static constexpr double kChevronSize = 12;

// The Files sidebar's: one and a half small spacings, on whole device pixels.
static double plateInset(const ui::View* view)
{
    const double scale = view->window() ? view->window()->scale() : 1;
    return std::round(kSmallSpacing * 1.5 * scale) / scale;
}

// The selected thumbnail's plate is the accent, pressed or not.
static void paintPlate(ui::Painter& painter, const ui::View* view, bool pressed, bool selected, bool accent)
{
    ui::Color fill {0, 0, 0, 0};
    if (selected && accent)
        fill = ui::Theme::current().highlight;
    else if (pressed)
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
        m_number->setColor(m_row == m_sidebar.m_current ? ui::ColorRecipe(ui::ColorId::HighlightedText)
                                                        : ui::ColorRecipe(ui::ColorId::DisabledText));
        m_number->setGeometry({inset, geometry().height - kThumbnailGap - number.height,
                               std::max(0.0, geometry().width - 2 * inset), number.height});
        if (m_sidebar.m_document)
            requestThumbnail();
    }

    void paint(ui::Painter& painter) override
    {
        paintPlate(painter, this, m_pressed, m_row == m_sidebar.m_current, true);
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

// An item of the outline, indented by its depth, with a chevron that opens
// and closes the items under it.
class Sidebar::OutlineRow : public ui::View
{
public:
    explicit OutlineRow(Sidebar& sidebar)
        : m_sidebar(sidebar)
    {
        m_chevron = addChild<ui::Icon>("chevron-right-small", kChevronSize);
        m_title = addChild<ui::Label>();
        m_title->setElide(ui::Label::Elide::Right);
    }

    void bind(size_t row)
    {
        m_row = row;
        const OutlineEntry& entry = m_sidebar.m_outlineRows[row];
        m_chevron->setVisible(!entry.item->children.empty());
        m_chevron->setIconName(m_sidebar.m_open.contains(entry.item) ? "chevron-down-small" : "chevron-right-small");
        m_title->setText(entry.item->title);
        requestLayout();
    }

    bool acceptsPointer() const override { return true; }

protected:
    void layout() override
    {
        const ui::Theme& theme = ui::Theme::current();
        const double height = geometry().height;
        const double x = plateInset(this) + kThumbnailGap + m_sidebar.m_outlineRows[m_row].depth * kOutlineIndent;
        m_chevron->setGeometry({x, centeredStart(height, kChevronSize), kChevronSize, kChevronSize});
        m_chevron->setColor(theme.text);
        const double left = x + kChevronSize + 4;
        const ui::SizeF title = m_title->sizeHint();
        m_title->setColor(theme.text);
        m_title->setGeometry({left, centeredStart(height, title.height),
                              std::max(0.0, geometry().width - plateInset(this) - kThumbnailGap - left), title.height});
    }

    void paint(ui::Painter& painter) override { paintPlate(painter, this, m_pressed, false, false); }

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
        if (!localRect().contains(event.position))
            return;
        if (m_chevron->isVisible() && event.position.x < m_title->geometry().x)
            m_sidebar.toggleContentsRow(m_row);
        else
            m_sidebar.activateContentsRow(m_row);
    }

private:
    Sidebar& m_sidebar;
    ui::Icon* m_chevron;
    ui::Label* m_title;
    size_t m_row = 0;
    bool m_pressed = false;
};

Sidebar::Sidebar(double headerHeight)
    : m_headerHeight(headerHeight)
{
    m_modes = addChild<ui::SegmentedControl>();
    m_modes->setContentHeight(28);
    m_modes->setVisible(false);
    m_modes->onCurrentChanged = [this](int index) {
        setShowsContents(index == 1);
        if (onModeChanged)
            onModeChanged();
    };
    m_list = addChild<ui::ListView>();
    m_list->setRowHeight(kRowHeight);
    m_list->setSpacing(kRowSpacing);
    m_list->createItem = [this] { return std::make_unique<Row>(*this); };
    m_list->bindItem = [](ui::View* item, size_t row) { static_cast<Row*>(item)->bind(row); };
    m_subscriptions.push_back(m_list->scrolled.connect([this] { keepVisibleThumbnails(); }));
    m_outline = addChild<ui::ListView>();
    m_outline->setVisible(false);
    m_outline->setRowHeight(kOutlineRowHeight);
    m_outline->createItem = [this] { return std::make_unique<OutlineRow>(*this); };
    m_outline->bindItem = [](ui::View* item, size_t row) { static_cast<OutlineRow*>(item)->bind(row); };
    retranslate();
}

Sidebar::~Sidebar() = default;

void Sidebar::retranslate()
{
    m_modes->setItems({tr("Thumbnails"), tr("Contents")});
    m_modes->setCurrentIndex(m_showsContents ? 1 : 0);
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
    m_open.clear();
    m_outlineRows.clear();
    m_showsContents = false;
    if (document) {
        m_subscriptions.push_back(document->thumbnailChanged.connect([this](int page) {
            if (ui::View* row = m_list->itemForRow(size_t(page)))
                row->requestPaint();
        }));
        m_subscriptions.push_back(document->outlineChanged.connect([this] { outlineChanged(); }));
    }
    m_list->setRowCount(document ? size_t(document->pageCount()) : 0);
    m_list->reload();
    outlineChanged();
}

size_t Sidebar::count() const
{
    return m_list->rowCount();
}

void Sidebar::setCurrent(int index)
{
    // The number's colour follows, set as the row lays out.
    if (m_current >= 0) {
        if (ui::View* row = m_list->itemForRow(size_t(m_current))) {
            row->requestLayout();
            row->requestPaint();
        }
    }
    m_current = index;
    if (index < 0)
        return;
    if (ui::View* row = m_list->itemForRow(size_t(index))) {
        row->requestLayout();
        row->requestPaint();
    }
    m_list->scrollToRow(size_t(index));
}

void Sidebar::refresh()
{
    m_list->reload();
}

bool Sidebar::hasContents() const
{
    return m_document && !m_document->outline().empty();
}

void Sidebar::setShowsContents(bool contents)
{
    contents = contents && hasContents();
    m_showsContents = contents;
    m_modes->setCurrentIndex(contents ? 1 : 0);
    m_list->setVisible(!contents);
    m_outline->setVisible(contents);
    requestLayout();
}

void Sidebar::outlineChanged()
{
    m_open.clear();
    flattenOutline();
    m_modes->setVisible(hasContents());
    setShowsContents(m_showsContents);
}

void Sidebar::flattenOutline()
{
    m_outlineRows.clear();
    if (m_document) {
        const auto add = [this](const auto& self, const std::vector<pdf::OutlineItem>& items, int depth) -> void {
            for (const pdf::OutlineItem& item : items) {
                m_outlineRows.push_back({&item, depth});
                if (m_open.contains(&item))
                    self(self, item.children, depth + 1);
            }
        };
        add(add, m_document->outline(), 0);
    }
    m_outline->setRowCount(m_outlineRows.size());
    m_outline->reload();
}

const std::string& Sidebar::contentsTitle(size_t row) const
{
    return m_outlineRows[row].item->title;
}

void Sidebar::toggleContentsRow(size_t row)
{
    const pdf::OutlineItem* item = m_outlineRows[row].item;
    if (!m_open.erase(item))
        m_open.insert(item);
    flattenOutline();
}

void Sidebar::activateContentsRow(size_t row)
{
    if (onDestinationActivated)
        onDestinationActivated(m_outlineRows[row].item->destination);
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
    double top = m_headerHeight + kSmallSpacing;
    if (m_modes->isVisible()) {
        const double inset = plateInset(this);
        const double height = m_modes->sizeHint().height;
        m_modes->setGeometry({inset, top, std::max(0.0, geometry().width - 2 * inset), height});
        top += height + kSmallSpacing;
    }
    const ui::RectF rest {0, top, geometry().width, std::max(0.0, geometry().height - top)};
    m_list->setGeometry(rest);
    m_outline->setGeometry(rest);
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
