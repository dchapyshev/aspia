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

#ifndef CLIENT_UI_COMPUTERS_TAB_SEARCH_WIDGET_H
#define CLIENT_UI_COMPUTERS_TAB_SEARCH_WIDGET_H

#include "client/ui/computers_tab/content_widget.h"

class QTreeWidget;

namespace client {

class SearchWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit SearchWidget(QWidget* parent = nullptr);
    ~SearchWidget() override;

    void search(const QString& query);
    void clear();
    int itemCount() const override;
    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;

signals:
    void sig_computerDoubleClicked(qint64 computer_id);

private:
    QTreeWidget* tree_computer_ = nullptr;

    Q_DISABLE_COPY_MOVE(SearchWidget)
};

} // namespace client

#endif // CLIENT_UI_COMPUTERS_TAB_SEARCH_WIDGET_H
