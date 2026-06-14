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

#ifndef CLIENT_ANDROID_LOCAL_WIDGET_H
#define CLIENT_ANDROID_LOCAL_WIDGET_H

#include <QList>
#include <QWidget>

class IconButton;
class Label;

// Local screen for the Android client: the address book stored locally on the device. Its entries
// may connect either directly or through a router.
class LocalWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit LocalWidget(QWidget* parent = nullptr);
    ~LocalWidget() final;

    // Action buttons hosted in the app bar while the local screen is active.
    QList<QWidget*> appBarActions() const;

    void retranslate();

private:
    IconButton* search_button_ = nullptr;
    Label* placeholder_ = nullptr;

    Q_DISABLE_COPY_MOVE(LocalWidget)
};

#endif // CLIENT_ANDROID_LOCAL_WIDGET_H
