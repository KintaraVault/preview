// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/image.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

// A folder of its own under /tmp, removed with everything in it.
class TemporaryDir
{
public:
    TemporaryDir()
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "preview-test-XXXXXX").string();
        if (::mkdtemp(pattern.data()))
            m_path = pattern;
    }

    ~TemporaryDir()
    {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

    TemporaryDir(const TemporaryDir&) = delete;
    TemporaryDir& operator=(const TemporaryDir&) = delete;

    std::string filePath(const std::string& name) const { return m_path + "/" + name; }

private:
    std::string m_path;
};

inline std::string writeFile(const std::string& path, const std::string& contents = "text\n")
{
    std::ofstream(path, std::ios::binary | std::ios::trunc) << contents;
    return path;
}

// A grey PNG of this many pixels.
inline std::string writePng(const std::string& path, int width, int height)
{
    cutefish::ui::Image image(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            image.setPixel(x, y, cutefish::ui::Color::rgb(0x808080));
    image.savePng(path);
    return path;
}
