// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>
#include <vector>

// What a window shows; the sidebar is kept per kind.
enum class DocumentKind {
    Image,
};

// preview.conf: whether the sidebar shows, per kind.
class PreviewSettings
{
public:
    explicit PreviewSettings(std::string file);

    // ~/.config/cutefish/preview.conf.
    static std::string defaultPath();

    bool sidebarShown(DocumentKind kind) const;
    void setSidebarShown(DocumentKind kind, bool shown);

private:
    std::string m_file;
};
