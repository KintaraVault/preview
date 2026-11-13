// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pdfview.h"

#include "ui/animation.h"
#include "ui/clipboard.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace cutefish;
using namespace std::chrono_literals;

static constexpr int64_t kScaleSteps = 4096;
// Once the zoom stops this long, pages are rendered at it.
static constexpr auto kRestDelay = 150ms;
static constexpr double kDragDistance = 3;

static const ui::Color kPageColor {255, 255, 255, 255};
// Pages stay white in both modes, so these are the light ones.
static const ui::Color kSelectionColor = ui::Color::rgb(0x0A84FF).withAlpha(0.3);

static ui::RectF toRect(const base::RectF& rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}

// The rects of the characters from `first` to `last`, one per stretch of a line.
static std::vector<ui::RectF> rangeRects(const pdf::PageText& text, size_t first, size_t last)
{
    std::vector<ui::RectF> rects;
    last = std::min(last, text.characters.size());
    for (size_t i = first; i < last; ++i) {
        const base::RectF& box = text.characters[i].box;
        if (box.isEmpty())
            continue;
        const ui::RectF rect = toRect(box);
        if (!rects.empty()) {
            ui::RectF& line = rects.back();
            const double overlap = std::min(line.bottom(), rect.bottom()) - std::max(line.y, rect.y);
            if (overlap > 0.5 * std::min(line.height, rect.height) && rect.x >= line.x - 1
                && rect.x <= line.right() + rect.height) {
                const double left = std::min(line.x, rect.x);
                const double top = std::min(line.y, rect.y);
                line = {left, top, std::max(line.right(), rect.right()) - left,
                        std::max(line.bottom(), rect.bottom()) - top};
                continue;
            }
        }
        rects.push_back(rect);
    }
    return rects;
}

// Before or after the character nearest the point, by which half it is in;
// a character on the point's line wins over a nearer one above or below.
static size_t caretAt(const pdf::PageText& text, ui::PointF point)
{
    size_t best = text.characters.size();
    double bestDistance = std::numeric_limits<double>::max();
    for (size_t i = 0; i < text.characters.size(); ++i) {
        const base::RectF& box = text.characters[i].box;
        if (box.isEmpty())
            continue;
        const double dx = std::max({0.0, box.x() - point.x, point.x - box.right()});
        const double dy = std::max({0.0, box.y() - point.y, point.y - box.bottom()});
        const double distance = dy * 4 + dx;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    if (best == text.characters.size())
        return 0;
    const base::RectF& box = text.characters[best].box;
    return point.x > box.x() + box.width() / 2 ? best + 1 : best;
}

size_t PdfView::TileHash::operator()(const TileKey& key) const
{
    size_t hash = std::hash<int64_t>()(key.scale);
    for (const int part : {key.page, key.column, key.row})
        hash = hash * 1000003 ^ std::hash<int>()(part);
    return hash;
}

// The scrolled content: the pages and their tiles.
class PdfView::Canvas : public ui::View
{
public:
    explicit Canvas(PdfView* view)
        : m_view(view)
    {
    }

    ui::SizeF sizeHint() const override
    {
        return {m_view->contentWidth(m_view->m_zoom), m_view->contentHeight(m_view->m_zoom)};
    }
    double heightForWidth(double) const override { return sizeHint().height; }
    bool acceptsPointer() const override { return true; }

protected:
    void paint(ui::Painter& painter) override;
    void pointerPress(const ui::PointerEvent& event) override;
    void pointerMove(const ui::PointerEvent& event) override;
    void pointerRelease(const ui::PointerEvent& event) override;

private:
    // Where a press or a drag is, as a place in the text: false where the
    // page's text is not read yet.
    bool caret(ui::PointF content, TextPosition* position, const pdf::PageText** text = nullptr);

    PdfView* m_view;
    bool m_pressed = false;
    bool m_dragged = false;
    bool m_selecting = false;
    ui::PointF m_pressAt;
};

bool PdfView::Canvas::caret(ui::PointF content, TextPosition* position, const pdf::PageText** text)
{
    ui::PointF point;
    const int page = m_view->pagePoint(content, &point);
    const pdf::PageText* pageText = page >= 0 ? m_view->m_document->text(page, PdfWorker::Urgency::Now) : nullptr;
    if (!pageText)
        return false;
    *position = {page, caretAt(*pageText, point)};
    if (text)
        *text = pageText;
    return true;
}

void PdfView::Canvas::paint(ui::Painter& painter)
{
    PdfDocument* document = m_view->m_document;
    if (!document)
        return;
    const ui::PointF pan = m_view->scrollPosition();
    const double top = pan.y;
    const double bottom = pan.y + m_view->geometry().height;
    const double zoom = m_view->m_zoom;
    const int64_t sharp = std::llround(m_view->m_renderScale * kScaleSteps);
    const double line = 1 / painter.scale();
    const ui::Theme& theme = ui::Theme::current();
    const ui::Color edge = theme.text.withAlpha(theme.darkMode ? 0.2 : 0.15);
    const TextPosition start = m_view->selectionStart();
    const TextPosition end = m_view->selectionEnd();
    for (int page = m_view->pageAt(top); page < document->pageCount(); ++page) {
        const ui::RectF rect = m_view->pageRect(page);
        const ui::RectF content {rect.x + pan.x, rect.y + pan.y, rect.width, rect.height};
        if (content.y > bottom)
            break;
        if (content.bottom() < top)
            continue;
        painter.fillRect(content.adjusted(-line, -line, line, line), edge);
        painter.fillRect(content, kPageColor);
        painter.save();
        painter.clip(content);
        // Tiles of other zooms under those of this one.
        for (const bool current : {false, true}) {
            for (const Tile& tile : m_view->m_tiles) {
                if (tile.key.page != page || (tile.key.scale == sharp) != current)
                    continue;
                const double factor = zoom / tile.scale;
                painter.drawImage(tile.image.skImage(),
                                  {content.x + tile.key.column * kTileSize * factor,
                                   content.y + tile.key.row * kTileSize * factor, tile.image.width() * factor,
                                   tile.image.height() * factor});
            }
        }
        const auto fill = [&](const ui::RectF& points, ui::Color color) {
            painter.fillRect({content.x + points.x * zoom, content.y + points.y * zoom, points.width * zoom,
                              points.height * zoom},
                             color);
        };
        if (m_view->hasSelection() && page >= start.page && page <= end.page) {
            if (const pdf::PageText* text = document->text(page)) {
                const size_t first = page == start.page ? start.index : 0;
                const size_t last = page == end.page ? end.index : text->characters.size();
                for (const ui::RectF& points : rangeRects(*text, first, last))
                    fill(points, kSelectionColor);
            }
        }
        painter.restore();
    }
}

void PdfView::Canvas::pointerPress(const ui::PointerEvent& event)
{
    if (!m_view->m_document)
        return;
    if (event.button != ui::MouseButton::Left)
        return;
    m_pressed = true;
    m_dragged = false;
    m_pressAt = event.position;
    TextPosition position;
    const pdf::PageText* text = nullptr;
    m_selecting = caret(event.position, &position, &text);
    if (!m_selecting) {
        m_view->clearSelection();
        return;
    }
    size_t first = position.index;
    size_t last = position.index;
    if (event.clickCount == 1 && (event.modifiers & ui::ShiftModifier) && m_view->hasSelection())
        m_view->setSelection(m_view->m_anchor, position);
    else
        m_view->setSelection({position.page, first}, {position.page, last});
}

void PdfView::Canvas::pointerMove(const ui::PointerEvent& event)
{
    if (!m_pressed)
        return;
    if (std::hypot(event.position.x - m_pressAt.x, event.position.y - m_pressAt.y) > kDragDistance)
        m_dragged = true;
    TextPosition position;
    if (m_dragged && m_selecting && caret(event.position, &position))
        m_view->setSelection(m_view->m_anchor, position);
}

void PdfView::Canvas::pointerRelease(const ui::PointerEvent& event)
{
    if (!m_pressed)
        return;
    m_pressed = false;
}

PdfView::PdfView()
{
    setScrollDirections(true, true);
    setClipsChildren(true);
    setTracksHover(true);
    m_canvas = setContent<Canvas>(this);

    m_restTimer.setSingleShot(true);
    m_restTimer.setInterval(kRestDelay);
    m_restTimer.onTimeout = [this] { rest(); };
}

PdfView::~PdfView() = default;

void PdfView::setDocument(PdfDocument* document)
{
    m_subscriptions.clear();
    m_pendingTiles.clear();
    m_tiles.clear();
    m_tileIndex.clear();
    m_tileBytes = 0;
    m_document = document;
    m_heightsBefore.clear();
    m_widest = 0;
    m_anchor = m_focus = {};
    m_currentPage = 0;
    m_fitted = true;
    m_renderScale = 0;
    if (document) {
        double height = 0;
        for (const ui::SizeF& size : document->pageSizes()) {
            m_heightsBefore.push_back(height);
            height += size.height;
            m_widest = std::max(m_widest, size.width);
        }
        m_heightsBefore.push_back(height);
        m_subscriptions.push_back(document->pageDataChanged.connect([this](int) { m_canvas->requestPaint(); }));
    }
    m_zoom = fitZoom();
    m_lastSize = {};
    m_canvas->requestLayout();
    requestLayout();
    setScrollPosition({0, 0});
}

double PdfView::ratio() const
{
    return window() && window()->scale() > 0 ? window()->scale() : 1;
}

double PdfView::snap(double value) const
{
    return std::round(value * ratio()) / ratio();
}

double PdfView::fitZoom() const
{
    const double width = geometry().width - 2 * kMargin;
    if (m_widest <= 0 || width <= 0)
        return 1;
    return width / m_widest;
}

double PdfView::minimumZoom() const
{
    return std::min(fitZoom(), 1.0);
}

double PdfView::pageTop(int page, double zoom) const
{
    return kMargin + m_heightsBefore[size_t(page)] * zoom + page * kPageGap;
}

double PdfView::pageLeft(int page, double zoom) const
{
    return (contentWidth(zoom) - m_document->pageSizes()[size_t(page)].width * zoom) / 2;
}

double PdfView::contentWidth(double zoom) const
{
    return std::max(geometry().width, m_widest * zoom + 2 * kMargin);
}

double PdfView::contentHeight(double zoom) const
{
    if (!m_document)
        return geometry().height;
    const int count = m_document->pageCount();
    return std::max(geometry().height, 2 * kMargin + m_heightsBefore.back() * zoom + (count - 1) * kPageGap);
}

int PdfView::pageAt(double y) const
{
    if (!m_document || m_document->pageCount() == 0)
        return 0;
    int low = 0;
    int high = m_document->pageCount() - 1;
    while (low < high) {
        const int middle = (low + high + 1) / 2;
        if (pageTop(middle, m_zoom) <= y)
            low = middle;
        else
            high = middle - 1;
    }
    return low;
}

int PdfView::pagePoint(ui::PointF content, ui::PointF* point) const
{
    if (!m_document || m_document->pageCount() == 0)
        return -1;
    int page = pageAt(content.y);
    // In the gap under a page, the next one is nearer.
    const double bottom = pageTop(page, m_zoom) + m_document->pageSizes()[size_t(page)].height * m_zoom;
    if (content.y > bottom + kPageGap / 2 && page + 1 < m_document->pageCount())
        ++page;
    *point = {(content.x - snap(pageLeft(page, m_zoom))) / m_zoom, (content.y - snap(pageTop(page, m_zoom))) / m_zoom};
    return page;
}

ui::RectF PdfView::pageRect(int page) const
{
    if (!m_document || page < 0 || page >= m_document->pageCount())
        return {};
    const ui::SizeF size = m_document->pageSizes()[size_t(page)];
    const ui::PointF pan = scrollPosition();
    return {snap(pageLeft(page, m_zoom)) - pan.x, snap(pageTop(page, m_zoom)) - pan.y, size.width * m_zoom,
            size.height * m_zoom};
}

ui::PointF PdfView::viewPoint(int page, ui::PointF point) const
{
    const ui::RectF rect = pageRect(page);
    return {rect.x + point.x * m_zoom, rect.y + point.y * m_zoom};
}

size_t PdfView::sharpTileCount() const
{
    const int64_t sharp = std::llround(m_renderScale * kScaleSteps);
    return size_t(std::count_if(m_tiles.begin(), m_tiles.end(), [sharp](const Tile& tile) { return tile.key.scale == sharp; }));
}

bool PdfView::hasPendingTiles() const
{
    return std::any_of(m_pendingTiles.begin(), m_pendingTiles.end(),
                       [](const auto& entry) { return entry.second.isPending(); });
}

void PdfView::setPosition(ui::PointF position)
{
    setScrollPosition(position);
}

void PdfView::zoomTo(double target, ui::PointF at, bool animate)
{
    if (!m_document)
        return;
    target = std::clamp(target, minimumZoom(), maximumZoom());
    m_anchorPage = pagePoint(scrollPosition() + at, &m_anchorPoint);
    m_anchorView = at;
    m_fitted = std::abs(target - fitZoom()) < 0.0001;
    if (m_animation)
        m_animation->stop();
    m_anchored = true;
    if (animate && std::abs(target - m_zoom) > 0.0001 && window()) {
        if (!m_animation) {
            m_animation = std::make_unique<ui::Animation>(window());
            m_animation->setDuration(200ms);
            m_animation->setEasing(ui::Animation::Easing::OutCubic);
            m_animation->onUpdate = [this](double value) {
                setZoom(m_animationFrom + (m_animationTo - m_animationFrom) * value);
            };
        }
        m_animationFrom = m_zoom;
        m_animationTo = target;
        m_animation->start();
    } else {
        setZoom(target);
    }
}

// Steps compound on an animation's target, so quick wheel clicks add up.
void PdfView::zoomBy(double factor, ui::PointF at, bool animate)
{
    const double base = m_animation && m_animation->isRunning() ? m_animationTo : m_zoom;
    zoomTo(base * factor, at, animate);
}

void PdfView::setZoom(double zoom)
{
    m_zoom = zoom;
    m_canvas->requestLayout();
    // Laid out now, so the position below is clamped against the new content.
    ScrollView::layout();
    if (m_anchored && m_anchorPage >= 0) {
        setPosition({snap(pageLeft(m_anchorPage, m_zoom)) + m_anchorPoint.x * m_zoom - m_anchorView.x,
                     snap(pageTop(m_anchorPage, m_zoom)) + m_anchorPoint.y * m_zoom - m_anchorView.y});
    } else {
        setPosition(scrollPosition());
    }
    m_canvas->requestPaint();
    m_restTimer.start();
}

void PdfView::rest()
{
    if (!m_document)
        return;
    m_renderScale = m_zoom * ratio();
    updateTiles();
    m_canvas->requestPaint();
}

void PdfView::layout()
{
    const ui::SizeF size = geometry().size();
    if (size != m_lastSize) {
        m_lastSize = size;
        if (m_fitted) {
            m_zoom = fitZoom();
            m_canvas->requestLayout();
            m_restTimer.start();
        }
    }
    ScrollView::layout();
    if (!m_document || size.width <= 0)
        return;
    // The first pages at once; later changes once they come to rest.
    if (m_renderScale <= 0)
        rest();
    else if (std::abs(m_renderScale - m_zoom * ratio()) > 1e-9 && !m_restTimer.isActive())
        m_restTimer.start();
    updateCurrentPage();
}

void PdfView::positionChanged()
{
    if (!m_document)
        return;
    updateCurrentPage();
    updateTiles();
}

void PdfView::updateCurrentPage()
{
    if (!m_document || m_keepCurrentPage)
        return;
    const double top = scrollPosition().y;
    const double bottom = top + geometry().height;
    int best = m_currentPage;
    double bestHeight = -1;
    for (int page = pageAt(top); page < m_document->pageCount(); ++page) {
        const double pageTopY = pageTop(page, m_zoom);
        if (pageTopY > bottom)
            break;
        const double shown = std::min(bottom, pageTopY + m_document->pageSizes()[size_t(page)].height * m_zoom)
                - std::max(top, pageTopY);
        if (shown > bestHeight + 0.5) {
            bestHeight = shown;
            best = page;
        }
    }
    if (best == m_currentPage)
        return;
    m_currentPage = best;
    if (onCurrentPageChanged)
        onCurrentPageChanged();
}

void PdfView::goToPage(int page)
{
    if (!m_document)
        return;
    page = std::clamp(page, 0, m_document->pageCount() - 1);
    m_keepCurrentPage = true;
    setPosition({scrollPosition().x, snap(pageTop(page, m_zoom)) - kMargin});
    m_keepCurrentPage = false;
    if (page != m_currentPage) {
        m_currentPage = page;
        if (onCurrentPageChanged)
            onCurrentPageChanged();
    }
}

void PdfView::scrollScreen(int direction)
{
    const ui::PointF position = scrollPosition();
    setPosition({position.x, position.y + direction * std::max(1.0, geometry().height - 40)});
}

void PdfView::setSelection(TextPosition anchor, TextPosition focus)
{
    if (anchor == m_anchor && focus == m_focus)
        return;
    m_anchor = anchor;
    m_focus = focus;
    m_canvas->requestPaint();
    if (onSelectionChanged)
        onSelectionChanged();
}

void PdfView::selectAll()
{
    if (m_document)
        setSelection({0, 0}, {m_document->pageCount() - 1, std::numeric_limits<size_t>::max()});
}

void PdfView::clearSelection()
{
    setSelection({}, {});
}

void PdfView::copy()
{
    if (!m_document || !hasSelection())
        return;
    m_copy = m_document->readText(selectionStart(), selectionEnd(), [](std::string text) {
        if (!text.empty())
            ui::Clipboard::setText(text);
    });
}

void PdfView::updateTiles()
{
    if (!m_document || m_renderScale <= 0 || geometry().width <= 0 || geometry().height <= 0)
        return;
    const double scale = m_renderScale;
    const int64_t scaleKey = std::llround(scale * kScaleSteps);
    const ui::PointF pan = scrollPosition();
    const ui::RectF shown {pan.x, pan.y, geometry().width, geometry().height};
    const double nearTop = shown.y - shown.height / 2;
    const double nearBottom = shown.bottom() + shown.height / 2;
    std::unordered_map<TileKey, PdfRequest, TileHash> pending;
    int firstShown = -1;
    int lastShown = -1;
    for (int page = pageAt(std::max(0.0, nearTop)); page < m_document->pageCount(); ++page) {
        const ui::SizeF size = m_document->pageSizes()[size_t(page)];
        const double top = snap(pageTop(page, m_zoom));
        const double left = snap(pageLeft(page, m_zoom));
        const double bottom = top + size.height * m_zoom;
        if (top > nearBottom)
            break;
        if (bottom < nearTop)
            continue;
        const bool inSight = top < shown.bottom() && bottom > shown.y;
        if (inSight) {
            if (firstShown < 0)
                firstShown = page;
            lastShown = page;
            m_document->text(page);
        }
        const int pixelWidth = int(std::ceil(size.width * scale));
        const int pixelHeight = int(std::ceil(size.height * scale));
        // The part of the page wanted, in its pixels at the scale.
        const auto pixels = [&](double content, double origin, int limit) {
            return std::clamp((content - origin) / m_zoom * scale, 0.0, double(limit));
        };
        const int firstColumn = int(pixels(shown.x, left, pixelWidth)) / kTileSize;
        const int lastColumn = int(std::ceil(pixels(shown.right(), left, pixelWidth))) / kTileSize;
        const int firstRow = int(pixels(std::max(nearTop, top), top, pixelHeight)) / kTileSize;
        const int lastRow = int(std::ceil(pixels(std::min(nearBottom, bottom), top, pixelHeight))) / kTileSize;
        for (int row = firstRow; row <= lastRow && row * kTileSize < pixelHeight; ++row) {
            const double rowTop = top + row * kTileSize / scale * m_zoom;
            const double rowBottom = rowTop + kTileSize / scale * m_zoom;
            const bool rowInSight = rowTop < shown.bottom() && rowBottom > shown.y;
            for (int column = firstColumn; column <= lastColumn && column * kTileSize < pixelWidth; ++column) {
                const TileKey key {page, scaleKey, column, row};
                const auto kept = m_tileIndex.find(key);
                if (kept != m_tileIndex.end()) {
                    m_tiles.splice(m_tiles.begin(), m_tiles, kept->second);
                    continue;
                }
                const auto asked = m_pendingTiles.find(key);
                if (asked != m_pendingTiles.end() && asked->second.isPending()) {
                    pending.emplace(key, std::move(asked->second));
                    continue;
                }
                const base::Rect region(column * kTileSize, row * kTileSize,
                                        std::min(kTileSize, pixelWidth - column * kTileSize),
                                        std::min(kTileSize, pixelHeight - row * kTileSize));
                pending.emplace(key, m_document->worker().run(
                                             rowInSight ? PdfWorker::Urgency::Visible : PdfWorker::Urgency::Near,
                                             [page, scale, region](const pdf::Document* document,
                                                                   const std::atomic<bool>* cancelled) {
                                                 pdf::Bitmap bitmap = document->render(page, scale, region, cancelled);
                                                 return ui::RasterImage(bitmap.width, bitmap.height,
                                                                        std::move(bitmap.pixels), true);
                                             },
                                             [this, key, scale](ui::RasterImage image) {
                                                 m_pendingTiles.erase(key);
                                                 if (!image.isNull())
                                                     tileArrived({key, scale, std::move(image)});
                                             }));
            }
        }
    }
    // What scrolled away is not drawn after all.
    m_pendingTiles = std::move(pending);
    if (firstShown >= 0)
        m_document->keepPageRequests(firstShown, lastShown);
}

void PdfView::tileArrived(Tile tile)
{
    m_tileBytes += tile.image.sizeInBytes();
    const TileKey key = tile.key;
    m_tiles.push_front(std::move(tile));
    m_tileIndex[key] = m_tiles.begin();
    while (m_tileBytes > kTileBytes && m_tiles.size() > 1) {
        m_tileBytes -= m_tiles.back().image.sizeInBytes();
        m_tileIndex.erase(m_tiles.back().key);
        m_tiles.pop_back();
    }
    m_canvas->requestPaint();
}

bool PdfView::scroll(const ui::ScrollEvent& event)
{
    if (!(event.modifiers & ui::ControlModifier)) {
        // A pan ends a running zoom, or its anchor would pull the view back.
        if (m_animation && m_animation->isRunning()) {
            m_animation->stop();
            setZoom(m_animationTo);
        }
        m_anchored = false;
        return ScrollView::scroll(event);
    }
    if (event.value120Y != 0)
        zoomBy(std::pow(1.25, -event.value120Y / 120.0), m_pointer, true);
    else if (event.deltaY != 0)
        zoomBy(std::pow(1.006, -event.deltaY), m_pointer, false);
    return true;
}

bool PdfView::gesture(const ui::GestureEvent& event)
{
    if (event.type == ui::GestureEvent::Type::PinchBegin) {
        m_pinchScale = 1;
        return true;
    }
    if (event.type == ui::GestureEvent::Type::PinchUpdate) {
        const double delta = event.scale / m_pinchScale;
        m_pinchScale = event.scale;
        zoomTo(m_zoom * delta, m_pointer, false);
        return true;
    }
    return event.type == ui::GestureEvent::Type::PinchEnd;
}

void PdfView::hoverMove(const ui::PointerEvent& event)
{
    m_pointer = event.position;
}
