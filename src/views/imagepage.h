// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/thread_pool.h"
#include "ui/image_info.h"
#include "ui/view.h"
#include "ui/zoomable_image_view.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace cutefish::ui {
class ImageView;
}

class FileCard;

// A file as the page shows it, read on a worker.
struct ShownFile
{
    std::string path;
    std::string name;
    std::string mimeType;
    std::string size;
    cutefish::ui::ImageInfo info;
};

// The zoomable image.
class ImageViewer : public cutefish::ui::ZoomableImageView
{
};

// The current file under the header. An image replaces the one shown once
// it is decoded at the size it is shown at, so the step that brings it
// changes everything in one frame; its neighbours are decoded ahead. A file
// that is not an image gets a card with its icon, name and size.
class ImagePage : public cutefish::ui::View
{
public:
    ImagePage();
    ~ImagePage() override;

    // Reads the file's header on a worker and decodes it to fit
    // viewportFor(), then shows it.
    void load(const std::string& path);
    // Decodes these ahead, to fit the page as it is now.
    void preload(const std::vector<std::string>& paths);
    void clear();

    // What is on screen; it trails load() while the next image decodes.
    const ShownFile& shown() const { return m_shown; }
    bool showsImage() const { return m_shown.info.readable; }
    ImageViewer* viewer() const { return m_viewer; }

    // The page's size when it shows the image, for the first image, which
    // sizes the window.
    std::function<cutefish::ui::SizeF(const cutefish::ui::ImageInfo& info)> viewportFor;
    // What is shown changed, before the frame that shows it.
    std::function<void()> onShown;

protected:
    void layout() override;

private:
    void stage(ShownFile file);
    void present(ShownFile file);
    double ratio() const;
    // The image's decode in device pixels when it fits the viewport.
    cutefish::ui::SizeF fittedPixels(const cutefish::ui::ImageInfo& info, cutefish::ui::SizeF viewport) const;
    static bool decodesAhead(const cutefish::ui::ImageInfo& info);

    ImageViewer* m_viewer;
    FileCard* m_card;
    // Decode into the image cache: the image staged, and the neighbours.
    cutefish::ui::ImageView* m_preloader;
    std::vector<cutefish::ui::ImageView*> m_ahead;
    ShownFile m_shown;
    std::optional<ShownFile> m_pending;
    cutefish::base::Job m_describe;
    std::vector<cutefish::base::Job> m_aheadJobs;
};
