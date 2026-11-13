// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pdfpage.h"
#include "passworddialog.h"
#include "pdfview.h"

#include "base/i18n.h"
#include "ui/cell_layout.h"
#include "ui/dialog_window.h"
#include "ui/label.h"
#include "ui/theme.h"
#include "ui/window.h"

#include <filesystem>

using namespace cutefish;
using cutefish::i18n::tr;
using ui::rounding::centeredStart;

PdfPage::PdfPage()
{
    m_view = addChild<PdfView>();
    m_message = addChild<ui::Label>();
    m_message->setVisible(false);
    m_unlock = addChild<ui::DialogButton>();
    m_unlock->setVisible(false);
    m_unlock->onClicked = [this] { askPassword(); };

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
    m_password.reset();
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
    m_password.reset();
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
    m_unlock->setText(tr("Enter Password…"));
    if (m_password)
        m_password->retranslate();
    updateMessage();
}

void PdfPage::stateChanged()
{
    switch (m_document->state()) {
    case PdfDocument::State::Ready:
        if (m_password)
            m_password->close();
        m_view->setDocument(m_document.get());
        break;
    case PdfDocument::State::WrongPassword:
        if (m_password && m_password->isOpen())
            m_password->retry();
        break;
    case PdfDocument::State::Locked:
    case PdfDocument::State::Unreadable:
    case PdfDocument::State::Opening:
        break;
    }
    updateMessage();
    if (onStateChanged)
        onStateChanged();
}

void PdfPage::askPassword()
{
    if (!m_document || !window())
        return;
    if (!m_password || !m_password->isOpen()) {
        m_password = std::make_unique<PasswordDialog>(window(), std::filesystem::path(m_path).filename().string());
        m_password->onAccepted = [this](const std::string& password) { m_document->unlock(password); };
    }
    m_password->show();
}

bool PdfPage::asksPassword() const
{
    return m_password && m_password->isOpen();
}

void PdfPage::updateMessage()
{
    const PdfDocument::State state = m_document ? m_document->state() : PdfDocument::State::Opening;
    const bool locked = state == PdfDocument::State::Locked || state == PdfDocument::State::WrongPassword;
    const bool unreadable = state == PdfDocument::State::Unreadable;
    m_message->setText(locked ? tr("This document is protected by a password.")
                              : tr("This document could not be opened."));
    m_message->setVisible(locked || unreadable);
    m_unlock->setVisible(locked);
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
    const ui::SizeF button = m_unlock->sizeHint();
    const double height = message.height + (m_unlock->isVisible() ? 16 + button.height : 0);
    const double top = centeredStart(rect.height, height);
    m_message->setGeometry({centeredStart(rect.width, message.width), top, message.width, message.height});
    m_unlock->setGeometry({centeredStart(rect.width, button.width), top + message.height + 16, button.width,
                           button.height});
}
