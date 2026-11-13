// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "application.h"
#include "imagepage.h"
#include "previewsettings.h"
#include "previewwindow.h"
#include "sidebar.h"
#include "testutil.h"
#include "waitfor.h"

#include "ui/application.h"
#include "ui/keys.h"
#include "ui/test_input.h"
#include "ui/window.h"

#include <gtest/gtest.h>

using namespace cutefish::ui;

namespace {

class UiTest : public testing::Test
{
protected:
    UiTest()
        : settings(directory.filePath("preview.conf"))
        , application(settings)
    {
    }

    cutefish::base::EventLoop& loop() { return cutefish::ui::Application::instance()->loop(); }

    PreviewWindow& onlyWindow()
    {
        EXPECT_EQ(application.windows().size(), 1u);
        return *application.windows().front();
    }

    // Until the window shows the file.
    bool showing(PreviewWindow& window, const std::string& path)
    {
        return waitFor(loop(), [&] { return window.imagePage()->shown().path == path; });
    }

    void press(PreviewWindow& window, uint32_t key, uint32_t modifiers = NoModifier)
    {
        TestInput::typeKey(window.window(), key, modifiers);
    }

    TemporaryDir directory;
    PreviewSettings settings;
    ::Application application;
};

} // namespace

TEST_F(UiTest, OneImageBringsItsFolderAndSizesTheWindow)
{
    const std::string a = writePng(directory.filePath("a.png"), 1000, 500);
    const std::string b = writePng(directory.filePath("b.png"), 300, 200);
    writePng(directory.filePath("c.png"), 300, 200);
    application.open({b});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, b));
    EXPECT_EQ(window.count(), 3);
    EXPECT_EQ(window.positionText(), "2 of 3");
    EXPECT_EQ(window.titleText(), "b.png");
    // At least 480 x 360 under the 40 px header.
    EXPECT_EQ(window.window().size(), (SizeF {480, 400}));

    press(window, key::Left);
    ASSERT_TRUE(showing(window, a));
    EXPECT_EQ(window.positionText(), "1 of 3");
    EXPECT_EQ(window.titleText(), "a.png");
    // Stepping never resizes the window.
    EXPECT_EQ(window.window().size(), (SizeF {480, 400}));
}

TEST_F(UiTest, TheFirstImageFitsThreeQuartersOfTheWorkArea)
{
    // Headless, the work area is 1280 x 800: 960 wide at most.
    const std::string path = writePng(directory.filePath("wide.png"), 1000, 500);
    application.open({path});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, path));
    EXPECT_EQ(window.window().size(), (SizeF {960, 480 + PreviewWindow::kHeaderHeight}));
}

TEST_F(UiTest, KeysStepThroughTheSequence)
{
    std::vector<std::string> paths;
    for (const char* name : {"1.png", "2.png", "3.png"})
        paths.push_back(writePng(directory.filePath(name), 40, 30));
    application.open({paths[0]});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, paths[0]));
    press(window, key::End);
    EXPECT_TRUE(showing(window, paths[2]));
    press(window, key::Space, ShiftModifier);
    EXPECT_TRUE(showing(window, paths[1]));
    press(window, key::Home);
    EXPECT_TRUE(showing(window, paths[0]));
    press(window, key::PageDown);
    EXPECT_TRUE(showing(window, paths[1]));
    EXPECT_EQ(window.sidebar()->current(), 1);
}

TEST_F(UiTest, SeveralImagesAreExactlyThose)
{
    const std::string a = writePng(directory.filePath("a.png"), 40, 30);
    writePng(directory.filePath("b.png"), 40, 30);
    const std::string c = writePng(directory.filePath("c.png"), 40, 30);
    application.open({c, a});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, c));
    EXPECT_EQ(window.count(), 2);
    EXPECT_EQ(window.positionText(), "1 of 2");
}

TEST_F(UiTest, OtherFilesGetWindowsOfTheirOwnWithACard)
{
    const std::string image = writePng(directory.filePath("a.png"), 40, 30);
    const std::string text = writeFile(directory.filePath("notes.txt"));
    application.open({image, text});
    ASSERT_EQ(application.windows().size(), 2u);
    PreviewWindow& textWindow = *application.windows().back();
    ASSERT_TRUE(showing(textWindow, text));
    EXPECT_FALSE(textWindow.imagePage()->showsImage());
    EXPECT_EQ(textWindow.positionText(), std::string());
}

TEST_F(UiTest, AWelcomeWindowIsReusedAndAShownFileRaisesItsWindow)
{
    application.open({});
    PreviewWindow& window = onlyWindow();
    EXPECT_TRUE(window.isWelcome());
    const std::string path = writePng(directory.filePath("a.png"), 40, 30);
    application.open({path});
    EXPECT_EQ(application.windows().size(), 1u);
    ASSERT_TRUE(showing(window, path));
    application.open({path});
    EXPECT_EQ(application.windows().size(), 1u);
    application.open({});
    EXPECT_EQ(application.windows().size(), 2u);
}
