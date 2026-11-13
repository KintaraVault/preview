// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/signal.h"

#include <memory>
#include <string>
#include <vector>

// The images a window steps through, one of them current: those of a
// folder, or the files it was given.
class ImageSequence
{
public:
    // These files in this order.
    static std::unique_ptr<ImageSequence> files(std::vector<std::string> paths);

    ImageSequence(const ImageSequence&) = delete;
    ImageSequence& operator=(const ImageSequence&) = delete;

    // By its name, a type the toolkit decodes.
    static bool isImage(const std::string& path);

    const std::vector<std::string>& paths() const { return m_paths; }
    int count() const { return int(m_paths.size()); }
    bool isEmpty() const { return m_paths.empty(); }
    int currentIndex() const { return m_index; }
    // Empty when the sequence is.
    std::string currentPath() const;
    void setCurrentIndex(int index);

    // The files or the current one changed.
    cutefish::base::Signal<> changed;

private:
    ImageSequence(std::vector<std::string> paths, int index);

    std::vector<std::string> m_paths;
    int m_index = 0;
};
