//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef COMMON_CLIPBOARD_ANDROID_H
#define COMMON_CLIPBOARD_ANDROID_H

#include "common/clipboard.h"

class ClipboardAndroid final : public Clipboard
{
    Q_OBJECT

public:
    explicit ClipboardAndroid(QObject* parent = nullptr);
    ~ClipboardAndroid() final = default;

protected:
    // Clipboard implementation.
    void init() final;
    void setData(const proto::clipboard::Event& event) final;

private slots:
    void onDataChanged();

private:
    QString injected_text_;
    QString injected_html_;
    bool has_injected_data_ = false;

    Q_DISABLE_COPY_MOVE(ClipboardAndroid)
};

#endif // COMMON_CLIPBOARD_ANDROID_H
