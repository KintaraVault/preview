// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "previewwindow.h"
#include "application.h"
#include "imagepage.h"
#include "imagesequence.h"
#include "sidebar.h"

#include "base/event_loop.h"
#include "base/i18n.h"
#include "base/url.h"
#include "ui/cell_layout.h"
#include "ui/animation.h"
#include "ui/application.h"
#include "ui/color_provider.h"
#include "ui/file_chooser.h"
#include "ui/file_drop_area.h"
#include "ui/header_button.h"
#include "ui/keys.h"
#include "ui/label.h"
#include "ui/painter.h"
#include "ui/theme.h"
#include "ui/window.h"
#include "ui/window_header.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <utility>

using namespace cutefish;
using namespace std::chrono_literals;
using cutefish::i18n::format;
using cutefish::i18n::tr;
using ui::rounding::centeredStart;

static constexpr double kButtonSize = 32;
static constexpr double kMinimumContentWidth = 480;
static constexpr double kMinimumContentHeight = 360;
static constexpr double kZoomStep = 1.5;
static constexpr double kSidebarGap = 9;

namespace {

// The header's strip: the window's colour over the content, so the header
// stays readable over a full screen image, and a hairline along its bottom.
class HeaderBar : public ui::View
{
public:
    explicit HeaderBar(double height)
    {
        header = addChild<ui::WindowHeader>(height);
        header->setButtonMargin(4);
        header->setButtonsCentered(true);
        header->setContentUnderButtons(true);
    }

    ui::WindowHeader* header;
    // Where the content starts, past the sidebar.
    double contentLeft = 0;

protected:
    void layout() override { header->setGeometry(localRect()); }

    void paint(ui::Painter& painter) override
    {
        const ui::RectF rect = localRect();
        painter.fillRect({contentLeft, 0, rect.width - contentLeft, rect.height}, ui::Theme::current().background);
        const double line = 1 / painter.scale();
        painter.drawRoundedRect({contentLeft, rect.height - line, rect.width - contentLeft, line}, {0},
                                ui::color(ui::ColorId::HeaderSeparator));
    }
};

} // namespace

// The header's own part: the sidebar toggle after the window buttons, the
// arrows with the place in the sequence and the file name from the
// sidebar's edge, the zoom buttons at the end.
class PreviewWindow::HeaderRow : public ui::View
{
public:
    explicit HeaderRow(PreviewWindow& owner, ui::WindowHeader* header)
        : m_header(header)
    {
        sidebar = addButton("sidebar-left", [&owner] { owner.toggleSidebar(); });
        previous = addButton("chevron-left", [&owner] { owner.step(-1); });
        next = addButton("chevron-right", [&owner] { owner.step(1); });
        position = addChild<ui::Label>();
        title = addChild<ui::Label>();
        title->setWeight(700);
        title->setElide(ui::Label::Elide::Middle);
        zoomOut = addButton("zoom-out", [&owner] { owner.zoomOut(); });
        zoomIn = addButton("zoom-in", [&owner] { owner.zoomIn(); });
    }

    ui::HeaderButton* sidebar;
    ui::HeaderButton* previous;
    ui::HeaderButton* next;
    ui::Label* position;
    ui::Label* title;
    ui::HeaderButton* zoomOut;
    ui::HeaderButton* zoomIn;
    double sidebarWidth = 0;

protected:
    void layout() override
    {
        const double height = geometry().height;
        const double top = centeredStart(height, kButtonSize);
        double x = m_header->leadingInset() + 4;
        sidebar->setGeometry({x, top, kButtonSize, kButtonSize});
        // From the sidebar's edge as the Files header's buttons are.
        x = std::max(x + kButtonSize + 4, sidebarWidth + kSidebarGap);
        if (previous->isVisible()) {
            previous->setGeometry({x, top, kButtonSize, kButtonSize});
            x += kButtonSize;
            next->setGeometry({x, top, kButtonSize, kButtonSize});
            x += kButtonSize + 4;
            const ui::SizeF hint = position->sizeHint();
            position->setColor(ui::ColorId::Text);
            position->setGeometry({x, centeredStart(height, hint.height), hint.width, hint.height});
            x += hint.width;
        }
        double right = geometry().width - 4;
        zoomIn->setGeometry({right - kButtonSize, top, kButtonSize, kButtonSize});
        right -= kButtonSize;
        zoomOut->setGeometry({right - kButtonSize, top, kButtonSize, kButtonSize});
        right -= kButtonSize;
        const double left = x + 8;
        const ui::SizeF hint = title->sizeHint();
        title->setColor(ui::ColorId::Text);
        title->setGeometry({left, centeredStart(height, hint.height), std::max(0.0, right - 8 - left), hint.height});
    }

private:
    ui::HeaderButton* addButton(const char* glyph, std::function<void()> action)
    {
        auto* button = addChild<ui::HeaderButton>(std::string(), kButtonSize);
        button->setGlyph(glyph, 16);
        button->onClicked = std::move(action);
        return button;
    }

    ui::WindowHeader* m_header;
};

// The window's content: the sidebar down the left edge, the page beside it
// under the header, and in full screen the page under the header too, on
// black. Files dropped on it open as File > Open opens them.
class PreviewWindow::Root : public ui::FileDropArea
{
public:
    explicit Root(PreviewWindow& owner)
        : m_owner(owner)
    {
        setTracksHover(true);
        sidebar = addChild<Sidebar>(kHeaderHeight);
        image = addChild<ImagePage>();
        bar = addChild<HeaderBar>(kHeaderHeight);
        row = bar->header->setContent<HeaderRow>(owner, bar->header);
        filter = [](const std::string& url) { return base::url::isLocalFile(url); };
        onDropped = [this](const std::vector<std::string>& urls) {
            std::vector<std::string> paths;
            for (const std::string& url : urls)
                paths.push_back(base::url::toLocalFile(url).value_or(std::string()));
            m_owner.m_application.open(paths);
        };
    }

    double sidebarWidth() const { return sidebar->isVisible() ? Sidebar::kWidth : 0; }

    Sidebar* sidebar;
    ImagePage* image;
    HeaderBar* bar;
    HeaderRow* row;

protected:
    void layout() override
    {
        const ui::RectF rect = localRect();
        sidebar->setVisible(m_owner.m_sidebarShown && !m_owner.isWelcome());
        image->setVisible(!m_owner.isWelcome());
        const double left = sidebarWidth();
        const double top = m_owner.isFullScreen() ? 0 : kHeaderHeight;
        sidebar->setGeometry({0, 0, left, rect.height});
        const ui::RectF content {left, top, std::max(0.0, rect.width - left), std::max(0.0, rect.height - top)};
        image->setGeometry(content);
        bar->contentLeft = left;
        bar->setGeometry({0, 0, rect.width, kHeaderHeight});
        row->sidebarWidth = left;
        row->requestLayout();
        if (window())
            window()->setBackgroundBlur(rect, ui::Theme::current().windowRadius);
    }

    void paint(ui::Painter& painter) override
    {
        const double left = sidebarWidth();
        const ui::RectF rect = localRect();
        painter.fillRect({left, 0, rect.width - left, rect.height},
                         m_owner.isFullScreen() ? ui::Color {0, 0, 0, 255} : ui::Theme::current().background);
    }

    void hoverMove(const ui::PointerEvent& event) override
    {
        // Surface changes resend motion at the same spot; that is not activity.
        const ui::PointF position = mapToWindow(event.position);
        if (position.x == m_lastHover.x && position.y == m_lastHover.y)
            return;
        m_lastHover = position;
        m_owner.poke();
    }

private:
    PreviewWindow& m_owner;
    ui::PointF m_lastHover {-1, -1};
};

PreviewWindow::PreviewWindow(Application& application)
    : m_application(application)
    , m_window(std::make_unique<ui::Window>())
{
    m_window->setTitle(tr("Preview"));
    m_window->resize({720, 540});
    m_window->setMinimumSize({kMinimumContentWidth, kMinimumContentHeight + kHeaderHeight});
    m_window->setResizeAroundCenter(true);
    auto root = std::make_unique<Root>(*this);
    m_root = root.get();
    m_window->setContentView(std::move(root));

    ImagePage* page = m_root->image;
    page->viewportFor = [this](const ui::ImageInfo& info) { return viewportFor(info); };
    page->onShown = [this] { shown(); };
    page->viewer()->onSwipe = [this](int delta) { step(delta); };
    m_root->sidebar->onActivated = [this](int index) { goTo(index); };

    m_window->onCloseRequested = [this] { close(); };
    m_window->onActiveChanged = [this] { updateBackground(); };
    m_window->onStatesChanged = [this] {
        const bool fullScreen = isFullScreen();
        ui::WindowHeader* header = m_root->bar->header;
        header->setMinimizeVisible(!fullScreen);
        header->setMaximizeVisible(!fullScreen);
        header->setCloseVisible(!fullScreen);
        poke();
        m_root->requestLayout();
        m_root->requestPaint();
    };

    m_idle.setSingleShot(true);
    m_idle.setInterval(2000ms);
    m_idle.onTimeout = [this] {
        m_pointerActive = false;
        updateChrome();
    };

    bindShortcuts();
    updateBackground();
    updateHeader();
}

PreviewWindow::~PreviewWindow() = default;

ImagePage* PreviewWindow::imagePage() const
{
    return m_root->image;
}

Sidebar* PreviewWindow::sidebar() const
{
    return m_root->sidebar;
}

std::string PreviewWindow::positionText() const
{
    return m_root->row->position->isVisible() ? m_root->row->position->text() : std::string();
}

std::string PreviewWindow::titleText() const
{
    return m_root->row->title->text();
}

std::string PreviewWindow::currentPath() const
{
    return m_sequence ? m_sequence->currentPath() : std::string();
}

int PreviewWindow::currentIndex() const
{
    if (!m_sequence)
        return -1;
    const std::vector<std::string>& paths = m_sequence->paths();
    const auto found = std::find(paths.begin(), paths.end(), m_root->image->shown().path);
    return found == paths.end() ? m_sequence->currentIndex() : int(found - paths.begin());
}

int PreviewWindow::count() const
{
    return m_sequence ? m_sequence->count() : 0;
}

void PreviewWindow::show(const std::vector<std::string>& paths)
{
    std::error_code error;
    const bool folder = paths.size() == 1 && ImageSequence::isImage(paths.front())
            && std::filesystem::is_regular_file(paths.front(), error);
    m_sequenceConnections.clear();
    m_sequence = folder ? ImageSequence::folderOf(paths.front()) : ImageSequence::files(paths);
    m_sequenceConnections.push_back(m_sequence->changed.connect([this] { sequenceChanged(); }));
    m_sequenceConnections.push_back(
            m_sequence->currentWritten.connect([this] { m_root->image->load(m_sequence->currentPath()); }));
    m_sidebarShown = m_application.settings().sidebarShown(kind());
    m_mapPending = !m_mapped;
    m_root->sidebar->setItems(m_sequence->paths());
    m_root->image->load(m_sequence->currentPath());
    m_root->requestLayout();
    updateHeader();
}

void PreviewWindow::raise(const std::string& activationToken)
{
    if (m_mapPending) {
        m_activationToken = activationToken;
        return;
    }
    m_window->show();
    m_mapped = true;
    m_window->activate(activationToken);
}

void PreviewWindow::sequenceChanged()
{
    if (m_sequence->isEmpty()) {
        // Not from inside the sequence's own signal.
        base::EventLoop::current()->post([this, alive = std::weak_ptr<bool>(m_alive)] {
            if (alive.expired() || !m_sequence || !m_sequence->isEmpty())
                return;
            m_sequenceConnections.clear();
            m_sequence.reset();
            // The next file sizes the window again, as the first one did.
            m_sized = false;
            m_root->image->clear();
            m_root->requestLayout();
            updateHeader();
            updateTitle();
        });
        return;
    }
    m_root->sidebar->setItems(m_sequence->paths());
    if (m_sequence->currentPath() != m_root->image->shown().path)
        m_root->image->load(m_sequence->currentPath());
    updateHeader();
}

void PreviewWindow::shown()
{
    if (!m_sized) {
        m_sized = true;
        if (!isFullScreen() && !m_window->states().maximized)
            m_window->resize(windowSizeFor(m_root->image->shown().info));
    }
    if (m_mapPending) {
        m_mapPending = false;
        m_window->show();
        m_mapped = true;
        m_window->activate(std::exchange(m_activationToken, std::string()));
    }
    const int index = currentIndex();
    std::vector<std::string> ahead;
    for (const int neighbour : {index + 1, index - 1}) {
        if (neighbour >= 0 && neighbour < count())
            ahead.push_back(m_sequence->paths()[size_t(neighbour)]);
    }
    m_root->image->preload(ahead);
    updateHeader();
    updateTitle();
}

void PreviewWindow::updateHeader()
{
    HeaderRow* row = m_root->row;
    const bool document = !isWelcome();
    const bool multiple = count() > 1;
    const bool image = document && m_root->image->showsImage();
    row->sidebar->setVisible(document);
    row->sidebar->setChecked(m_sidebarShown);
    for (ui::View* view : std::initializer_list<ui::View*> {row->previous, row->next, row->position})
        view->setVisible(multiple);
    const int index = currentIndex();
    row->previous->setEnabled(index > 0);
    row->next->setEnabled(index + 1 < count());
    // TRANSLATORS: the shown image's place among the window's images, such as "3 of 24".
    row->position->setText(format(tr("{index} of {count}"),
                                  {{"index", std::to_string(index + 1)}, {"count", std::to_string(count())}}));
    row->title->setVisible(document);
    row->title->setText(m_root->image->shown().name);
    row->zoomOut->setVisible(document);
    row->zoomIn->setVisible(document);
    row->zoomOut->setEnabled(image);
    row->zoomIn->setEnabled(image);
    m_root->sidebar->setCurrent(index);
    row->requestLayout();
}

void PreviewWindow::updateTitle()
{
    const std::string& name = m_root->image->shown().name;
    m_window->setTitle(isWelcome() || name.empty() ? std::string(tr("Preview")) : name);
}

void PreviewWindow::updateBackground()
{
    m_window->setBackgroundColor(m_window->isActive() ? ui::ColorId::TranslucentWindowBackground
                                                      : ui::ColorId::TranslucentWindowBackgroundInactive);
    // Views colour their text by it as they lay out.
    m_window->requestLayout();
}

ui::SizeF PreviewWindow::windowSizeFor(const ui::ImageInfo& info) const
{
    ui::SizeF area = m_window->availableSize();
    if (area.width <= 0 || area.height <= 0)
        area = {1280, 800};
    const double sidebar = m_sidebarShown ? Sidebar::kWidth : 0;
    ui::SizeF content {kMinimumContentWidth, kMinimumContentHeight};
    if (info.readable && info.width > 0 && info.height > 0) {
        // One image pixel to a device pixel, shrunk into a large share of the work area.
        const double ratio = m_window->scale() > 0 ? m_window->scale() : 1;
        const double width = info.width / ratio;
        const double height = info.height / ratio;
        const double scale = std::min({1.0, (std::floor(area.width * 0.75) - sidebar) / width,
                                       (std::floor(area.height * 0.8) - kHeaderHeight) / height});
        content = {std::max(kMinimumContentWidth, std::round(width * scale)),
                   std::max(kMinimumContentHeight, std::round(height * scale))};
    } else if (info.readable) {
        // Without a size in its header the image fits this.
        content = {720, 540};
    }
    return {content.width + sidebar, content.height + kHeaderHeight};
}

ui::SizeF PreviewWindow::viewportFor(const ui::ImageInfo& info) const
{
    if (m_sized || isFullScreen() || m_window->states().maximized)
        return m_root->image->geometry().size();
    const ui::SizeF size = windowSizeFor(info);
    return {size.width - (m_sidebarShown ? Sidebar::kWidth : 0), size.height - kHeaderHeight};
}

void PreviewWindow::step(int delta)
{
    if (m_sequence)
        goTo(m_sequence->currentIndex() + delta);
}

void PreviewWindow::goTo(int index)
{
    if (m_sequence)
        m_sequence->setCurrentIndex(index);
}

void PreviewWindow::zoomIn()
{
    ImageViewer* viewer = m_root->image->viewer();
    if (m_root->image->showsImage())
        viewer->zoomBy(kZoomStep, viewer->center(), true);
}

void PreviewWindow::zoomOut()
{
    ImageViewer* viewer = m_root->image->viewer();
    if (m_root->image->showsImage())
        viewer->zoomBy(1 / kZoomStep, viewer->center(), true);
}

void PreviewWindow::zoomToFit()
{
    if (m_root->image->showsImage())
        m_root->image->viewer()->zoomToFit();
}

void PreviewWindow::zoomToActualSize()
{
    if (m_root->image->showsImage())
        m_root->image->viewer()->zoomToActualSize();
}

void PreviewWindow::toggleSidebar()
{
    if (isWelcome())
        return;
    m_sidebarShown = !m_sidebarShown;
    m_application.settings().setSidebarShown(kind(), m_sidebarShown);
    m_root->requestLayout();
    m_root->requestPaint();
    updateHeader();
}

bool PreviewWindow::isFullScreen() const
{
    return m_window->states().fullScreen;
}

void PreviewWindow::toggleFullScreen()
{
    if (!isWelcome() || isFullScreen())
        m_window->setFullScreen(!isFullScreen());
}

void PreviewWindow::openDialog()
{
    m_chooser = std::make_unique<ui::FileChooser>(m_window.get());
    m_chooser->setMode(ui::FileChooser::Mode::OpenFiles);
    m_chooser->setTitle(tr("Open"));
    ui::FileChooser::Filter images;
    images.name = tr("Images");
    images.mimeTypes = ui::imageMimeTypes();
    m_chooser->setFilters({images});
    if (!isWelcome())
        m_chooser->setCurrentFolder(std::filesystem::path(currentPath()).parent_path().string());
    m_chooser->onAccepted = [this](const std::vector<std::string>& paths, const std::string&) {
        m_application.open(paths);
    };
    m_chooser->open();
}

void PreviewWindow::close()
{
    m_application.releaseWindow(this);
}

void PreviewWindow::poke()
{
    m_pointerActive = true;
    m_idle.start();
    updateChrome();
}

void PreviewWindow::updateChrome()
{
    const double target = !isFullScreen() || isWelcome() || m_pointerActive ? 1 : 0;
    if (target == m_chromeTo)
        return;
    HeaderBar* bar = m_root->bar;
    const auto apply = [this, bar](double opacity) {
        m_chromeOpacity = opacity;
        bar->setOpacity(opacity);
        bar->setVisible(opacity > 0);
    };
    m_chromeFrom = m_chromeOpacity;
    m_chromeTo = target;
    if (!m_chrome) {
        m_chrome = std::make_unique<ui::Animation>(m_window.get());
        m_chrome->setDuration(200ms);
        m_chrome->onUpdate = [this, apply](double value) { apply(m_chromeFrom + (m_chromeTo - m_chromeFrom) * value); };
    }
    m_chrome->stop();
    if (ui::Application::instance()->platform() == ui::Application::Platform::Headless)
        apply(target);
    else
        m_chrome->start();
}

void PreviewWindow::bindShortcuts()
{
    ui::Window& window = *m_window;
    const uint32_t none = ui::NoModifier;
    const uint32_t ctrl = ui::ControlModifier;
    const uint32_t shift = ui::ShiftModifier;
    window.addShortcut(none, ui::key::Left, [this] { step(-1); });
    window.addShortcut(none, ui::key::Right, [this] { step(1); });
    window.addShortcut(none, ui::key::PageUp, [this] { step(-1); });
    window.addShortcut(none, ui::key::PageDown, [this] { step(1); });
    window.addShortcut(shift, ui::key::Space, [this] { step(-1); });
    window.addShortcut(none, ui::key::Space, [this] { step(1); });
    window.addShortcut(none, ui::key::Home, [this] { goTo(0); });
    window.addShortcut(none, ui::key::End, [this] { goTo(count() - 1); });
    window.addShortcut(ctrl, ui::key::letter('='), [this] { zoomIn(); });
    window.addShortcut(ctrl, ui::key::letter('+'), [this] { zoomIn(); });
    window.addShortcut(ctrl | shift, ui::key::letter('+'), [this] { zoomIn(); });
    window.addShortcut(ctrl, ui::key::letter('-'), [this] { zoomOut(); });
    window.addShortcut(ctrl, ui::key::letter('0'), [this] { zoomToFit(); });
    window.addShortcut(ctrl, ui::key::letter('9'), [this] { zoomToActualSize(); });
    window.addShortcut(ctrl, ui::key::letter('o'), [this] { openDialog(); });
    window.addShortcut(ctrl, ui::key::letter('w'), [this] { close(); });
    window.addShortcut(ctrl, ui::key::letter('q'), [this] { m_application.quit(); });
    // F and F11, as in the Video Player.
    window.addShortcut(none, ui::key::letter('f'), [this] { toggleFullScreen(); });
    window.addShortcut(none, ui::key::F1 + 10, [this] { toggleFullScreen(); });
    window.addShortcut(none, ui::key::Escape, [this] {
        if (isFullScreen())
            toggleFullScreen();
    });
}
