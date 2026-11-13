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

class Clickable;
class RecentRow;

// A window without a file: how to open one, and the files opened recently;
// scrolls when the window is too short for them.
class WelcomePage : public cutefish::ui::ScrollView
{
public:
    WelcomePage();

    // The recent files, newest first; five of them are listed.
    void setRecentFiles(const std::vector<std::string>& paths);
    void retranslate();

    std::function<void()> onOpen;
    std::function<void(const std::string& path)> onRecentClicked;
    std::function<void()> onClearRecent;

private:
    class Column;

    double stackHeight() const;
    void layoutColumn(double pageWidth, double pageHeight);

    Column* m_column;
    cutefish::ui::ThemeIcon* m_icon;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_subtitle;
    cutefish::ui::DialogButton* m_open;
    cutefish::ui::Label* m_recentLabel;
    Clickable* m_clear;
    cutefish::ui::Label* m_clearLabel;
    cutefish::ui::View* m_card;
    std::vector<RecentRow*> m_rows;
};
