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

#include "client/desktop/credential_list_model.h"

#include <QIcon>

#include <algorithm>

namespace {

constexpr int kColumnCount = 2;

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialListModel::CredentialListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    collator_.setCaseSensitivity(Qt::CaseInsensitive);
    collator_.setNumericMode(true);
}

//--------------------------------------------------------------------------------------------------
CredentialListModel::~CredentialListModel() = default;

//--------------------------------------------------------------------------------------------------
void CredentialListModel::setCredentials(const QList<CredentialConfig>& credentials)
{
    beginResetModel();
    credentials_ = credentials;
    applySort();
    endResetModel();
}

//--------------------------------------------------------------------------------------------------
const CredentialConfig* CredentialListModel::credentialAt(int row) const
{
    if (row < 0 || row >= credentials_.size())
        return nullptr;

    return &credentials_[row];
}

//--------------------------------------------------------------------------------------------------
int CredentialListModel::rowOf(qint64 credential_id) const
{
    for (int i = 0; i < credentials_.size(); ++i)
    {
        if (credentials_[i].id() == credential_id)
            return i;
    }

    return -1;
}

//--------------------------------------------------------------------------------------------------
int CredentialListModel::rowCount(const QModelIndex& parent) const
{
    // A table has its rows at the root and nothing below them.
    if (parent.isValid())
        return 0;

    return static_cast<int>(credentials_.size());
}

//--------------------------------------------------------------------------------------------------
int CredentialListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return kColumnCount;
}

//--------------------------------------------------------------------------------------------------
QVariant CredentialListModel::data(const QModelIndex& index, int role) const
{
    const CredentialConfig* credential = credentialAt(index.row());
    if (!credential || index.column() < 0 || index.column() >= kColumnCount)
        return QVariant();

    const Column column = static_cast<Column>(index.column());

    if (role == Qt::DecorationRole)
    {
        if (column != Column::NAME)
            return QVariant();

        return QIcon(":/img/keys.svg");
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    return textAt(*credential, column);
}

//--------------------------------------------------------------------------------------------------
QVariant CredentialListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QVariant();

    switch (static_cast<Column>(section))
    {
        case Column::NAME:
            return tr("Name");

        case Column::USERNAME:
            return tr("User Name");
    }

    return QVariant();
}

//--------------------------------------------------------------------------------------------------
void CredentialListModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= kColumnCount)
        return;

    sort_column_ = column;
    sort_order_ = order;

    emit layoutAboutToBeChanged();

    // What the persistent indexes point at is read before the rows move. Without this the selection
    // of the view stays on the row number and ends up on another record.
    const QModelIndexList old_indexes = persistentIndexList();

    QList<qint64> selected_ids;
    selected_ids.reserve(old_indexes.size());

    for (const QModelIndex& old_index : old_indexes)
    {
        const CredentialConfig* credential = credentialAt(old_index.row());
        selected_ids.append(credential ? credential->id() : -1);
    }

    applySort();

    QModelIndexList new_indexes;
    new_indexes.reserve(old_indexes.size());

    for (int i = 0; i < old_indexes.size(); ++i)
    {
        const int row = rowOf(selected_ids[i]);
        new_indexes.append(row < 0 ? QModelIndex() : index(row, old_indexes[i].column()));
    }

    changePersistentIndexList(old_indexes, new_indexes);

    emit layoutChanged();
}

//--------------------------------------------------------------------------------------------------
QString CredentialListModel::textAt(const CredentialConfig& credential, Column column) const
{
    switch (column)
    {
        case Column::NAME:
            return credential.displayName();

        case Column::USERNAME:
            return credential.username();
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
void CredentialListModel::applySort()
{
    if (sort_column_ < 0 || sort_column_ >= kColumnCount)
        return;

    const Column column = static_cast<Column>(sort_column_);
    const bool ascending = sort_order_ == Qt::AscendingOrder;

    // Compared the way the user reads them, so "host2" comes before "host10".
    std::stable_sort(credentials_.begin(), credentials_.end(),
                     [&](const CredentialConfig& first, const CredentialConfig& second)
    {
        const int result = collator_.compare(textAt(first, column), textAt(second, column));
        return ascending ? result < 0 : result > 0;
    });
}
