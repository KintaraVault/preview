// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "pdfdocument.h"

#include "base/signal.h"
#include "ui/view.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cutefish::ui {
class Label;
}

class PdfView;

// A PDF under the header: its pages; a file that cannot be read, or stays
// locked, says so instead.
class PdfPage : public cutefish::ui::View
{
public:
    PdfPage();
    ~PdfPage() override;

    // Opens the file on a worker; onStateChanged tells how it went.
    void load(const std::string& path);
    void clear();
    void retranslate();

    // Null before load().
    PdfDocument* document() const { return m_document.get(); }
    const std::string& path() const { return m_path; }
    bool isReady() const;
    PdfView* view() const { return m_view; }

    // Opened, locked or unreadable.
    std::function<void()> onStateChanged;
    // The document is about to go, for a new one or none.
    std::function<void()> onDocumentReplaced;

protected:
    void layout() override;

private:
    void stateChanged();
    void updateMessage();

    std::string m_path;
    std::unique_ptr<PdfDocument> m_document;
    std::vector<cutefish::base::Subscription> m_subscriptions;
    PdfView* m_view;
    cutefish::ui::Label* m_message;
};
