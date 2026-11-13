// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "previewsettings.h"

#include "base/paths.h"
#include "base/settings.h"

using namespace cutefish;

static std::string sidebarKey(DocumentKind kind)
{
    switch (kind) {
    case DocumentKind::Image:
        return "Sidebar/Images";
    }
    return {};
}

PreviewSettings::PreviewSettings(std::string file)
    : m_file(std::move(file))
{
}

std::string PreviewSettings::defaultPath()
{
    return base::paths::cutefishConfig("preview");
}

bool PreviewSettings::sidebarShown(DocumentKind kind) const
{
    return base::Settings(m_file).boolValue(sidebarKey(kind), false);
}

void PreviewSettings::setSidebarShown(DocumentKind kind, bool shown)
{
    base::Settings settings(m_file);
    settings.setValue(sidebarKey(kind), shown);
    settings.sync();
}
