// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/signal.h"
#include "ui/view.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace cutefish::pdf {
struct Destination;
struct OutlineItem;
}

namespace cutefish::ui {
class ListView;
}

class PdfDocument;

// Thumbnails of the window's items down its left edge, in the material and
// selection style of the Files sidebar; the current one is selected and
// kept in view. For a PDF the items are its pages, numbered, and a document
// with an outline gets Thumbnails and Contents at the top.
class Sidebar : public cutefish::ui::View
{
public:
    static constexpr double kWidth = 190;

    // The header's height, which the thumbnails start below.
    explicit Sidebar(double headerHeight);
    ~Sidebar() override;

    void setItems(const std::vector<std::string>& paths);
    // Its pages instead of files; null goes back to files. It outlives the
    // sidebar's use of it.
    void setDocument(PdfDocument* document);
    void setCurrent(int index);
    // Thumbnails read again, of files written since.
    void refresh();
    int current() const { return m_current; }
    size_t count() const;

    bool hasContents() const;
    bool showsContents() const { return m_showsContents; }
    void setShowsContents(bool contents);
    // The outline's rows as they are shown, an item's children under it
    // while it is open.
    size_t contentsRowCount() const { return m_outlineRows.size(); }
    const std::string& contentsTitle(size_t row) const;
    void toggleContentsRow(size_t row);
    void activateContentsRow(size_t row);
    // The thumbnail and the number of a page row in sight, in the window;
    // empty when the row is not.
    cutefish::ui::RectF pageThumbnailGeometry(int page, double ratio) const;
    cutefish::ui::RectF pageNumberGeometry(int page) const;
    cutefish::ui::RectF rowGeometry(int page) const;

    std::function<void(int index)> onActivated;
    std::function<void(const cutefish::pdf::Destination& destination)> onDestinationActivated;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    class Row;
    class OutlineRow;

    struct OutlineEntry
    {
        const cutefish::pdf::OutlineItem* item;
        int depth;
    };

    void outlineChanged();
    void flattenOutline();
    void keepVisibleThumbnails();
    // A document's rows follow its pages' shape and the window's scale.
    void updateRowHeight();

    double m_headerHeight;
    cutefish::ui::ListView* m_list;
    cutefish::ui::ListView* m_outline;
    std::vector<std::string> m_paths;
    PdfDocument* m_document = nullptr;
    std::vector<cutefish::base::Subscription> m_subscriptions;
    std::vector<OutlineEntry> m_outlineRows;
    std::set<const cutefish::pdf::OutlineItem*> m_open;
    bool m_showsContents = false;
    int m_current = -1;
    // A page thumbnail's box height, for the document's rows, from the
    // largest height to width of its pages.
    double m_pageHeight = 0;
    double m_pageAspect = 0;
};
