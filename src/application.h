// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace cutefish::base::dbus {
class Connection;
class Object;
}

class PreviewSettings;
class PreviewWindow;
class TurnSaver;

// The process: its windows, the settings they share, and the D-Bus service
// later launches hand their files to.
class Application
{
public:
    static constexpr const char* kService = "org.cutefish.Preview";
    static constexpr const char* kPath = "/Preview";

    explicit Application(PreviewSettings& settings);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    PreviewSettings& settings() const { return m_settings; }
    TurnSaver& turnSaver() const { return *m_turnSaver; }
    const std::vector<std::unique_ptr<PreviewWindow>>& windows() const { return m_windows; }

    // Local paths, each request in new windows, or one still on its welcome
    // page: the images in one, every other file in its own. A file a window
    // shows already raises that window instead. No paths, a welcome page.
    // The token activates the window, for a request from another process.
    // A file joins the recent files once a window displays it.
    void open(const std::vector<std::string>& paths, const std::string& activationToken = {});
    // Destroys a closed window after the event it closed in; the last one
    // ends the process.
    void releaseWindow(PreviewWindow* window);
    // Once every turn asked for is written.
    void quit();

    void showInFiles(const std::string& path) const;

    // Publishes org.cutefish.Preview at /Preview; false when another process
    // has the name.
    bool registerService(cutefish::base::dbus::Connection* bus);

private:
    PreviewWindow* windowShowing(const std::string& path) const;
    // A window on its welcome page, or a new one.
    PreviewWindow* freeWindow();

    PreviewSettings& m_settings;
    // Before the windows, which hold on to it, and outliving them.
    std::unique_ptr<TurnSaver> m_turnSaver;
    std::vector<std::unique_ptr<PreviewWindow>> m_windows;
    std::unique_ptr<cutefish::base::dbus::Object> m_service;
};
