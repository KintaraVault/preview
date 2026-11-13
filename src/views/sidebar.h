// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/signal.h"
#include "ui/view.h"

#include <functional>
#include <string>
#include <vector>

namespace cutefish::ui {
class ListView;
}

class PdfDocument;

// Thumbnails of the window's items down its left edge, in the material and
// selection style of the Files sidebar; the current one is selected and
// kept in view.
class Sidebar : public cutefish::ui::View
{
public:
    static constexpr double kWidth = 190;

    // The header's height, which the thumbnails start below.
    explicit Sidebar(double headerHeight);

    void setItems(const std::vector<std::string>& paths);
    // Its pages instead of files; null goes back to files. It outlives the
    // sidebar's use of it.
    void setDocument(PdfDocument* document);
    void setCurrent(int index);
    // Thumbnails read again, of files written since.
    void refresh();
    int current() const { return m_current; }
    size_t count() const;

    std::function<void(int index)> onActivated;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    class Row;
    void keepVisibleThumbnails();

    double m_headerHeight;
    cutefish::ui::ListView* m_list;
    std::vector<std::string> m_paths;
    PdfDocument* m_document = nullptr;
    std::vector<cutefish::base::Subscription> m_subscriptions;
    int m_current = -1;
};
