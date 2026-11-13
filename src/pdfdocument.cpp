// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#include "pdfdocument.h"

#include "base/mime_database.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

using namespace cutefish;
using Urgency = PdfWorker::Urgency;

static constexpr size_t kPagesKept = 64;

PdfDocument::PdfDocument(std::string path)
    : m_path(std::move(path))
    , m_worker(std::make_unique<PdfWorker>())
    , m_pages(kPagesKept)
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

PdfDocument::PageData& PdfDocument::data(int page)
{
    if (PageData* found = m_pages.find(page))
        return *found;
    m_pages.insert(page, std::make_unique<PageData>());
    return *m_pages.find(page);
}

const pdf::PageText* PdfDocument::text(int page, Urgency urgency)
{
    if (m_state != State::Ready || page < 0 || page >= pageCount())
        return nullptr;
    PageData& entry = data(page);
    if (entry.text)
        return &*entry.text;
    PdfRequest& request = m_textRequests[page];
    if (!request.isPending()) {
        request = m_worker->run(
                urgency, [page](const pdf::Document* document, const std::atomic<bool>*) { return document->text(page); },
                [this, page](pdf::PageText text) {
                    m_textRequests.erase(page);
                    data(page).text = std::move(text);
                    pageDataChanged(page);
                });
    }
    return nullptr;
}

const std::vector<pdf::Link>* PdfDocument::links(int page, Urgency urgency)
{
    if (m_state != State::Ready || page < 0 || page >= pageCount())
        return nullptr;
    PageData& entry = data(page);
    if (entry.links)
        return &*entry.links;
    PdfRequest& request = m_linkRequests[page];
    if (!request.isPending()) {
        request = m_worker->run(
                urgency, [page](const pdf::Document* document, const std::atomic<bool>*) { return document->links(page); },
                [this, page](std::vector<pdf::Link> links) {
                    m_linkRequests.erase(page);
                    data(page).links = std::move(links);
                    pageDataChanged(page);
                });
    }
    return nullptr;
}

void PdfDocument::keepPageRequests(int first, int last)
{
    const auto outside = [first, last](const auto& entry) { return entry.first < first || entry.first > last; };
    std::erase_if(m_textRequests, outside);
    std::erase_if(m_linkRequests, outside);
}

PdfRequest PdfDocument::resolve(const pdf::Destination& destination, std::function<void(int page, ui::PointF point)> done)
{
    if (m_state != State::Ready || destination.page < 0 || destination.page >= pageCount())
        return {};
    return m_worker->run(
            Urgency::Now,
            [destination](const pdf::Document* document, const std::atomic<bool>*) {
                return document->position(destination);
            },
            [page = destination.page, done = std::move(done)](base::PointF point) {
                done(page, {point.x(), point.y()});
            });
}

PdfRequest PdfDocument::readText(TextPosition from, TextPosition to, std::function<void(std::string text)> done)
{
    if (m_state != State::Ready || to <= from)
        return {};
    return m_worker->run(
            Urgency::Now,
            [from, to](const pdf::Document* document, const std::atomic<bool>* cancelled) {
                std::string text;
                for (int page = from.page; page <= to.page && !cancelled->load(); ++page) {
                    const pdf::PageText pageText = document->text(page);
                    const size_t size = pageText.characters.size();
                    const size_t first = page == from.page ? std::min(from.index, size) : 0;
                    const size_t last = page == to.page ? std::min(to.index, size) : size;
                    if (!text.empty() && page != from.page)
                        text += '\n';
                    text += pageText.text(first, last - first);
                }
                return text;
            },
            std::move(done));
}
