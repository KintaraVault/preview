// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "imagesequence.h"

#include "base/collator.h"
#include "base/mime_database.h"
#include "ui/image_info.h"

#include <algorithm>
#include <filesystem>

using namespace cutefish;
namespace fs = std::filesystem;

// The folder's images and `own`, sorted as the folder's videos are queued.
static std::vector<std::string> folderImages(const fs::path& folder, const std::string& own)
{
    std::vector<std::string> names;
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
        std::error_code entryError;
        if (!entry.is_regular_file(entryError))
            continue;
        const std::string name = entry.path().filename().string();
        if (name == own || (!name.starts_with('.') && ImageSequence::isImage(name)))
            names.push_back(name);
    }
    base::Collator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitive(false);
    std::sort(names.begin(), names.end(), collator);

    std::vector<std::string> paths;
    paths.reserve(names.size());
    for (const std::string& name : names)
        paths.push_back((folder / name).string());
    return paths;
}

ImageSequence::ImageSequence(std::vector<std::string> paths, int index)
    : m_paths(std::move(paths))
    , m_index(index)
{
}

std::unique_ptr<ImageSequence> ImageSequence::folderOf(const std::string& path)
{
    const fs::path file = fs::path(path).lexically_normal();
    std::vector<std::string> paths = folderImages(file.parent_path(), file.filename().string());
    const auto found = std::find(paths.begin(), paths.end(), file.string());
    const int index = found == paths.end() ? 0 : int(found - paths.begin());
    return std::unique_ptr<ImageSequence>(new ImageSequence(std::move(paths), index));
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
