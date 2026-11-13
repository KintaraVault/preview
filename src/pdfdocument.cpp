// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pdfdocument.h"

#include "base/mime_database.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace cutefish;
using Urgency = PdfWorker::Urgency;

PdfDocument::PdfDocument(std::string path)
    : m_path(std::move(path))
    , m_worker(std::make_unique<PdfWorker>())
{
    m_opening = m_worker->open(m_path, {}, [this](int status) { opened(status); });
}

PdfDocument::~PdfDocument() = default;

bool PdfDocument::isPdf(const std::string& path)
{
    return base::mime::typeForFileName(std::filesystem::path(path).filename().string()) == "application/pdf";
}

void PdfDocument::opened(int status)
{
    switch (pdf::Document::Status(status)) {
    case pdf::Document::Status::PasswordRequired:
        m_state = State::Locked;
        break;
    case pdf::Document::Status::PasswordWrong:
        m_state = State::WrongPassword;
        break;
    case pdf::Document::Status::Unreadable:
        m_state = State::Unreadable;
        break;
    case pdf::Document::Status::Opened:
        m_info = m_worker->run(
                Urgency::Now,
                [](const pdf::Document* document, const std::atomic<bool>*) {
                    std::vector<ui::SizeF> sizes;
                    for (const base::SizeF& size : document->pageSizes())
                        sizes.push_back({size.width(), size.height()});
                    return sizes;
                },
                [this](std::vector<ui::SizeF> sizes) {
                    m_sizes = std::move(sizes);
                    m_state = m_sizes.empty() ? State::Unreadable : State::Ready;
                    stateChanged();
                });
        return;
    }
    stateChanged();
}
