//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_UI_SYS_INFO_TREE_TO_HTML_H
#define CLIENT_UI_SYS_INFO_TREE_TO_HTML_H

#include <QString>

class QTreeWidget;

namespace client {

QString treeToHtmlString(const QTreeWidget* tree);

bool treeToHtmlFile(const QTreeWidget* tree,
                    const QString& file_path,
                    QString* error_string = nullptr);

} // namespace client

#endif // CLIENT_UI_SYS_INFO_TREE_TO_HTML_H
