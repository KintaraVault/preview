// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "application.h"
#include "icons.h"
#include "previewsettings.h"

#include "base/dbus.h"
#include "base/i18n.h"
#include "ui/application.h"
#include "ui/icon.h"

#include <chrono>
#include <string>
#include <vector>

using namespace cutefish;
using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    const std::vector<std::string> paths(argv + 1, argv + argc);

    ui::Application app(argc, argv);
    app.setApplicationId("cutefish-preview");
    i18n::setDomain("cutefish-preview");
    for (const auto& [name, svg] : embeddedIcons())
        ui::Icon::registerGlyph(std::string(name), std::string(svg));

    PreviewSettings settings(PreviewSettings::defaultPath());
    Application application(settings);
    if (base::dbus::Connection* bus = app.sessionBus()) {
        if (!application.registerService(bus)) {
            base::dbus::Proxy running(bus, Application::kService, Application::kPath, Application::kService);
            running.setTimeout(3s);
            // A Preview that does not answer leaves the files to this one.
            if (!running.call("Open", paths, app.takeActivationToken()).isError())
                return 0;
        }
    }
    application.open(paths, app.takeActivationToken());
    return app.run();
}
