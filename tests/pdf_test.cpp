// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "application.h"
#include "cards.h"
#include "passworddialog.h"
#include "pdfdocument.h"
#include "pdfpage.h"
#include "pdfview.h"
#include "previewsettings.h"
#include "previewwindow.h"
#include "sidebar.h"
#include "testutil.h"
#include "waitfor.h"

#include "ui/application.h"
#include "ui/clipboard.h"
#include "ui/keys.h"
#include "ui/test_input.h"
#include "ui/window.h"

#include <gtest/gtest.h>

using namespace cutefish::ui;
using namespace std::chrono_literals;

namespace {

// Three pages of 300 x 200. The first: a red square at its top left, "Hello
// Cutefish", a link to page 3 at its top right, one to a web address at its
// bottom right and a script at its bottom left. An outline: Start, End.
std::string samplePdf()
{
    const std::string page = "/Type /Page /Parent 2 0 R /MediaBox [0 0 300 200] /Resources << /Font << /F1 9 0 R >> >>";
    return pdfOf({
        "<< /Type /Catalog /Pages 2 0 R /Outlines 12 0 R >>",
        "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>",
        "<< " + page + " /Contents 6 0 R /Annots [10 0 R 11 0 R 15 0 R] >>",
        "<< " + page + " /Contents 7 0 R >>",
        "<< " + page + " /Contents 8 0 R >>",
        pdfStream("1 0 0 rg 0 180 20 20 re f BT /F1 12 Tf 30 100 Td (Hello Cutefish) Tj ET"),
        pdfStream("BT /F1 12 Tf 30 100 Td (Second Cutefish) Tj ET"),
        pdfStream("BT /F1 12 Tf 30 100 Td (Third page) Tj ET"),
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        "<< /Type /Annot /Subtype /Link /Rect [200 0 300 30] /Border [0 0 0] "
        "/A << /S /URI /URI (https://cutefish.org) >> >>",
        "<< /Type /Annot /Subtype /Link /Rect [200 170 300 200] /Border [0 0 0] /Dest [5 0 R /XYZ 0 200 0] >>",
        "<< /Type /Outlines /First 13 0 R /Last 14 0 R /Count 2 >>",
        "<< /Title (Start) /Parent 12 0 R /Next 14 0 R /Dest [3 0 R /Fit] >>",
        "<< /Title (End) /Parent 12 0 R /Prev 13 0 R /Dest [5 0 R /Fit] >>",
        "<< /Type /Annot /Subtype /Link /Rect [0 0 100 30] /Border [0 0 0] "
        "/A << /S /URI /URI (javascript:alert\\(1\\)) >> >>",
    });
}

class PdfUiTest : public testing::Test
{
protected:
    PdfUiTest()
        : settings(directory.filePath("preview.conf"))
        , application(settings)
    {
    }

    cutefish::base::EventLoop& loop() { return cutefish::ui::Application::instance()->loop(); }

    PreviewWindow& openPdf(const std::string& path)
    {
        application.open({path});
        EXPECT_EQ(application.windows().size(), 1u);
        PreviewWindow& window = *application.windows().front();
        EXPECT_TRUE(waitFor(loop(), [&] { return window.pdfPage()->isReady(); }));
        window.window().grab();
        return window;
    }

    // Frames go on, so animations and scrolling move, until done() holds.
    template<typename Done>
    bool until(PreviewWindow& window, Done done)
    {
        return waitFor(loop(), [&] {
            TestInput::presentFrame(window.window());
            return done();
        });
    }

    // Every tile in sight drawn at the zoom the view rests at.
    bool settled(PreviewWindow& window)
    {
        PdfView* view = window.pdfPage()->view();
        return until(window, [&] {
            return view->renderScale() == view->zoom() * window.window().scale() && !view->hasPendingTiles()
                    && view->sharpTileCount() > 0;
        });
    }

    PointF windowPoint(PreviewWindow& window, int page, PointF point)
    {
        PdfView* view = window.pdfPage()->view();
        return view->mapToWindow(view->viewPoint(page, point));
    }

    void press(PreviewWindow& window, uint32_t key, uint32_t modifiers = NoModifier)
    {
        TestInput::typeKey(window.window(), key, modifiers);
    }

    // Once the copy, read on the worker, is on the clipboard.
    bool copied(const std::string& expected)
    {
        return waitFor(loop(), [&] {
            std::optional<std::string> text;
            Clipboard::requestText([&text](std::string received) { text = std::move(received); });
            waitFor(loop(), [&] { return text.has_value(); });
            return text == expected;
        });
    }

    TemporaryDir directory;
    PreviewSettings settings;
    ::Application application;
};

} // namespace

TEST_F(PdfUiTest, APdfOpensOnItsFirstPageFittedToTheWidth)
{
    PreviewWindow& window = openPdf(writeFile(directory.filePath("sample.pdf"), samplePdf()));
    EXPECT_EQ(window.kind(), DocumentKind::Pdf);
    EXPECT_EQ(window.positionText(), "1 of 3");
    EXPECT_EQ(window.titleText(), "sample.pdf");
    // More than one page: the sidebar shows, numbered pages in it.
    EXPECT_TRUE(window.sidebarShown());
    EXPECT_EQ(window.sidebar()->count(), 3u);
    EXPECT_TRUE(until(window, [&] { return window.pdfPage()->document()->anyThumbnail(2) != nullptr; }));
    PdfView* view = window.pdfPage()->view();
    EXPECT_TRUE(view->isFitted());
    EXPECT_DOUBLE_EQ(view->pageRect(0).width, view->geometry().width - 2 * PdfView::kMargin);
    ASSERT_TRUE(settled(window));
    const PointF square = windowPoint(window, 0, {10, 10});
    EXPECT_EQ(window.window().grab().pixel(int(square.x), int(square.y)), Color::rgb(0xFF0000));

    // A wider window keeps the pages fitted.
    TestInput::configure(window.window(), 900, 600, {});
    window.window().grab();
    EXPECT_DOUBLE_EQ(view->pageRect(0).width, view->geometry().width - 2 * PdfView::kMargin);
}

TEST_F(PdfUiTest, KeysGoThroughThePagesAndScreenfuls)
{
    PreviewWindow& window = openPdf(writeFile(directory.filePath("sample.pdf"), samplePdf()));
    PdfView* view = window.pdfPage()->view();
    press(window, key::Right);
    EXPECT_EQ(view->currentPage(), 1);
    EXPECT_EQ(window.positionText(), "2 of 3");
    EXPECT_EQ(window.sidebar()->current(), 1);
    EXPECT_NEAR(view->pageRect(1).y, PdfView::kMargin, 1);
    press(window, key::End);
    EXPECT_EQ(view->currentPage(), 2);
    press(window, key::Home);
    EXPECT_EQ(view->currentPage(), 0);
    EXPECT_EQ(view->scrollPosition().y, 0);
    press(window, key::Space);
    EXPECT_NEAR(view->scrollPosition().y, view->geometry().height - 40, 1);
    press(window, key::Space, ShiftModifier);
    EXPECT_EQ(view->scrollPosition().y, 0);

    // Two fingers scroll, and what scrolls into sight is drawn.
    const PointF middle = view->mapToWindow(view->center());
    ScrollEvent event;
    event.precise = true;
    for (int i = 0; i < 10; ++i) {
        event.deltaY = 20;
        TestInput::scroll(window.window(), middle, event);
    }
    ScrollEvent stop;
    stop.precise = true;
    stop.stop = true;
    TestInput::scroll(window.window(), middle, stop);
    EXPECT_TRUE(until(window, [&] { return view->scrollPosition().y >= 150; }));
    EXPECT_TRUE(settled(window));
}

TEST_F(PdfUiTest, ZoomKeepsThePointUnderThePointerAndDrawsSharpAtRest)
{
    PreviewWindow& window = openPdf(writeFile(directory.filePath("sample.pdf"), samplePdf()));
    PdfView* view = window.pdfPage()->view();
    ASSERT_TRUE(settled(window));
    const double fit = view->zoom();
    press(window, key::letter('='), ControlModifier);
    EXPECT_TRUE(until(window, [&] { return std::abs(view->zoom() - fit * 1.5) < 1e-6; }));
    EXPECT_FALSE(view->isFitted());
    EXPECT_TRUE(settled(window));
    EXPECT_DOUBLE_EQ(view->renderScale(), fit * 1.5);

    // A pinch scales what is drawn until it rests.
    const PointF pointer = windowPoint(window, 0, {150, 100});
    TestInput::movePointer(window.window(), pointer);
    const PointF at = view->mapFromWindow(pointer);
    const PointF before = {(at.x - view->pageRect(0).x) / view->zoom(), (at.y - view->pageRect(0).y) / view->zoom()};
    GestureEvent gesture;
    gesture.type = GestureEvent::Type::PinchBegin;
    TestInput::gesture(window.window(), gesture);
    gesture.type = GestureEvent::Type::PinchUpdate;
    gesture.scale = 2;
    TestInput::gesture(window.window(), gesture);
    gesture.type = GestureEvent::Type::PinchEnd;
    TestInput::gesture(window.window(), gesture);
    EXPECT_NEAR(view->zoom(), fit * 3, 1e-6);
    EXPECT_DOUBLE_EQ(view->renderScale(), fit * 1.5);
    const PointF after = {(at.x - view->pageRect(0).x) / view->zoom(), (at.y - view->pageRect(0).y) / view->zoom()};
    EXPECT_NEAR(after.x, before.x, 0.5);
    EXPECT_NEAR(after.y, before.y, 0.5);
    EXPECT_TRUE(settled(window));
    EXPECT_NEAR(view->renderScale(), fit * 3, 1e-6);

    press(window, key::letter('0'), ControlModifier);
    EXPECT_TRUE(until(window, [&] { return view->isFitted() && std::abs(view->zoom() - fit) < 1e-6; }));
    press(window, key::letter('9'), ControlModifier);
    EXPECT_TRUE(until(window, [&] { return std::abs(view->zoom() - 1) < 1e-6; }));
}

TEST_F(PdfUiTest, FindTintsTheMatchesAndStepsThroughThem)
{
    PreviewWindow& window = openPdf(writeFile(directory.filePath("sample.pdf"), samplePdf()));
    PdfPage* page = window.pdfPage();
    PdfView* view = page->view();
    press(window, key::letter('f'), ControlModifier);
    ASSERT_TRUE(page->findCard()->isVisible());
    TestInput::typeText(window.window(), "cutefish");
    ASSERT_TRUE(waitFor(loop(), [&] { return page->document()->searchFinished(); }));
    EXPECT_EQ(page->document()->matchCount(), 2);
    EXPECT_EQ(view->matchPage(), 0);
    EXPECT_EQ(page->findCard()->countText(), "1 of 2");
    press(window, key::letter('g'), ControlModifier);
    EXPECT_EQ(view->matchPage(), 1);
    EXPECT_EQ(page->findCard()->countText(), "2 of 2");
    // Round the end, back to the first.
    TestInput::typeKey(window.window(), key::Return);
    EXPECT_EQ(view->matchPage(), 0);
    press(window, key::letter('g'), ControlModifier | ShiftModifier);
    EXPECT_EQ(view->matchPage(), 1);

    TestInput::typeText(window.window(), "zz");
    ASSERT_TRUE(waitFor(loop(), [&] { return page->document()->searchFinished(); }));
    EXPECT_EQ(page->findCard()->countText(), "No matches");
    TestInput::typeKey(window.window(), key::Escape);
    EXPECT_FALSE(page->findCard()->isVisible());
    EXPECT_EQ(page->document()->matchCount(), 0);
}

TEST_F(PdfUiTest, GoToPageTakesAPageNumber)
{
    PreviewWindow& window = openPdf(writeFile(directory.filePath("sample.pdf"), samplePdf()));
    PdfPage* page = window.pdfPage();
    press(window, key::letter('g'), ControlModifier | AltModifier);
    ASSERT_TRUE(page->goToPageCard()->isVisible());
    TestInput::typeText(window.window(), "9");
    TestInput::typeKey(window.window(), key::Return);
    // No such page: the card stays.
    EXPECT_TRUE(page->goToPageCard()->isVisible());
    EXPECT_EQ(page->view()->currentPage(), 0);
    TestInput::typeKey(window.window(), key::BackSpace);
    TestInput::typeText(window.window(), "3");
    TestInput::typeKey(window.window(), key::Return);
    EXPECT_FALSE(page->goToPageCard()->isVisible());
    EXPECT_EQ(page->view()->currentPage(), 2);
}

TEST_F(PdfUiTest, ADragSelectsTextAndCopyCopiesIt)
{
    PreviewWindow& window = openPdf(writeFile(directory.filePath("sample.pdf"), samplePdf()));
    PdfView* view = window.pdfPage()->view();
    PdfDocument* document = window.pdfPage()->document();
    ASSERT_TRUE(waitFor(loop(), [&] { return document->text(0) != nullptr; }));
    const cutefish::pdf::PageText& text = *document->text(0);
    ASSERT_GE(text.characters.size(), 14u);
    const cutefish::base::RectF first = text.characters[0].box;
    const cutefish::base::RectF fifth = text.characters[4].box;
    const PointF from = windowPoint(window, 0, {first.x() + 0.5, first.y() + first.height() / 2});
    const PointF to = windowPoint(window, 0, {fifth.right() - 0.5, fifth.y() + fifth.height() / 2});
    TestInput::movePointer(window.window(), from);
    TestInput::press(window.window(), from);
    TestInput::movePointer(window.window(), to);
    TestInput::release(window.window(), to);
    EXPECT_TRUE(view->hasSelection());
    press(window, key::letter('c'), ControlModifier);
    EXPECT_TRUE(copied("Hello"));

    // A double click takes the word.
    const cutefish::base::RectF word = text.characters[8].box;
    const PointF inWord = windowPoint(window, 0, {word.x() + word.width() / 2, word.y() + word.height() / 2});
    TestInput::click(window.window(), inWord);
    TestInput::click(window.window(), inWord);
    press(window, key::letter('c'), ControlModifier);
    EXPECT_TRUE(copied("Cutefish"));

    press(window, key::letter('a'), ControlModifier);
    press(window, key::letter('c'), ControlModifier);
    EXPECT_TRUE(copied("Hello Cutefish\nSecond Cutefish\nThird page"));
}

TEST_F(PdfUiTest, AWrongPasswordIsAskedForAgainInPlace)
{
    application.open({PREVIEW_SOURCE_DIR "/tests/data/protected.pdf"});
    PreviewWindow& window = *application.windows().front();
    PdfPage* page = window.pdfPage();
    ASSERT_TRUE(waitFor(loop(), [&] { return page->asksPassword(); }));
    PasswordDialog* dialog = page->passwordDialog();
    TestInput::typeText(*dialog->window(), "wrong");
    TestInput::typeKey(*dialog->window(), key::Return);
    ASSERT_TRUE(waitFor(loop(), [&] { return page->document()->state() == PdfDocument::State::WrongPassword; }));
    EXPECT_EQ(page->passwordDialog(), dialog);
    EXPECT_TRUE(page->asksPassword());
    TestInput::typeText(*dialog->window(), "secret");
    TestInput::typeKey(*dialog->window(), key::Return);
    ASSERT_TRUE(waitFor(loop(), [&] { return page->isReady(); }));
    EXPECT_TRUE(waitFor(loop(), [&] { return !page->asksPassword(); }));
}
