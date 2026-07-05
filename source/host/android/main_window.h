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

#ifndef HOST_ANDROID_MAIN_WINDOW_H
#define HOST_ANDROID_MAIN_WINDOW_H

#include <QWidget>

class AppBar;
class Label;

// Top-level application window for the Android host: a top app bar with a content area. For now
// the content is a placeholder; the host sections (status, users, settings) will fill it as the
// host is ported to Android.
class AndroidMainWindow final : public QWidget
{
    Q_OBJECT

public:
    explicit AndroidMainWindow(QWidget* parent = nullptr);
    ~AndroidMainWindow() final;

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private:
    void retranslate();

    AppBar* app_bar_ = nullptr;
    Label* status_label_ = nullptr;

    Q_DISABLE_COPY_MOVE(AndroidMainWindow)
};

#endif // HOST_ANDROID_MAIN_WINDOW_H
