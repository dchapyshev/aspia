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

#ifndef CLIENT_UI_COMPUTERS_TAB_LOCAL_GROUP_WIDGET_H
#define CLIENT_UI_COMPUTERS_TAB_LOCAL_GROUP_WIDGET_H

#include "client/ui/computers_tab/content_widget.h"

class QTreeWidget;

namespace client {

class BookDatabase;

class LocalGroupWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit LocalGroupWidget(BookDatabase* database, QWidget* parent = nullptr);
    ~LocalGroupWidget() override;

    void showGroup(qint64 group_id);
    int itemCount() const override;

signals:
    void sig_computerDoubleClicked(qint64 computer_id);

private:
    QTreeWidget* tree_computer_ = nullptr;
    BookDatabase* database_ = nullptr;

    Q_DISABLE_COPY_MOVE(LocalGroupWidget)
};

} // namespace client

#endif // CLIENT_UI_COMPUTERS_TAB_LOCAL_GROUP_WIDGET_H
