// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

#include <functional>
#include <string>
#include <vector>

namespace cutefish::ui {
class ListView;
}

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
    void setCurrent(int index);
    int current() const { return m_current; }

    std::function<void(int index)> onActivated;

protected:
    void layout() override;
    void paint(cutefish::ui::Painter& painter) override;

private:
    class Row;

    double m_headerHeight;
    cutefish::ui::ListView* m_list;
    std::vector<std::string> m_paths;
    int m_current = -1;
};
