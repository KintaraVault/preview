// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "imagesequence.h"
#include "testutil.h"

#include <gtest/gtest.h>

#include <filesystem>

TEST(ImageSequenceTest, AFolderIsItsImagesInNumericOrder)
{
    TemporaryDir directory;
    for (const char* name : {"img10.png", "img2.png", "Img1.jpg", "b.webp", ".hidden.png", "notes.txt"})
        writeFile(directory.filePath(name));
    const auto sequence = ImageSequence::folderOf(directory.filePath("img2.png"));
    const std::vector<std::string> expected = {directory.filePath("b.webp"), directory.filePath("Img1.jpg"),
                                               directory.filePath("img2.png"), directory.filePath("img10.png")};
    EXPECT_EQ(sequence->paths(), expected);
    EXPECT_EQ(sequence->currentIndex(), 2);
}

TEST(ImageSequenceTest, AHiddenImageOpenedKeepsItsPlace)
{
    TemporaryDir directory;
    writeFile(directory.filePath("a.png"));
    writeFile(directory.filePath(".b.png"));
    const auto sequence = ImageSequence::folderOf(directory.filePath(".b.png"));
    EXPECT_EQ(sequence->count(), 2);
    EXPECT_EQ(sequence->currentPath(), directory.filePath(".b.png"));
}

TEST(ImageSequenceTest, FilesKeepTheOrderGiven)
{
    TemporaryDir directory;
    const std::string c = writeFile(directory.filePath("c.png"));
    const std::string a = writeFile(directory.filePath("a.png"));
    writeFile(directory.filePath("b.png"));
    const auto sequence = ImageSequence::files({c, a});
    EXPECT_EQ(sequence->paths(), (std::vector<std::string> {c, a}));
    EXPECT_EQ(sequence->currentPath(), c);
}
