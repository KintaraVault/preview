// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "pdfworker.h"

#include "cutefish/pdf/document.h"

#include "base/lru_cache.h"
#include "base/signal.h"
#include "ui/geometry.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

// A place in the text of the document: before character `index` of `page`.
struct TextPosition
{
    int page = 0;
    size_t index = 0;

    auto operator<=>(const TextPosition&) const = default;
};

// A PDF as the UI sees it, everything read on its worker. Asking for
// something not there yet asks the worker and tells by a signal when it
// arrives.
class PdfDocument
{
public:
    enum class State {
        Opening,
        // The file wants its password, or the one given was wrong.
        Locked,
        WrongPassword,
        Unreadable,
        Ready,
    };

    explicit PdfDocument(std::string path);
    ~PdfDocument();

    PdfDocument(const PdfDocument&) = delete;
    PdfDocument& operator=(const PdfDocument&) = delete;

    // By its name.
    static bool isPdf(const std::string& path);

    const std::string& path() const { return m_path; }
    State state() const { return m_state; }

    int pageCount() const { return int(m_sizes.size()); }
    // In points, as shown.
    const std::vector<cutefish::ui::SizeF>& pageSizes() const { return m_sizes; }

    // Null until it arrives.
    const cutefish::pdf::PageText* text(int page, PdfWorker::Urgency urgency = PdfWorker::Urgency::Near);
    const std::vector<cutefish::pdf::Link>* links(int page, PdfWorker::Urgency urgency = PdfWorker::Urgency::Near);
    // Text and links asked for pages outside these are not read after all.
    void keepPageRequests(int first, int last);

    // The point of the page as shown it goes to. The caller keeps the request.
    [[nodiscard]] PdfRequest resolve(const cutefish::pdf::Destination& destination,
                 std::function<void(int page, cutefish::ui::PointF point)> done);
    // UTF-8 from `from` up to `to`, lines of separate pages apart.
    [[nodiscard]] PdfRequest readText(TextPosition from, TextPosition to, std::function<void(std::string text)> done);

    PdfWorker& worker() { return *m_worker; }

    cutefish::base::Signal<> stateChanged;
    // Its text or links arrived.
    cutefish::base::Signal<int> pageDataChanged;

private:
    struct PageData
    {
        std::optional<cutefish::pdf::PageText> text;
        std::optional<std::vector<cutefish::pdf::Link>> links;
    };

    void opened(int status);
    PageData& data(int page);

    std::string m_path;
    std::unique_ptr<PdfWorker> m_worker;
    State m_state = State::Opening;
    std::vector<cutefish::ui::SizeF> m_sizes;
    PdfRequest m_opening;
    PdfRequest m_info;
    // The pages near what was asked for last; far ones are read again.
    cutefish::base::LruCache<int, PageData> m_pages;
    std::unordered_map<int, PdfRequest> m_textRequests;
    std::unordered_map<int, PdfRequest> m_linkRequests;
};
