//
// PROJECT:         Aspia
// FILE:            network/ssl_server.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_NETWORK__SSL_SERVER_H
#define _ASPIA_NETWORK__SSL_SERVER_H

#include <QSslCertificate>
#include <QSslKey>
#include <QTcpServer>

class QSslSocket;

namespace aspia {

class SslServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit SslServer(QObject* parent = nullptr);
    virtual ~SslServer();

    static bool generateCertificateAndKey(QSslCertificate* certificate, QSslKey* key);

    QSslCertificate localCertificate() const;
    void setLocalCertificate(const QSslCertificate& certificate);
    bool setLocalCertificate(const QString& file_path, QSsl::EncodingFormat format = QSsl::Pem);

    QSslKey privateKey() const;
    void setPrivateKey(const QSslKey& key);
    bool setPrivateKey(const QString& file_path,
                       QSsl::KeyAlgorithm algorithm = QSsl::Rsa,
                       QSsl::EncodingFormat format = QSsl::Pem,
                       const QByteArray& password = QByteArray());

signals:
    void newSslConnection();

protected:
    // QTcpServer implementation.
    void incomingConnection(qintptr socket_descriptor) override;

private:
    QSslCertificate local_certificate_;
    QSslKey private_key_;

    Q_DISABLE_COPY(SslServer)
};

} // namespace aspia

#endif // _ASPIA_NETWORK__SSL_SERVER_H
