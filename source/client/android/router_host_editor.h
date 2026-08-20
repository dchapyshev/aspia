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

#ifndef CLIENT_ANDROID_ROUTER_HOST_EDITOR_H
#define CLIENT_ANDROID_ROUTER_HOST_EDITOR_H

#include <QWidget>

#include "base/peer/host_id.h"

class Label;
class LineEdit;

class RouterHostEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit RouterHostEditor(QWidget* parent = nullptr);
    ~RouterHostEditor() final;

    // Loads what is kept for the host |host_id| of the router |router_id| into the form. Returns
    // false when nothing can be kept for such a host; the form keeps its previous state and must
    // not be shown.
    bool prepareForEdit(qint64 router_id, HostId host_id);

signals:
    // Emitted after the credentials have been saved or forgotten.
    void sig_accepted();

private slots:
    void onSaveClicked();

private:
    void showError(const QString& message);

    LineEdit* username_ = nullptr;
    LineEdit* password_ = nullptr;
    Label* error_ = nullptr;
    qint64 router_id_ = 0;
    HostId host_id_ = kInvalidHostId;

    Q_DISABLE_COPY_MOVE(RouterHostEditor)
};

#endif // CLIENT_ANDROID_ROUTER_HOST_EDITOR_H
