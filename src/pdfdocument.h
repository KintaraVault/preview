// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "pdfworker.h"

#include "cutefish/pdf/document.h"

#include "base/signal.h"
#include "ui/geometry.h"

#include <map>
#include <string>
#include <vector>

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

    PdfWorker& worker() { return *m_worker; }

    cutefish::base::Signal<> stateChanged;

private:
    void opened(int status);

    std::string m_path;
    std::unique_ptr<PdfWorker> m_worker;
    State m_state = State::Opening;
    std::vector<cutefish::ui::SizeF> m_sizes;
    PdfRequest m_opening;
    PdfRequest m_info;
};
