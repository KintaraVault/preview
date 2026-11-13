// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <memory>
#include <string>
#include <vector>

class PreviewSettings;
class PreviewWindow;

// The process: its windows and the settings they share.
class Application
{
public:
    explicit Application(PreviewSettings& settings);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    PreviewSettings& settings() const { return m_settings; }
    const std::vector<std::unique_ptr<PreviewWindow>>& windows() const { return m_windows; }

    // Local paths, each request in new windows, or one still on its welcome
    // page: the images in one, every other file in its own. A file a window
    // shows already raises that window instead. No paths, a welcome page.
    // The token activates the window, for a request from another process.
    void open(const std::vector<std::string>& paths, const std::string& activationToken = {});
    // Destroys a closed window after the event it closed in; the last one
    // ends the process.
    void releaseWindow(PreviewWindow* window);
    void quit();

private:
    PreviewWindow* windowShowing(const std::string& path) const;
    // A window on its welcome page, or a new one.
    PreviewWindow* freeWindow();

    PreviewSettings& m_settings;
    std::vector<std::unique_ptr<PreviewWindow>> m_windows;
};
