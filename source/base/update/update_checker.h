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

#ifndef BASE_UPDATE_UPDATE_CHECKER_H
#define BASE_UPDATE_UPDATE_CHECKER_H

#include <QByteArray>
#include <QList>
#include <QThread>

#include "base/update/update_info.h"

class UpdateChecker final : public QThread
{
    Q_OBJECT

public:
    // Reads what |channel| offers for |package|. A name that is not a channel of ours is read as
    // the stable one, so that a setting of another version cannot send the check elsewhere.
    UpdateChecker(const QString& channel, const QString& package, QObject* parent = nullptr);
    ~UpdateChecker();

    // Replaces the address the files are read from, so that tests can serve their own.
    void setServerForTesting(const QString& server);

    // Replaces the keys the files are checked against with |public_keys|, so that tests can sign
    // with a key of their own.
    void setPublicKeysForTesting(const QList<QByteArray>& public_keys);

signals:
    void sig_checkFinished(const UpdateInfo& update_info);
    void sig_checkFailed();

protected:
    void run() final;

private:
    void check();
    QByteArray downloadSigned(const QString& url);
    QByteArray download(const QString& url);

    QString server_;
    const QString package_;
    QList<QByteArray> public_keys_;
    std::atomic_bool interrupted_ { false };

    Q_DISABLE_COPY_MOVE(UpdateChecker)
};

#endif // BASE_UPDATE_UPDATE_CHECKER_H
