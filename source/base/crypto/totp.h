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

#ifndef BASE_CRYPTO_TOTP_H
#define BASE_CRYPTO_TOTP_H

#include <QByteArray>
#include <QString>

// Time-based One-Time Password (RFC 6238) on top of HOTP (RFC 4226) using HMAC-SHA1,
// matching the parameters expected by Google Authenticator and Microsoft Authenticator
// (30-second step, 6 decimal digits).
class Totp
{
public:
    static const int kDefaultStepSec = 30;
    static const int kDefaultDigits = 6;
    static const int kDefaultWindowSteps = 1;
    static const int kRecommendedSecretBytes = 20; // 160 bits, RFC 4226 section 4.

    // Generates a cryptographically random shared secret. |bytes| should be at least 16
    // and at most 64 (HMAC-SHA1 block size).
    static QByteArray generateSecret(int bytes = kRecommendedSecretBytes);

    // Computes the HOTP value for the given counter.
    static QString hotp(const QByteArray& secret, quint64 counter, int digits = kDefaultDigits);

    // Computes the TOTP code for absolute Unix time |unix_time_sec|.
    static QString code(const QByteArray& secret,
                        qint64 unix_time_sec,
                        int step_sec = kDefaultStepSec,
                        int digits = kDefaultDigits);

    // Verifies |code| against the TOTP value for |now_unix_sec|, accepting +/-|window_steps|
    // adjacent steps to compensate for clock drift. If |matched_counter| is non-null and
    // verification succeeds, it is filled with the counter that produced the matching code -
    // callers may use this to detect replay (reject any subsequent code whose counter is
    // less-than-or-equal-to the previously accepted counter).
    static bool verify(const QByteArray& secret,
                       const QString& code,
                       qint64 now_unix_sec,
                       int step_sec = kDefaultStepSec,
                       int digits = kDefaultDigits,
                       int window_steps = kDefaultWindowSteps,
                       quint64* matched_counter = nullptr);

    // Builds an otpauth:// URI per the de-facto Key Uri Format used by Google Authenticator.
    // |issuer| and |account| are percent-encoded; |secret| is Base32-encoded without padding.
    static QString buildUri(const QString& issuer,
                            const QString& account,
                            const QByteArray& secret,
                            int step_sec = kDefaultStepSec,
                            int digits = kDefaultDigits);

private:
    Q_DISABLE_COPY_MOVE(Totp)
};

#endif // BASE_CRYPTO_TOTP_H
