// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/scroll_view.h"

#include <functional>
#include <string>
#include <vector>

namespace cutefish::ui {
class DialogButton;
class Label;
class ThemeIcon;
}

// A window without a file: how to open one; scrolls when the window is too
// short for it.
class WelcomePage : public cutefish::ui::ScrollView
{
public:
    WelcomePage();

    void retranslate();

    std::function<void()> onOpen;

private:
    class Column;

    double stackHeight() const;
    void layoutColumn(double pageWidth, double pageHeight);

    Column* m_column;
    cutefish::ui::ThemeIcon* m_icon;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_subtitle;
    cutefish::ui::DialogButton* m_open;
};
