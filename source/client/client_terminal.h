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

#ifndef CLIENT_CLIENT_TERMINAL_H
#define CLIENT_CLIENT_TERMINAL_H

#include <memory>

#include "client/client.h"

class ZstdStreamDecompressor;

class ClientTerminal final : public Client
{
    Q_OBJECT

public:
    explicit ClientTerminal(QObject* parent = nullptr);
    ~ClientTerminal() final;

public slots:
    void sendCredentials(const QString& user_name, const QString& password);
    void sendInput(const QByteArray& data);
    void sendResize(int columns, int rows);

signals:
    void sig_outputReceived(const QByteArray& data);
    void sig_resultReceived(int result_code);

protected:
    // Client implementation.
    void onStarted() final;
    void onMessageReceived(quint8 channel_id, const QByteArray& buffer) final;

private:
    // Streaming zstd decompressor for the host -> client output, paired with the host's compressor.
    std::unique_ptr<ZstdStreamDecompressor> output_decompressor_;

    Q_DISABLE_COPY_MOVE(ClientTerminal)
};

#endif // CLIENT_CLIENT_TERMINAL_H
