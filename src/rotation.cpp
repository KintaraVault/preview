// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "rotation.h"

#include "base/file.h"

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <optional>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace cutefish;
namespace fs = std::filesystem;

namespace rotation {

static constexpr uint16_t kOrientationTag = 0x0112;
static constexpr uint16_t kShortType = 3;

bool turnsType(const std::string& mimeType)
{
    return mimeType == "image/jpeg";
}

bool canTurn(const std::string& path, const std::string& mimeType)
{
    const std::string folder = fs::path(path).parent_path().string();
    return turnsType(mimeType) && ::access(path.c_str(), W_OK) == 0 && ::access(folder.c_str(), W_OK | X_OK) == 0;
}

int turnedOrientation(int orientation, int quarterTurns)
{
    // One quarter clockwise more: the upright ones go 1, 6, 3, 8 and the
    // mirrored ones 2, 7, 4, 5.
    static constexpr int kNext[] = {1, 6, 7, 8, 5, 2, 3, 4, 1};
    if (orientation < 1 || orientation > 8)
        orientation = 1;
    for (int i = 0; i < ((quarterTurns % 4) + 4) % 4; ++i)
        orientation = kNext[orientation];
    return orientation;
}

// Replaces the file with these bytes through a new file in its folder,
// renamed over it, with its permissions.
static int replaceFile(const std::string& path, const std::string& bytes)
{
    struct stat st;
    if (::stat(path.c_str(), &st) != 0)
        return errno;
    const fs::path file(path);
    std::string temporary = (file.parent_path() / ("." + file.filename().string() + ".XXXXXX")).string();
    const int fd = ::mkstemp(temporary.data());
    if (fd < 0)
        return errno;
    int error = ::fchmod(fd, st.st_mode & 07777) == 0 ? 0 : errno;
    for (size_t written = 0; !error && written < bytes.size();) {
        const ssize_t n = ::write(fd, bytes.data() + written, bytes.size() - written);
        if (n < 0 && errno != EINTR)
            error = errno;
        else if (n > 0)
            written += size_t(n);
    }
    if (!error && ::fsync(fd) != 0)
        error = errno;
    if (::close(fd) != 0 && !error)
        error = errno;
    if (!error && ::rename(temporary.c_str(), path.c_str()) != 0)
        error = errno;
    if (error)
        ::unlink(temporary.c_str());
    return error;
}

// Byte order of the TIFF structure inside an Exif segment.
struct Tiff
{
    std::string& data;
    size_t start;
    bool little;

    uint32_t get(size_t at, int size) const
    {
        uint32_t value = 0;
        for (int i = 0; i < size; ++i) {
            const uint32_t byte = uint8_t(data[start + at + size_t(i)]);
            value |= little ? byte << (8 * i) : byte << (8 * (size - 1 - i));
        }
        return value;
    }

    void put(size_t at, int size, uint32_t value)
    {
        for (int i = 0; i < size; ++i)
            data[start + at + size_t(i)] = char(little ? value >> (8 * i) : value >> (8 * (size - 1 - i)));
    }
};

// The APP1 segment with Exif data and the orientation alone.
static std::string exifSegment(int orientation)
{
    std::string body("Exif\0\0II*\0\x08\0\0\0\x01\0", 16);
    body += std::string("\x12\x01\x03\0\x01\0\0\0", 8);
    body += char(orientation);
    body += std::string(3, '\0');
    body += std::string(4, '\0');
    const size_t length = body.size() + 2;
    return std::string("\xff\xe1", 2) + char(length >> 8) + char(length & 0xff) + body;
}

// Sets the orientation in the Exif segment at `segment` (its marker), or adds
// it: IFD0 is copied with the tag to the segment's end and the header points
// there, so nothing else moves and every offset in it stays right.
static bool setExifOrientation(std::string& data, size_t segment, int quarterTurns)
{
    const size_t length = uint8_t(data[segment + 2]) << 8 | uint8_t(data[segment + 3]);
    const size_t end = segment + 2 + length;
    const size_t start = segment + 10;
    if (end > data.size() || length < 8 + 8 || (data.compare(start, 2, "II") != 0 && data.compare(start, 2, "MM") != 0))
        return false;
    Tiff tiff {data, start, data[start] == 'I'};
    const size_t size = end - start;
    const size_t ifd = tiff.get(4, 4);
    if (ifd + 2 > size)
        return false;
    const uint32_t count = tiff.get(ifd, 2);
    if (ifd + 2 + count * 12 + 4 > size)
        return false;
    for (uint32_t i = 0; i < count; ++i) {
        const size_t entry = ifd + 2 + i * 12;
        if (tiff.get(entry, 2) == kOrientationTag) {
            tiff.put(entry + 8, 2, uint32_t(turnedOrientation(int(tiff.get(entry + 8, 2)), quarterTurns)));
            return true;
        }
    }
    // Offsets in TIFF are even.
    const size_t copy = size + size % 2;
    const size_t grown = copy + 2 + (count + 1) * 12 + 4;
    if (length + grown - size > 0xffff)
        return false;
    data.insert(end, std::string(grown - size, '\0'));
    Tiff out {data, start, tiff.little};
    out.put(copy, 2, count + 1);
    size_t at = copy + 2;
    bool placed = false;
    for (uint32_t i = 0; i <= count; ++i) {
        const size_t entry = ifd + 2 + i * 12;
        if (!placed && (i == count || tiff.get(entry, 2) > kOrientationTag)) {
            out.put(at, 2, kOrientationTag);
            out.put(at + 2, 2, kShortType);
            out.put(at + 4, 4, 1);
            out.put(at + 8, 4, 0);
            out.put(at + 8, 2, uint32_t(turnedOrientation(1, quarterTurns)));
            at += 12;
            placed = true;
        }
        if (i == count)
            break;
        data.replace(start + at, 12, data, start + entry, 12);
        at += 12;
    }
    out.put(at, 4, tiff.get(ifd + 2 + count * 12, 4));
    out.put(4, 4, uint32_t(copy));
    const size_t newLength = length + (grown - size);
    data[segment + 2] = char(newLength >> 8);
    data[segment + 3] = char(newLength & 0xff);
    return true;
}

// Rewrites the orientation in the segments before the scan; the scan is
// left as it is.
static std::optional<std::string> turnedJpeg(std::string data, int quarterTurns)
{
    if (data.size() < 4 || uint8_t(data[0]) != 0xff || uint8_t(data[1]) != 0xd8)
        return std::nullopt;
    size_t offset = 2;
    size_t afterApp0 = 2;
    while (offset + 4 <= data.size() && uint8_t(data[offset]) == 0xff) {
        const uint8_t marker = uint8_t(data[offset + 1]);
        if (marker == 0xff) {
            ++offset;
            continue;
        }
        if (marker == 0xda || marker == 0xd9)
            break;
        const size_t length = uint8_t(data[offset + 2]) << 8 | uint8_t(data[offset + 3]);
        if (marker == 0xe1 && data.compare(offset + 4, 6, std::string("Exif\0\0", 6)) == 0)
            return setExifOrientation(data, offset, quarterTurns) ? std::optional(std::move(data)) : std::nullopt;
        if (marker == 0xe0 && offset == 2)
            afterApp0 = offset + 2 + length;
        offset += 2 + length;
    }
    // No Exif yet: after the JFIF segment, which has to come first.
    data.insert(afterApp0, exifSegment(turnedOrientation(1, quarterTurns)));
    return data;
}

int turn(const std::string& path, const std::string& mimeType, int quarterTurns)
{
    if (((quarterTurns % 4) + 4) % 4 == 0)
        return 0;
    std::optional<std::string> data = base::file::read(path);
    if (!data)
        return ::access(path.c_str(), R_OK) != 0 ? errno : EIO;
    const std::optional<std::string> turned
            = mimeType == "image/jpeg" ? turnedJpeg(std::move(*data), quarterTurns) : std::nullopt;
    if (!turned)
        return EINVAL;
    return replaceFile(path, *turned);
}

} // namespace rotation
