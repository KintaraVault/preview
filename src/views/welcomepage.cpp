// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "welcomepage.h"

#include "base/i18n.h"
#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/icon.h"
#include "ui/label.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

using namespace cutefish;
using cutefish::i18n::tr;
using ui::rounding::centeredStart;

static constexpr double kIconSize = 72;
static constexpr double kButtonHeight = 28;

static ui::Color textColor(double alpha)
{
    return ui::Theme::current().text.withAlpha(alpha);
}

static double labelHeight(const ui::Label* label)
{
    return std::ceil(label->sizeHint().height);
}

// The column under the viewport: as tall as the viewport while it fits,
// centred in it.
class WelcomePage::Column : public ui::View
{
public:
    explicit Column(WelcomePage& page)
        : m_page(page)
    {
        ui::LayoutHints hints;
        hints.fillHeight = true;
        setLayoutHints(hints);
    }

    double heightForWidth(double) const override { return m_page.stackHeight() + 24 * 2; }

protected:
    void layout() override { m_page.layoutColumn(geometry().width, geometry().height); }

private:
    WelcomePage& m_page;
};

WelcomePage::WelcomePage()
{
    setScrollDirections(false, true);
    setClipsChildren(true);
    m_column = setContent<Column>(*this);
    m_icon = m_column->addChild<ui::ThemeIcon>("accessories-document-viewer", kIconSize);
    m_title = m_column->addChild<ui::Label>();
    m_title->setFontSize(15 * 4.0 / 3);
    m_title->setWeight(500);
    m_title->setElide(ui::Label::Elide::None);
    m_subtitle = m_column->addChild<ui::Label>();
    m_subtitle->setElide(ui::Label::Elide::None);
    m_open = m_column->addChild<ui::DialogButton>(std::string(), true);
    m_open->onClicked = [this] {
        if (onOpen)
            onOpen();
    };
    retranslate();
}

void WelcomePage::retranslate()
{
    m_title->setText(tr("Drop an Image Here"));
    m_subtitle->setText(tr("Or open a file to view it."));
    m_open->setText(tr("Open…"));
    m_column->requestLayout();
}

// The column's own height, without the margin kept above and below it once
// it scrolls.
double WelcomePage::stackHeight() const
{
    double total = kIconSize + 16 + labelHeight(m_title) + 6 + labelHeight(m_subtitle) + 20 + kButtonHeight;
    return total;
}

void WelcomePage::layoutColumn(double pageWidth, double pageHeight)
{
    m_title->setColor(textColor(1));
    m_subtitle->setColor(textColor(0.65));

    const double width = std::min(pageWidth - 48, 440.0);
    const double left = centeredStart(pageWidth, width);
    double y = centeredStart(pageHeight, stackHeight());

    m_icon->setGeometry({left + centeredStart(width, kIconSize), y, kIconSize, kIconSize});
    y += kIconSize + 16;
    const auto centred = [&](ui::Label* label) {
        const double w = std::ceil(label->sizeHint().width);
        label->setGeometry({left + std::round((width - w) / 2), y, w, label->sizeHint().height});
        y += labelHeight(label);
    };
    centred(m_title);
    y += 6;
    centred(m_subtitle);
    y += 20;
    const double openWidth = std::ceil(m_open->sizeHint().width);
    m_open->setGeometry({left + std::round((width - openWidth) / 2), y, openWidth, kButtonHeight});
    y += kButtonHeight;
}
