// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "imagesequence.h"

#include "base/mime_database.h"
#include "ui/image_info.h"

#include <algorithm>
#include <filesystem>

using namespace cutefish;
namespace fs = std::filesystem;

ImageSequence::ImageSequence(std::vector<std::string> paths, int index)
    : m_paths(std::move(paths))
    , m_index(index)
{
}

std::unique_ptr<ImageSequence> ImageSequence::files(std::vector<std::string> paths)
{
    return std::unique_ptr<ImageSequence>(new ImageSequence(std::move(paths), 0));
}

bool ImageSequence::isImage(const std::string& path)
{
    const std::vector<std::string>& types = ui::imageMimeTypes();
    return std::find(types.begin(), types.end(), base::mime::typeForFileName(fs::path(path).filename().string()))
            != types.end();
}

std::string ImageSequence::currentPath() const
{
    return m_paths.empty() ? std::string() : m_paths[size_t(m_index)];
}

void ImageSequence::setCurrentIndex(int index)
{
    index = std::clamp(index, 0, std::max(0, count() - 1));
    if (index == m_index)
        return;
    m_index = index;
    changed.emit();
}
