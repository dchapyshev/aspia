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

#ifndef CLIENT_ANDROID_CREDENTIAL_SELECTION_LIST_H
#define CLIENT_ANDROID_CREDENTIAL_SELECTION_LIST_H

#include <QList>
#include <QWidget>

class QVBoxLayout;
class Switch;

// A scrolling list of two-line items, each selected with a switch at its trailing end. A separator
// at the bottom sets the list off from the panel below it.
class CredentialSelectionList final : public QWidget
{
    Q_OBJECT

public:
    struct Item
    {
        QString name;
        QString details;
        QString status;
        bool enabled = true;
        bool checked = false;
    };

    explicit CredentialSelectionList(QWidget* parent = nullptr);
    ~CredentialSelectionList() final;

    void setItems(const QList<Item>& items);

    // Selects every enabled item unless all of them are selected already, then clears the selection.
    void toggleAll();

    // Indexes of the selected items, in the order they were given to setItems().
    QList<int> checkedItems() const;

signals:
    void sig_selectionChanged();

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;

private:
    QVBoxLayout* list_layout_ = nullptr;
    QList<QWidget*> rows_;
    QList<Switch*> switches_;

    Q_DISABLE_COPY_MOVE(CredentialSelectionList)
};

#endif // CLIENT_ANDROID_CREDENTIAL_SELECTION_LIST_H
