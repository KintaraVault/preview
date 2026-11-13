// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "rotation.h"
#include "testutil.h"

#include "base/file.h"
#include "ui/image.h"
#include "ui/image_info.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <jpeglib.h>
#include <sys/stat.h>

using cutefish::ui::Color;
using cutefish::ui::Image;
using cutefish::ui::ImageInfo;

// 16 x 8: red on the left half, blue on the right, a green top-left quarter
// of the red, so every turn and mirror tells apart.
static Color pattern(int x, int y)
{
    if (x < 4 && y < 4)
        return Color::rgb(0x00FF00);
    return Color::rgb(x < 8 ? 0xFF0000 : 0x0000FF);
}

// With a JFIF segment first, and an APP1 segment, when asked.
static std::string writeJpeg(const std::string& path, bool jfif = false, const std::string& app1 = {})
{
    FILE* file = std::fopen(path.c_str(), "wb");
    jpeg_compress_struct info;
    jpeg_error_mgr error;
    info.err = jpeg_std_error(&error);
    jpeg_create_compress(&info);
    jpeg_stdio_dest(&info, file);
    info.image_width = 16;
    info.image_height = 8;
    info.input_components = 3;
    info.in_color_space = JCS_RGB;
    jpeg_set_defaults(&info);
    info.write_JFIF_header = jfif;
    jpeg_set_quality(&info, 100, TRUE);
    // No chroma subsampling, so colours stay apart at their edges.
    for (int i = 0; i < info.num_components; ++i) {
        info.comp_info[i].h_samp_factor = 1;
        info.comp_info[i].v_samp_factor = 1;
    }
    jpeg_start_compress(&info, TRUE);
    if (!app1.empty())
        jpeg_write_marker(&info, JPEG_APP0 + 1, reinterpret_cast<const JOCTET*>(app1.data()), unsigned(app1.size()));
    std::vector<JSAMPLE> row(16 * 3);
    while (info.next_scanline < 8) {
        for (int x = 0; x < 16; ++x) {
            const Color c = pattern(x, int(info.next_scanline));
            row[size_t(x) * 3] = c.r;
            row[size_t(x) * 3 + 1] = c.g;
            row[size_t(x) * 3 + 2] = c.b;
        }
        JSAMPROW pointer = row.data();
        jpeg_write_scanlines(&info, &pointer, 1);
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    std::fclose(file);
    return path;
}

// The entropy-coded scan and what follows it.
static std::string scanOf(const std::string& data)
{
    const size_t sos = data.find("\xff\xda");
    return sos == std::string::npos ? std::string() : data.substr(sos);
}

static bool near(Color a, Color b)
{
    return std::abs(a.r - b.r) < 40 && std::abs(a.g - b.g) < 40 && std::abs(a.b - b.b) < 40;
}

// The decoded picture is the pattern turned this many quarters clockwise.
static void expectTurned(const std::string& path, int turns)
{
    const Image image = Image::fromFile(path);
    ASSERT_FALSE(image.isNull());
    const bool odd = turns % 2;
    ASSERT_EQ(image.width(), odd ? 8 : 16);
    ASSERT_EQ(image.height(), odd ? 16 : 8);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            // Back to where the pixel was before each turn: one clockwise
            // turn puts (x, y) of a picture `height` tall at (height - 1 - y, x).
            int sx = x;
            int sy = y;
            int width = image.width();
            for (int i = 0; i < turns; ++i) {
                const int before = sx;
                sx = sy;
                sy = width - 1 - before;
                width = width == 8 ? 16 : 8;
            }
            // Pixels next to an edge of colour blur in a JPEG.
            const bool edge = (sx % 4 == 3 || sx % 4 == 0) || (sy % 4 == 3 || sy % 4 == 0);
            if (!edge)
                EXPECT_TRUE(near(image.pixel(x, y), pattern(sx, sy))) << path << " " << turns << " at " << x << "," << y;
        }
    }
}

TEST(RotationTest, OrientationsTurnInFour)
{
    for (int orientation = 1; orientation <= 8; ++orientation) {
        EXPECT_EQ(rotation::turnedOrientation(orientation, 4), orientation);
        EXPECT_EQ(rotation::turnedOrientation(orientation, -1), rotation::turnedOrientation(orientation, 3));
    }
    EXPECT_EQ(rotation::turnedOrientation(1, 1), 6);
    EXPECT_EQ(rotation::turnedOrientation(1, 2), 3);
    EXPECT_EQ(rotation::turnedOrientation(1, -1), 8);
}

TEST(RotationTest, AJpegTurnsByItsOrientationAlone)
{
    TemporaryDir directory;
    for (const bool jfif : {false, true}) {
        const std::string path = writeJpeg(directory.filePath(jfif ? "jfif.jpg" : "plain.jpg"), jfif);
        const std::string original = *cutefish::base::file::read(path);
        for (int turns = 1; turns <= 4; ++turns) {
            ASSERT_EQ(rotation::turn(path, "image/jpeg", 1), 0);
            const std::string turned = *cutefish::base::file::read(path);
            EXPECT_EQ(scanOf(turned), scanOf(original));
            if (jfif)
                EXPECT_EQ(turned.compare(2, 2, "\xff\xe0"), 0);
            const ImageInfo info = cutefish::ui::readImageInfo(path, "image/jpeg");
            EXPECT_EQ(info.width, turns % 2 ? 8 : 16);
            expectTurned(path, turns % 4);
        }
    }
}

TEST(RotationTest, AnExifWithoutOrientationGetsOne)
{
    TemporaryDir directory;
    // Little-endian TIFF, IFD0 at 8 with one entry: Make, ASCII, 9 bytes at 26.
    std::string exif("Exif\0\0II*\0\x08\0\0\0\x01\0", 16);
    exif += std::string("\x0f\x01\x02\0\x09\0\0\0\x1a\0\0\0", 12);
    exif += std::string(4, '\0');
    exif += std::string("Cutefish\0", 9);
    const std::string path = writeJpeg(directory.filePath("make.jpg"), false, exif);
    const std::string original = *cutefish::base::file::read(path);
    ASSERT_EQ(rotation::turn(path, "image/jpeg", -1), 0);
    const std::string turned = *cutefish::base::file::read(path);
    EXPECT_EQ(scanOf(turned), scanOf(original));
    EXPECT_NE(turned.find("Cutefish"), std::string::npos);
    expectTurned(path, 3);
    ASSERT_EQ(rotation::turn(path, "image/jpeg", 1), 0);
    expectTurned(path, 0);
}

TEST(RotationTest, OnlyWritableJpegAndPngFilesTurn)
{
    TemporaryDir directory;
    const std::string jpeg = writeJpeg(directory.filePath("a.jpg"));
    EXPECT_TRUE(rotation::canTurn(jpeg, "image/jpeg"));
    EXPECT_FALSE(rotation::canTurn(jpeg, "image/webp"));
    EXPECT_FALSE(rotation::canTurn(jpeg, "image/gif"));
    ::chmod(jpeg.c_str(), 0444);
    EXPECT_FALSE(rotation::canTurn(jpeg, "image/jpeg"));
    EXPECT_EQ(rotation::turn(writeFile(directory.filePath("b.png"), "not a png"), "image/png", 1), EINVAL);
}
