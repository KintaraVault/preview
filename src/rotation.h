// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

// Turning an image file a number of quarters clockwise, in place and
// without loss: a JPEG by its EXIF orientation, its compressed data
// untouched; a PNG encoded again, its depth, palette, colour profile and
// text kept. The new file replaces the old one in one rename, with the old
// one's permissions.
namespace rotation {

// The types that turn without loss.
bool turnsType(const std::string& mimeType);
// The file is of such a type and it and its folder may be written.
bool canTurn(const std::string& path, const std::string& mimeType);
// 0 when done, else an errno value; on any thread.
int turn(const std::string& path, const std::string& mimeType, int quarterTurns);

// The EXIF orientation of an image shown with `orientation` and then turned
// that many quarters clockwise.
int turnedOrientation(int orientation, int quarterTurns);

} // namespace rotation
