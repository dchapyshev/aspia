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

#ifndef CLIENT_ANDROID_BIOMETRIC_GATE_H
#define CLIENT_ANDROID_BIOMETRIC_GATE_H

#include <QByteArray>
#include <QString>

#include <optional>

#include "base/crypto/secure_byte_array.h"

// Bridge to the Android Keystore and BiometricPrompt (see BiometricGate.java). Wraps the master key
// behind a biometric-gated key and recovers it later. The enroll() and unlock() calls block the
// calling thread until the asynchronous prompt finishes, so they must run on the GUI thread, never
// on the Android main thread.
class BiometricGate
{
    Q_GADGET

public:
    enum class Status
    {
        AVAILABLE,
        NO_HARDWARE,
        NONE_ENROLLED,
        UNAVAILABLE
    };
    Q_ENUM(Status)

    enum class Result
    {
        SUCCESS,
        CANCELED,
        FAILED,
        KEY_INVALIDATED
    };
    Q_ENUM(Result)

    // Localized strings shown by the system prompt.
    struct Prompt
    {
        QString title;
        QString negative_text;
    };

    // Wrapped master key as stored in the database: the GCM IV and the ciphertext.
    struct Blob
    {
        QByteArray iv;
        QByteArray ciphertext;
    };

    // Outcome of an unlock together with the recovered key on success. A KEY_INVALIDATED result
    // means the stored blob is no longer usable and must be discarded.
    struct UnlockResult
    {
        Result result = Result::FAILED;
        SecureByteArray key;
    };

    static Status status();

    static std::optional<Blob> enroll(const SecureByteArray& key, const Prompt& prompt);
    static UnlockResult unlock(const Blob& blob, const Prompt& prompt);

    static void deleteKey();

    // Serializes a blob into a single byte array for storage and back. The format is the IV length
    // prefixed before the IV and the ciphertext.
    static QByteArray pack(const Blob& blob);
    static std::optional<Blob> unpack(const QByteArray& packed);

private:
    Q_DISABLE_COPY_MOVE(BiometricGate)
};

#endif // CLIENT_ANDROID_BIOMETRIC_GATE_H
