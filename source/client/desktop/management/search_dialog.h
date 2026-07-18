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

#ifndef CLIENT_DESKTOP_MANAGEMENT_SEARCH_DIALOG_H
#define CLIENT_DESKTOP_MANAGEMENT_SEARCH_DIALOG_H

#include <QDialog>

#include <memory>

namespace Ui {
class SearchDialog;
} // namespace Ui

class SearchDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit SearchDialog(QWidget* parent = nullptr);
    ~SearchDialog() final;

    void setSearchText(const QString& text);

signals:
    void sig_searchTextChanged(const QString& text);

protected:
    // QWidget implementation.
    void showEvent(QShowEvent* event) final;

private:
    std::unique_ptr<Ui::SearchDialog> ui;

    Q_DISABLE_COPY_MOVE(SearchDialog)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_SEARCH_DIALOG_H
