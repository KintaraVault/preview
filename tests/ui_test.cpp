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
#include "ui/clipboard.h"
#include "ui/image_info.h"
#include "ui/keys.h"
#include "ui/test_input.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace cutefish::ui;
using namespace std::chrono_literals;

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
    ASSERT_TRUE(waitFor(loop(), [&] { return window.count() == 3; }));
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
    ASSERT_TRUE(waitFor(loop(), [&] { return window.count() == 3; }));
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

TEST_F(UiTest, TheSidebarIsKeptForImages)
{
    const std::string path = writePng(directory.filePath("a.png"), 40, 30);
    application.open({path});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, path));
    window.window().grab();
    EXPECT_FALSE(window.sidebar()->isVisible());
    window.toggleSidebar();
    window.window().grab();
    EXPECT_TRUE(window.sidebar()->isVisible());
    EXPECT_TRUE(settings.sidebarShown(DocumentKind::Image));
}

TEST_F(UiTest, CopyOffersPngTheFilesOwnTypeAndItsUrl)
{
    const std::string png = writePng(directory.filePath("a.png"), 40, 30);
    // A 4 x 3 true colour TGA, written by hand.
    std::string tga = std::string("\0\0\2", 3) + std::string(9, '\0') + std::string("\4\0\3\0\x18\0", 6);
    tga += std::string(4 * 3 * 3, '\x40');
    const std::string other = writeFile(directory.filePath("b.tga"), tga);
    application.open({png, other});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, png));

    const auto offers = [](const std::string& type) {
        const std::vector<std::string> types = Clipboard::mimeTypes();
        return std::find(types.begin(), types.end(), type) != types.end();
    };
    press(window, key::letter('c'), ControlModifier);
    ASSERT_TRUE(waitFor(loop(), [&] { return offers("image/png"); }));
    EXPECT_TRUE(offers("text/uri-list"));
    EXPECT_EQ(Clipboard::mimeTypes().size(), 2u);

    press(window, key::Right);
    ASSERT_TRUE(showing(window, other));
    press(window, key::letter('c'), ControlModifier);
    ASSERT_TRUE(waitFor(loop(), [&] { return offers("image/x-tga"); }));
    EXPECT_TRUE(offers("image/png"));
    EXPECT_TRUE(offers("text/uri-list"));
    std::string pasted;
    Clipboard::requestData("image/png", [&pasted](std::optional<std::string> data) { pasted = data.value_or(""); });
    ASSERT_TRUE(waitFor(loop(), [&] { return !pasted.empty(); }));
    const ImageInfo info = readImageInfo(writeFile(directory.filePath("pasted.png"), pasted), "image/png");
    EXPECT_EQ(info.width, 4);
    EXPECT_EQ(info.height, 3);
}

TEST_F(UiTest, ASwipeOnAFittedImageStepsOnce)
{
    std::vector<std::string> paths;
    for (const char* name : {"1.png", "2.png", "3.png"})
        paths.push_back(writePng(directory.filePath(name), 40, 30));
    application.open({paths[0]});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, paths[0]));
    ASSERT_TRUE(waitFor(loop(), [&] { return window.count() == 3; }));
    window.window().grab();
    const PointF center {240, 200};
    ScrollEvent event;
    event.precise = true;
    for (int i = 0; i < 10; ++i) {
        event.deltaX = 20;
        TestInput::scroll(window.window(), center, event);
    }
    ScrollEvent stop;
    stop.precise = true;
    stop.stop = true;
    TestInput::scroll(window.window(), center, stop);
    EXPECT_TRUE(showing(window, paths[1]));
    runFor(loop(), 100ms);
    EXPECT_EQ(window.imagePage()->shown().path, paths[1]);
}

TEST_F(UiTest, TheFullScreenHeaderFadesWhenThePointerRests)
{
    const std::string path = writePng(directory.filePath("a.png"), 40, 30);
    application.open({path});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, path));
    WindowStates states;
    states.fullScreen = true;
    TestInput::configure(window.window(), 1280, 800, states);
    // The header over the black around the image.
    EXPECT_EQ(window.window().grab().pixel(640, 20), Theme::current().background);
    EXPECT_EQ(window.chromeOpacity(), 1);
    EXPECT_TRUE(waitFor(loop(), [&] { return window.chromeOpacity() == 0; }, 3s));
    TestInput::movePointer(window.window(), {100, 100});
    EXPECT_TRUE(waitFor(loop(), [&] { return window.chromeOpacity() == 1; }));
}

// A pinch to three times over the image, which then overflows the view both
// ways, keeps the image's point under the pointer, with the sidebar shown or
// in full screen too.
TEST_F(UiTest, APinchZoomsAboutThePointer)
{
    const std::string path = writePng(directory.filePath("a.png"), 1600, 1200);
    writePng(directory.filePath("b.png"), 1600, 1200);
    application.open({path});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, path));
    ASSERT_TRUE(waitFor(loop(), [&] { return window.count() == 2; }));
    ImageViewer* viewer = window.imagePage()->viewer();
    const auto pinch = [&](PointF pointer) {
        window.window().grab();
        TestInput::movePointer(window.window(), pointer);
        const PointF anchor = viewer->mapFromWindow(pointer);
        const PointF before = viewer->imagePoint(anchor);
        const double fit = viewer->zoom();
        GestureEvent event;
        event.type = GestureEvent::Type::PinchBegin;
        TestInput::gesture(window.window(), event);
        event.type = GestureEvent::Type::PinchUpdate;
        event.scale = 3;
        TestInput::gesture(window.window(), event);
        event.type = GestureEvent::Type::PinchEnd;
        TestInput::gesture(window.window(), event);
        EXPECT_NEAR(viewer->zoom(), fit * 3, 1e-9);
        const PointF after = viewer->imagePoint(anchor);
        EXPECT_NEAR(after.x, before.x, 0.5);
        EXPECT_NEAR(after.y, before.y, 0.5);
        // Pinching in stops at the fit.
        event.type = GestureEvent::Type::PinchBegin;
        TestInput::gesture(window.window(), event);
        event.type = GestureEvent::Type::PinchUpdate;
        event.scale = 0.2;
        TestInput::gesture(window.window(), event);
        event.type = GestureEvent::Type::PinchEnd;
        TestInput::gesture(window.window(), event);
        EXPECT_NEAR(viewer->zoom(), fit, 1e-9);
    };
    pinch({300, 200});

    window.toggleSidebar();
    pinch({500, 300});

    press(window, key::Right);
    ASSERT_TRUE(showing(window, directory.filePath("b.png")));
    pinch({400, 250});

    WindowStates states;
    states.fullScreen = true;
    TestInput::configure(window.window(), 1280, 800, states);
    pinch({700, 500});
}

// A turn shows at once, the file follows, and its own write reloads it once.
TEST_F(UiTest, RotatingTurnsThePictureThenTheFile)
{
    const std::string png = writePng(directory.filePath("a.png"), 60, 30);
    std::string tga = std::string("\0\0\2", 3) + std::string(9, '\0') + std::string("\4\0\3\0\x18\0", 6);
    tga += std::string(4 * 3 * 3, '\x40');
    const std::string other = writeFile(directory.filePath("b.tga"), tga);
    application.open({png, other});
    PreviewWindow& window = onlyWindow();
    ASSERT_TRUE(showing(window, png));
    ImageViewer* viewer = window.imagePage()->viewer();
    EXPECT_TRUE(window.canRotate());

    // Three quick presses come to one turn left.
    press(window, key::letter('r'), ControlModifier);
    EXPECT_EQ(viewer->quarterTurns(), 1);
    press(window, key::letter('r'), ControlModifier);
    press(window, key::letter('r'), ControlModifier);
    EXPECT_EQ(viewer->quarterTurns(), 3);
    ASSERT_TRUE(waitFor(loop(), [&] {
        return readImageInfo(png, "image/png").width == 30 && window.imagePage()->shown().info.width == 30
                && viewer->quarterTurns() == 0;
    }));
    EXPECT_EQ(readImageInfo(png, "image/png").height, 60);

    // The folder watch reports the save too; it brings no second reload,
    // which would fit the image again.
    viewer->zoomTo(2, viewer->center(), false);
    runFor(loop(), 400ms);
    EXPECT_TRUE(viewer->isZoomed());

    press(window, key::Right);
    ASSERT_TRUE(showing(window, other));
    EXPECT_FALSE(window.canRotate());
    press(window, key::letter('l'), ControlModifier);
    EXPECT_EQ(viewer->quarterTurns(), 0);
}
