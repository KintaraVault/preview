// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pdfpage.h"
#include "pdfview.h"

#include "base/i18n.h"
#include "ui/cell_layout.h"
#include "ui/label.h"
#include "ui/theme.h"

using namespace cutefish;
using cutefish::i18n::tr;
using ui::rounding::centeredStart;

PdfPage::PdfPage()
{
    m_view = addChild<PdfView>();
    m_message = addChild<ui::Label>();
    m_message->setVisible(false);

    retranslate();
}

PdfPage::~PdfPage()
{
    // Before the document it draws.
    m_view->setDocument(nullptr);
}

void PdfPage::load(const std::string& path)
{
    m_path = path;
    m_view->setDocument(nullptr);
    m_subscriptions.clear();
    if (m_document && onDocumentReplaced)
        onDocumentReplaced();
    m_document = std::make_unique<PdfDocument>(path);
    m_subscriptions.push_back(m_document->stateChanged.connect([this] { stateChanged(); }));
    updateMessage();
}

void PdfPage::clear()
{
    m_view->setDocument(nullptr);
    m_subscriptions.clear();
    if (m_document && onDocumentReplaced)
        onDocumentReplaced();
    m_document.reset();
    m_path.clear();
    updateMessage();
}

bool PdfPage::isReady() const
{
    return m_document && m_document->state() == PdfDocument::State::Ready;
}

void PdfPage::retranslate()
{
    updateMessage();
}

void PdfPage::stateChanged()
{
    switch (m_document->state()) {
    case PdfDocument::State::Ready:
        m_view->setDocument(m_document.get());
        break;
    case PdfDocument::State::WrongPassword:
    case PdfDocument::State::Locked:
    case PdfDocument::State::Unreadable:
    case PdfDocument::State::Opening:
        break;
    }
    updateMessage();
    if (onStateChanged)
        onStateChanged();
}

void PdfPage::updateMessage()
{
    const PdfDocument::State state = m_document ? m_document->state() : PdfDocument::State::Opening;
    const bool locked = state == PdfDocument::State::Locked || state == PdfDocument::State::WrongPassword;
    const bool unreadable = state == PdfDocument::State::Unreadable;
    m_message->setText(locked ? tr("This document is protected by a password.")
                              : tr("This document could not be opened."));
    m_message->setVisible(locked || unreadable);
    m_view->setVisible(state == PdfDocument::State::Ready);
    requestLayout();
}

void PdfPage::layout()
{
    const ui::RectF rect = localRect();
    m_view->setGeometry(rect);
    const ui::Theme& theme = ui::Theme::current();
    m_message->setColor(theme.text);
    const ui::SizeF message = m_message->sizeHint();
    const double top = centeredStart(rect.height, message.height);
    m_message->setGeometry({centeredStart(rect.width, message.width), top, message.width, message.height});
}
