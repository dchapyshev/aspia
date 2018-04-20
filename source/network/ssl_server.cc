//
// PROJECT:         Aspia
// FILE:            network/ssl_server.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "network/ssl_server.h"

#include <QDebug>
#include <QFile>
#include <QSslSocket>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

namespace aspia {

SslServer::SslServer(QObject* parent)
    : QTcpServer(parent)
{
    // Nothing
}

SslServer::~SslServer() = default;

// static
bool SslServer::generateCertificateAndKey(QSslCertificate* certificate, QSslKey* key)
{
    if (!certificate || !key)
        return false;

    bool result = false;

    BIGNUM* big_number = nullptr;
    RSA* rsa = nullptr;
    X509* x509 = nullptr;
    BIO* bp_public = nullptr;
    BIO* bp_private = nullptr;

    EVP_PKEY* pkey = EVP_PKEY_new();
    if (!pkey)
    {
        qWarning("EVP_PKEY_new failed");
        goto cleanup;
    }

    big_number = BN_new();
    if (!big_number)
    {
        qWarning("BN_new failed");
        goto cleanup;
    }

    if (!BN_set_word(big_number, RSA_F4))
    {
        qWarning("BN_set_word failed");
        goto cleanup;
    }

    rsa = RSA_new();
    if (!rsa)
    {
        qWarning("RSA_new failed");
        goto cleanup;
    }

    if (!RSA_generate_key_ex(rsa, 2048, big_number, nullptr))
    {
        qWarning("RSA_generate_key_ex failed");
        goto cleanup;
    }

    if (!EVP_PKEY_assign_RSA(pkey, rsa))
    {
        qWarning("EVP_PKEY_assign_RSA failed");
        goto cleanup;
    }

    // The |rsa| key now belongs to the |pkey|. Deleting |pkey| will also delete |rsa|.
    rsa = nullptr;

    x509 = X509_new();
    if (!x509)
    {
        qWarning("X509_new failed");
        goto cleanup;
    }

    if (!ASN1_INTEGER_set(X509_get_serialNumber(x509), 1))
    {
        qWarning("ASN1_INTEGER_set failed");
        goto cleanup;
    }

    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L);
    X509_set_pubkey(x509, pkey);

    // |name| is an internal pointer which must not be freed.
    X509_NAME* name = X509_get_subject_name(x509);
    if (!name)
    {
        qWarning("X509_get_subject_name failed");
        goto cleanup;
    }

    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, reinterpret_cast<quint8*>("US"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<quint8*>("Aspia"), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<quint8*>("Aspia"), -1, -1, 0);

    X509_set_issuer_name(x509, name);
    X509_sign(x509, pkey, EVP_sha256());

    bp_private = BIO_new(BIO_s_mem());
    if (!bp_private)
    {
        qWarning("BIO_new failed");
        goto cleanup;
    }

    if (!PEM_write_bio_PrivateKey(bp_private, pkey, nullptr, nullptr, 0, nullptr, nullptr))
    {
        qWarning("PEM_write_bio_PrivateKey failed");
        goto cleanup;
    }

    bp_public = BIO_new(BIO_s_mem());
    if (!bp_public)
    {
        qWarning("BIO_new failed");
        goto cleanup;
    }

    if (!PEM_write_bio_X509(bp_public, x509))
    {
        qWarning("PEM_write_bio_PrivateKey failed");
        goto cleanup;
    }

    const char* buffer = nullptr;

    long size = BIO_get_mem_data(bp_public, &buffer);
    if (!buffer)
    {
        qWarning("BIO_get_mem_data failed");
        goto cleanup;
    }

    *certificate = QSslCertificate(QByteArray(buffer, size));
    if (certificate->isNull())
    {
        qWarning("Wrong local certificate");
        goto cleanup;
    }

    size = BIO_get_mem_data(bp_private, &buffer);
    if (!buffer)
    {
        qWarning("BIO_get_mem_data failed");
        goto cleanup;
    }

    *key = QSslKey(QByteArray(buffer, size), QSsl::Rsa);
    if (key->isNull())
    {
        qWarning("Wrong private key");
        goto cleanup;
    }

    result = true;

cleanup:
    BIO_free_all(bp_public);
    BIO_free_all(bp_private);
    X509_free(x509);
    RSA_free(rsa);
    BN_free(big_number);
    EVP_PKEY_free(pkey);

    return result;
}

QSslCertificate SslServer::localCertificate() const
{
    return local_certificate_;
}

void SslServer::setLocalCertificate(const QSslCertificate& certificate)
{
    local_certificate_ = certificate;
}

bool SslServer::setLocalCertificate(const QString& file_path, QSsl::EncodingFormat format)
{
    QFile certificate_file(file_path);

    if (!certificate_file.open(QFile::ReadOnly))
    {
        qWarning() << "Unable to open certificate file '" << file_path << "': "
                   << certificate_file.errorString();
        return false;
    }

    local_certificate_ = QSslCertificate(certificate_file.readAll(), format);
    return true;
}

QSslKey SslServer::privateKey() const
{
    return private_key_;
}

void SslServer::setPrivateKey(const QSslKey& key)
{
    private_key_ = key;
}

bool SslServer::setPrivateKey(const QString& file_path,
                              QSsl::KeyAlgorithm algorithm,
                              QSsl::EncodingFormat format,
                              const QByteArray& password)
{
    QFile key_file(file_path);

    if (!key_file.open(QFile::ReadOnly))
    {
        qWarning() << "Unable to open key file '" << file_path << "': "
            << key_file.errorString();
        return false;
    }

    private_key_ = QSslKey(key_file.readAll(), algorithm, format, QSsl::PrivateKey, password);
    return true;
}

void SslServer::incomingConnection(qintptr socket_descriptor)
{
    QSslSocket* socket = new QSslSocket();

    if (!socket->setSocketDescriptor(socket_descriptor))
    {
        qWarning("Invalid socket descriptor passed");
        delete socket;
    }
    else
    {
        // We only allow TLS 1.2 and above.
        socket->setProtocol(QSsl::SslProtocol::TlsV1_2OrLater);

        socket->setLocalCertificate(local_certificate_);
        socket->setPrivateKey(private_key_);

        addPendingConnection(socket);

        connect(socket, &QSslSocket::encrypted, this, &SslServer::newSslConnection);

        // Start TLS handshake.
        socket->startServerEncryption();
    }
}

} // namespace aspia
