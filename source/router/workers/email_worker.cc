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

#include "router/workers/email_worker.h"

#include <QDateTime>
#include <QSocketNotifier>

#include <cstring>

#include "base/logging.h"
#include "base/crypto/random.h"
#include "base/crypto/secure_string.h"
#include "base/peer/router_user.h"
#include "base/threading/asio_event_dispatcher.h"
#include "router/settings.h"

namespace {

const Seconds kFirstRetryDelay{ 10 };
const Seconds kMaxRetryDelay{ 600 };

//--------------------------------------------------------------------------------------------------
// RFC 2047 limits an encoded word to 75 characters, so a long text is split into several words that
// are folded onto separate lines. A character is never split between two words.
QByteArray encodeHeaderText(const QString& text)
{
    const qsizetype kMaxChunkSize = 45;

    const QByteArray utf8 = text.toUtf8();
    QByteArray result;
    qsizetype offset = 0;

    while (offset < utf8.size())
    {
        qsizetype size = std::min(kMaxChunkSize, utf8.size() - offset);

        while (offset + size < utf8.size() && (static_cast<quint8>(utf8[offset + size]) & 0xC0) == 0x80)
            --size;

        if (!result.isEmpty())
            result += "\r\n ";

        result += "=?UTF-8?B?" + utf8.mid(offset, size).toBase64() + "?=";
        offset += size;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
QByteArray encodeBody(const QString& text)
{
    const qsizetype kLineLength = 76;

    QString canonical = text;
    canonical.replace("\r\n", "\n");
    canonical.replace('\n', "\r\n");

    const QByteArray base64 = canonical.toUtf8().toBase64();
    QByteArray result;

    for (qsizetype i = 0; i < base64.size(); i += kLineLength)
        result += base64.mid(i, kLineLength) + "\r\n";

    return result;
}

//--------------------------------------------------------------------------------------------------
QByteArray makeMessage(const QByteArray& from, const QByteArray& to, const QString& subject,
    const QString& text, qint64 time)
{
    const QByteArray domain = from.mid(from.indexOf('@') + 1);

    QByteArray message;
    message += "Date: " + QDateTime::fromSecsSinceEpoch(time).toString(Qt::RFC2822Date).toLatin1() + "\r\n";
    message += "From: " + from + "\r\n";
    message += "To: " + to + "\r\n";
    message += "Message-ID: <" + Random::byteArray(16).toHex() + "@" + domain + ">\r\n";
    message += "Subject: " + encodeHeaderText(subject) + "\r\n";
    message += "MIME-Version: 1.0\r\n";
    message += "Content-Type: text/plain; charset=UTF-8\r\n";
    message += "Content-Transfer-Encoding: base64\r\n";
    message += "\r\n";
    message += encodeBody(text);
    return message;
}

} // namespace

//--------------------------------------------------------------------------------------------------
EmailWorker::EmailWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1)),
      retry_delay_(kFirstRetryDelay)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
EmailWorker::~EmailWorker()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::send(const QString& to, const EmailMessages::Message& message)
{
    post([this, to, message]()
    {
        doSend(to, message.subject, message.text);
    });
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::onPrepare()
{
    Settings settings;

    const QString server = settings.smtpServer();
    if (server.isEmpty())
    {
        LOG(INFO) << "SMTP server is not configured, emails are not sent";
        return;
    }

    const QString from = settings.smtpFrom();
    if (!RouterUser::isValidEmail(from))
    {
        LOG(ERROR) << "Invalid sender address in configuration file:" << from;
        return;
    }

    const quint16 port = settings.smtpPort();
    if (!port)
    {
        LOG(ERROR) << "Invalid SMTP port specified in configuration file";
        return;
    }

    const Settings::SmtpSecurity security = settings.smtpSecurity();

    QUrl url;
    url.setScheme(security == Settings::SmtpSecurity::TLS ? "smtps" : "smtp");
    url.setHost(server);
    url.setPort(port);

    if (!url.isValid() || url.host().isEmpty())
    {
        LOG(ERROR) << "Invalid SMTP server address:" << server;
        return;
    }

    const QByteArray encoded_url = url.toEncoded();
    const long use_ssl = security == Settings::SmtpSecurity::NONE ? CURLUSESSL_NONE : CURLUSESSL_ALL;
    const QByteArray user = settings.smtpUser().toUtf8();
    const SecureByteArray password = settings.smtpPassword().toUtf8();
    const QByteArray envelope_sender = "<" + from.toLatin1() + ">";

    from_ = from.toLatin1();

    if (!queue_.open(EmailQueue::filePath()))
    {
        LOG(ERROR) << "Unable to open the email queue, emails are not sent";
        return;
    }

    LOG(INFO) << "Emails are sent through" << encoded_url
              << (use_ssl == CURLUSESSL_ALL ? "with TLS" : "without TLS") << "from" << from_;

    multi_ = std::make_unique<ScopedCURLM>();
    curl_ = std::make_unique<ScopedCURL>();
    multi_timer_ = std::make_unique<asio::steady_timer>(AsioEventDispatcher::ioContext());

    curl_multi_setopt(multi_->get(), CURLMOPT_SOCKETFUNCTION, socketCallback);
    curl_multi_setopt(multi_->get(), CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(multi_->get(), CURLMOPT_TIMERFUNCTION, timerCallback);
    curl_multi_setopt(multi_->get(), CURLMOPT_TIMERDATA, this);

    const long kConnectTimeout = 30; // Seconds.
    const long kTimeout = 120; // Seconds.

    // The handle is reused for every email, so what does not change between them is set once. curl
    // copies the strings.
    CURL* curl = curl_->get();

    curl_easy_setopt(curl, CURLOPT_URL, encoded_url.constData());
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "smtp,smtps");
    curl_easy_setopt(curl, CURLOPT_USE_SSL, use_ssl);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, kConnectTimeout);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, kTimeout);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, envelope_sender.constData());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readCallback);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_);

    if (!user.isEmpty())
    {
        curl_easy_setopt(curl, CURLOPT_USERNAME, user.constData());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password.constData());
    }

    if (qEnvironmentVariableIsSet("ASPIA_NO_VERIFY_TLS_PEER"))
    {
        LOG(INFO) << "ASPIA_NO_VERIFY_TLS_PEER defined";
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    }
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::onStart()
{
    if (multi_)
        sendNext();
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::onStop()
{
    if (!multi_)
        return;

    multi_timer_.reset();

    // curl closes its sockets while it cleans up, so the notifiers stop watching them before that.
    curl_multi_setopt(multi_->get(), CURLMOPT_SOCKETFUNCTION, nullptr);
    curl_multi_setopt(multi_->get(), CURLMOPT_TIMERFUNCTION, nullptr);

    for (auto& [socket, notifiers] : sockets_)
        notifiers.setInterest(CURL_POLL_NONE);
    sockets_.clear();

    // While a retry waits the handle is not in the multi.
    if (current_ && retry_time_ == TimePoint::max())
        curl_multi_remove_handle(multi_->get(), curl_->get());

    current_.reset();
    curl_.reset();
    multi_.reset();
    queue_.close();
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::onTimer(TimePoint now)
{
    if (now < retry_time_)
        return;

    retry_time_ = TimePoint::max();
    sendCurrent();
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::SocketNotifiers::setInterest(int what)
{
    read->setEnabled(what & CURL_POLL_IN);
    write->setEnabled(what & CURL_POLL_OUT);
}

//--------------------------------------------------------------------------------------------------
// static
int EmailWorker::socketCallback(CURL* /* easy */, curl_socket_t socket, int what, void* userp, void* /* socketp */)
{
    static_cast<EmailWorker*>(userp)->watchSocket(socket, what);
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
int EmailWorker::timerCallback(CURLM* /* multi */, long timeout_ms, void* userp)
{
    static_cast<EmailWorker*>(userp)->setMultiTimeout(timeout_ms);
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
size_t EmailWorker::readCallback(char* buffer, size_t size, size_t count, void* userp)
{
    Email* email = static_cast<Email*>(userp);

    const qsizetype length =
        std::min(static_cast<qsizetype>(size * count), email->data.size() - email->offset);

    std::memcpy(buffer, email->data.constData() + email->offset, static_cast<size_t>(length));
    email->offset += length;

    return static_cast<size_t>(length);
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::doSend(const QString& to, const QString& subject, const QString& text)
{
    if (!multi_)
        return;

    if (!RouterUser::isValidEmail(to))
    {
        LOG(ERROR) << "Invalid email address:" << to;
        return;
    }

    if (!queue_.add(to, subject, text))
    {
        LOG(ERROR) << "Unable to queue email to" << to;
        return;
    }

    if (!current_)
        sendNext();
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::sendNext()
{
    EmailQueue::Record record;
    if (!queue_.front(&record))
        return;

    const QByteArray recipient = record.to.toLatin1();
    const QByteArray envelope_recipient = "<" + recipient + ">";

    Email& email = current_.emplace();
    email.id = record.id;
    email.to = record.to;
    email.data = makeMessage(from_, recipient, record.subject, record.text, record.time);

    if (!email.recipients.append(envelope_recipient.constData()))
    {
        LOG(ERROR) << "Unable to add recipient:" << record.to;
        scheduleRetry();
        return;
    }

    sendCurrent();
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::sendCurrent()
{
    Email& email = *current_;
    email.offset = 0;

    CURL* curl = curl_->get();

    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, email.recipients.get());
    curl_easy_setopt(curl, CURLOPT_READDATA, &email);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(email.data.size()));

    CURLMcode error_code = curl_multi_add_handle(multi_->get(), curl);
    if (error_code != CURLM_OK)
    {
        LOG(ERROR) << "Unable to send email to" << email.to << ":" << curl_multi_strerror(error_code);
        scheduleRetry();
    }
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::scheduleRetry()
{
    LOG(INFO) << "Email to" << current_->to << "is retried in" << retry_delay_.count() << "seconds";

    retry_time_ = Clock::now() + retry_delay_;
    retry_delay_ = std::min(retry_delay_ * 2, kMaxRetryDelay);
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::watchSocket(curl_socket_t socket, int what)
{
    if (what == CURL_POLL_REMOVE)
    {
        auto it = sockets_.find(socket);
        if (it == sockets_.end())
            return;

        // The deletion of the notifiers is deferred, and curl closes the socket right after this. The
        // system can give the same descriptor to a new socket, so the notifiers stop watching it now.
        it->second.setInterest(CURL_POLL_NONE);
        sockets_.erase(it);
        return;
    }

    SocketNotifiers& notifiers = sockets_[socket];
    if (!notifiers.read)
    {
        // A notifier with a socket in its constructor starts enabled, this way they start disabled.
        const qintptr descriptor = static_cast<qintptr>(socket);

        notifiers.read = new QSocketNotifier(QSocketNotifier::Read, this);
        notifiers.read->setSocket(descriptor);
        connect(notifiers.read.get(), &QSocketNotifier::activated, this, [this, socket]()
        {
            driveMulti(socket, CURL_CSELECT_IN);
        });

        notifiers.write = new QSocketNotifier(QSocketNotifier::Write, this);
        notifiers.write->setSocket(descriptor);
        connect(notifiers.write.get(), &QSocketNotifier::activated, this, [this, socket]()
        {
            driveMulti(socket, CURL_CSELECT_OUT);
        });
    }

    notifiers.setInterest(what);
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::setMultiTimeout(long timeout_ms)
{
    if (timeout_ms < 0)
    {
        multi_timer_->cancel();
        return;
    }

    multi_timer_->expires_after(MilliSeconds(timeout_ms));
    multi_timer_->async_wait([this](const std::error_code& error_code)
    {
        if (!error_code)
            driveMulti(CURL_SOCKET_TIMEOUT, 0);
    });
}

//--------------------------------------------------------------------------------------------------
void EmailWorker::driveMulti(curl_socket_t socket, int action)
{
    int running = 0;
    CURLMcode error_code = curl_multi_socket_action(multi_->get(), socket, action, &running);
    if (error_code != CURLM_OK)
        LOG(ERROR) << "curl_multi_socket_action failed:" << curl_multi_strerror(error_code);

    int messages_left = 0;
    while (CURLMsg* message = curl_multi_info_read(multi_->get(), &messages_left))
    {
        if (message->msg != CURLMSG_DONE)
            continue;

        const CURLcode result = message->data.result;
        const Email& email = *current_;

        // The last reply of the server. A 5xx reply is a permanent refusal (RFC 5321), the email is
        // dropped so that it does not hold back the ones behind it. Everything else is retried.
        long response_code = 0;
        curl_easy_getinfo(curl_->get(), CURLINFO_RESPONSE_CODE, &response_code);

        curl_multi_remove_handle(multi_->get(), curl_->get());

        if (result == CURLE_OK)
        {
            LOG(INFO) << "Email sent to" << email.to;
        }
        else if (response_code / 100 == 5)
        {
            LOG(ERROR) << "Email to" << email.to << "is rejected by the server:" << curl_easy_strerror(result)
                       << "(" << error_ << ")";
        }
        else
        {
            LOG(ERROR) << "Unable to send email to" << email.to << ":" << curl_easy_strerror(result)
                       << "(" << error_ << ")";
            scheduleRetry();
            continue;
        }

        retry_delay_ = kFirstRetryDelay;
        queue_.remove(email.id);
        current_.reset();
        sendNext();
    }
}
