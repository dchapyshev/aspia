//
// Aspia Project
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

#ifndef CLIENT_UI_FILE_TRANSFER_FILE_NAME_VALIDATOR_H
#define CLIENT_UI_FILE_TRANSFER_FILE_NAME_VALIDATOR_H

#include <QValidator>

namespace client {

class FileNameValidator final : public QValidator
{
    Q_OBJECT

public:
    explicit FileNameValidator(QObject* parent);

    // QValidator implementation.
    State validate(QString& input, int& pos) const final;
    void fixup(QString& input) const final;

signals:
    void sig_invalidNameEntered() const;

private:
    Q_DISABLE_COPY(FileNameValidator)
};

} // namespace client

#endif // CLIENT_UI_FILE_TRANSFER_FILE_NAME_VALIDATOR_H
