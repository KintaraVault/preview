// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "base/file_watcher.h"
#include "base/signal.h"
#include "base/thread_pool.h"

#include <memory>
#include <string>
#include <vector>

// The images a window steps through, one of them current: those of a
// folder, followed as files come and go, or the files it was given.
class ImageSequence
{
public:
    // The file and the images beside it, by name with numbers in order,
    // hidden files left out. The file alone until the folder is read, on a
    // worker: twenty thousand names take some 50 ms.
    static std::unique_ptr<ImageSequence> folderOf(const std::string& path);
    // These files in this order; one that leaves the disk leaves the sequence.
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
    // A file the window moved away: the next one takes the current one's
    // place, or the previous one at the end.
    void remove(const std::string& path);

    // The files or the current one changed.
    cutefish::base::Signal<> changed;
    // The current file was written over.
    cutefish::base::Signal<> currentWritten;

private:
    ImageSequence(std::vector<std::string> paths, int index, bool followsFolder);

    void watch();
    void folderChanged(const std::string& folder, const std::string& name);
    // Reads the folder on a worker and takes its images in.
    void listFolder();
    // Keeps the current file where it is still there, else its place.
    void replace(std::vector<std::string> paths);

    std::vector<std::string> m_paths;
    int m_index = 0;
    bool m_followsFolder = false;
    std::vector<cutefish::base::FileWatch> m_watches;
    cutefish::base::Job m_listing;
};
