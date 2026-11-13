// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "pdfdocument.h"

#include "base/signal.h"
#include "base/timer.h"
#include "ui/raster_image.h"
#include "ui/scroll_view.h"

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cutefish::ui {
class Animation;
}

// The pages of a PDF in one column on the window's background, scrolled
// vertically and zoomed as an image is, the keys about the middle. Zoom 1
// is one point to a logical
// pixel; a document opens fitted to the view's width and follows it until
// the user zooms. Pages are drawn in tiles of 512 device pixels rendered on
// the document's worker at the zoom things come to rest at, the ones in
// sight first, then half a screen above and below; while a zoom
// moves, the tiles there are scaled. At most 128 MiB of tiles stay, the
// least recently wanted going first.
class PdfView : public cutefish::ui::ScrollView
{
public:
    static constexpr double kMargin = 12;
    static constexpr double kPageGap = 12;
    static constexpr int kTileSize = 512;
    static constexpr size_t kTileBytes = size_t(128) * 1024 * 1024;

    PdfView();
    ~PdfView() override;

    // Ready, or null; it outlives the view.
    void setDocument(PdfDocument* document);
    PdfDocument* document() const { return m_document; }

    double zoom() const { return m_zoom; }
    double fitZoom() const;
    bool isFitted() const { return m_fitted; }
    void zoomBy(double factor, cutefish::ui::PointF at, bool animate);
    void zoomTo(double target, cutefish::ui::PointF at, bool animate);
    void zoomToFit() { zoomTo(fitZoom(), center(), true); }
    void zoomToActualSize() { zoomTo(1, center(), true); }
    cutefish::ui::PointF center() const { return {geometry().width / 2, geometry().height / 2}; }
    // Device pixels per point that the tiles drawn last were asked at.
    double renderScale() const { return m_renderScale; }

    // The page most of the view shows, from 0.
    int currentPage() const { return m_currentPage; }
    // The page's top at the view's top.
    void goToPage(int page);
    // A screenful down, or up.
    void scrollScreen(int direction);

    // In the view's coordinates.
    cutefish::ui::RectF pageRect(int page) const;
    cutefish::ui::PointF viewPoint(int page, cutefish::ui::PointF point) const;
    // How many tiles are kept, and of those at the current render scale.
    size_t tileCount() const { return m_tiles.size(); }
    size_t sharpTileCount() const;
    bool hasPendingTiles() const;

    std::function<void()> onCurrentPageChanged;

protected:
    void layout() override;
    void positionChanged() override;

private:
    class Canvas;

    struct TileKey
    {
        int page;
        // The scale in 1/4096ths of a pixel per point.
        int64_t scale;
        int column;
        int row;

        bool operator==(const TileKey&) const = default;
    };
    struct TileHash
    {
        size_t operator()(const TileKey& key) const;
    };
    struct Tile
    {
        TileKey key;
        double scale;
        cutefish::ui::RasterImage image;
    };

    // The page's top and left in the content at the zoom.
    double pageTop(int page, double zoom) const;
    double pageLeft(int page, double zoom) const;
    double contentWidth(double zoom) const;
    double contentHeight(double zoom) const;
    // The page at a height of the content, the nearest one in a gap.
    int pageAt(double y) const;
    // A point of the content as a page and a point on it.
    int pagePoint(cutefish::ui::PointF content, cutefish::ui::PointF* point) const;
    double ratio() const;
    double snap(double value) const;
    double minimumZoom() const;
    double maximumZoom() const { return std::max(fitZoom(), 8.0); }
    void setZoom(double zoom);
    void setPosition(cutefish::ui::PointF position);
    void updateCurrentPage();
    void rest();
    // Asks for the tiles, text and links of what is in sight and near it.
    void updateTiles();
    void tileArrived(Tile tile);

    PdfDocument* m_document = nullptr;
    std::vector<cutefish::base::Subscription> m_subscriptions;
    Canvas* m_canvas;
    // The sum of the heights before each page, in points.
    std::vector<double> m_heightsBefore;
    double m_widest = 0;
    double m_zoom = 1;
    bool m_fitted = true;
    cutefish::ui::SizeF m_lastSize;
    double m_renderScale = 0;
    int m_currentPage = 0;
    // goToPage() says which page is current, whatever shows most.
    bool m_keepCurrentPage = false;

    // Most recently wanted first.
    std::list<Tile> m_tiles;
    std::unordered_map<TileKey, std::list<Tile>::iterator, TileHash> m_tileIndex;
    size_t m_tileBytes = 0;
    std::unordered_map<TileKey, PdfRequest, TileHash> m_pendingTiles;
    cutefish::base::Timer m_restTimer;

    bool m_anchored = false;
    cutefish::ui::PointF m_anchorView;
    int m_anchorPage = 0;
    cutefish::ui::PointF m_anchorPoint;
    double m_animationFrom = 1;
    double m_animationTo = 1;
    std::unique_ptr<cutefish::ui::Animation> m_animation;
};
