// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "imagepage.h"

#include "base/mime_database.h"
#include "ui/cell_layout.h"
#include "ui/image_view.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace cutefish;
using ui::rounding::centeredStart;

static ShownFile describe(const std::string& path)
{
    ShownFile file;
    file.path = path;
    file.name = std::filesystem::path(path).filename().string();
    file.mimeType = base::mime::typeForFile(path);
    file.info = ui::readImageInfo(path, file.mimeType);
    return file;
}

static ui::ImageView* addPreloader(ui::View* parent)
{
    // Its source, size, orientation and fill mode are the viewer's: together
    // they are the image cache's key.
    auto* preloader = parent->addChild<ui::ImageView>();
    preloader->setFillMode(ui::ImageView::FillMode::PreserveAspectFit);
    preloader->setAutoTransform(true);
    preloader->setOpacity(0);
    return preloader;
}

ImagePage::ImagePage()
{
    m_viewer = addChild<ImageViewer>();
    m_preloader = addPreloader(this);
    for (int i = 0; i < 2; ++i)
        m_ahead.push_back(addPreloader(this));
    m_aheadJobs.resize(m_ahead.size());
}

ImagePage::~ImagePage() = default;

void ImagePage::load(const std::string& path)
{
    m_describe = base::ThreadPool::global().run([path] { return describe(path); },
                                                [this](ShownFile file) { stage(std::move(file)); });
}

void ImagePage::preload(const std::vector<std::string>& paths)
{
    for (size_t i = 0; i < m_ahead.size(); ++i) {
        ui::ImageView* ahead = m_ahead[i];
        if (i >= paths.size()) {
            m_aheadJobs[i].cancel();
            ahead->setSource({});
            continue;
        }
        m_aheadJobs[i] = base::ThreadPool::global().run(
                [path = paths[i]] { return describe(path); },
                [this, ahead](ShownFile file) {
                    if (!decodesAhead(file.info))
                        return;
                    const ui::SizeF pixels = fittedPixels(file.info, geometry().size());
                    ahead->setSourceSize({pixels.width / ratio(), pixels.height / ratio()});
                    ahead->setSource(file.path);
                    requestLayout();
                });
    }
}

void ImagePage::clear()
{
    m_describe.cancel();
    for (base::Job& job : m_aheadJobs)
        job.cancel();
    for (ui::ImageView* ahead : m_ahead)
        ahead->setSource({});
    m_pending.reset();
    m_preloader->onStatusChanged = nullptr;
    m_preloader->setSource({});
    m_shown = {};
    m_viewer->setImage({}, {}, {}, ratio());
}

double ImagePage::ratio() const
{
    return window() && window()->scale() > 0 ? window()->scale() : 1;
}

bool ImagePage::decodesAhead(const ui::ImageInfo& info)
{
    return info.readable && !info.animated && info.width > 0 && info.height > 0;
}

ui::SizeF ImagePage::fittedPixels(const ui::ImageInfo& info, ui::SizeF viewport) const
{
    const double zoom = std::min({1.0, viewport.width * ratio() / info.width, viewport.height * ratio() / info.height});
    return {std::ceil(info.width * zoom), std::ceil(info.height * zoom)};
}

void ImagePage::stage(ShownFile file)
{
    if (!decodesAhead(file.info)) {
        present(std::move(file));
        return;
    }
    const ui::SizeF pixels = fittedPixels(file.info, viewportFor ? viewportFor(file.info) : geometry().size());
    m_pending = file;
    m_preloader->onStatusChanged = [this](ui::ImageView::Status status) {
        if (m_pending && status != ui::ImageView::Status::Loading)
            present(*m_pending);
    };
    m_preloader->setSourceSize({pixels.width / ratio(), pixels.height / ratio()});
    m_preloader->setSource(file.path);
    requestLayout();
    if (m_preloader->status() != ui::ImageView::Status::Loading)
        present(std::move(file));
}

// By value: callers pass *m_pending, which is reset below.
void ImagePage::present(ShownFile file)
{
    m_pending.reset();
    const ui::SizeF prepared = decodesAhead(file.info)
            ? fittedPixels(file.info, viewportFor ? viewportFor(file.info) : geometry().size())
            : ui::SizeF();
    m_shown = std::move(file);
    m_viewer->setImage(m_shown.info.readable ? m_shown.path : std::string(), m_shown.info, prepared, ratio());
    m_preloader->onStatusChanged = nullptr;
    m_preloader->setSource({});
    requestLayout();
    if (onShown)
        onShown();
}

void ImagePage::layout()
{
    const ui::RectF rect = localRect();
    m_viewer->setGeometry(rect);
    m_preloader->setGeometry({0, 0, 1, 1});
    for (ui::ImageView* ahead : m_ahead)
        ahead->setGeometry({0, 0, 1, 1});
}
