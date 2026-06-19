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

#include "client/android/biometric_gate.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFuture>
#include <QHash>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMutex>

#include <utility>

#include "base/logging.h"

namespace {

const char kBiometricClass[] = "org/aspia/client/BiometricGate";

// Result codes. Must match the RESULT_* constants in BiometricGate.java.
const int kResultSuccess        = 0;
const int kResultCanceled       = 1;
const int kResultFailed         = 2;
const int kResultKeyInvalidated = 3;

// Status codes returned by BiometricGate.status() on the Java side.
const int kStatusAvailable    = 0;
const int kStatusNoHardware   = 1;
const int kStatusNoneEnrolled = 2;
const int kStatusUnavailable  = 3;

// State of one outstanding prompt. The dispatch fills |result|, |data| and |iv| from the Android
// main thread, then wakes |loop| running on the GUI thread.
struct PendingResult
{
    QEventLoop* loop = nullptr;
    int result = kResultFailed;
    QByteArray data;
    QByteArray iv;
};

QMutex g_mutex;
QHash<qint64, PendingResult*> g_pending;
qint64 g_next_token = 0;

//--------------------------------------------------------------------------------------------------
qint64 nextToken()
{
    QMutexLocker locker(&g_mutex);
    return ++g_next_token;
}

//--------------------------------------------------------------------------------------------------
QJniObject toJniByteArray(const QByteArray& bytes)
{
    QJniEnvironment env;
    jbyteArray array = env->NewByteArray(bytes.size());
    env->SetByteArrayRegion(array, 0, bytes.size(), reinterpret_cast<const jbyte*>(bytes.constData()));
    return QJniObject::fromLocalRef(array);
}

//--------------------------------------------------------------------------------------------------
QByteArray fromJniByteArray(JNIEnv* env, jbyteArray array)
{
    if (!array)
        return QByteArray();

    jsize length = env->GetArrayLength(array);
    QByteArray out(length, Qt::Uninitialized);
    env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(out.data()));
    return out;
}

//--------------------------------------------------------------------------------------------------
void finishPending(qint64 token, int result, const QByteArray& data, const QByteArray& iv)
{
    QMutexLocker locker(&g_mutex);

    auto it = g_pending.find(token);
    if (it == g_pending.end())
        return;

    PendingResult* pending = it.value();
    pending->result = result;
    pending->data = data;
    pending->iv = iv;

    // The callback runs on the Android main thread; wake the GUI-thread loop through its queue.
    QMetaObject::invokeMethod(pending->loop, "quit", Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void JNICALL nativeOnResult(JNIEnv* env, jclass /* clazz */, jlong token, jint result,
                            jbyteArray data, jbyteArray iv, jstring error)
{
    QByteArray data_bytes = fromJniByteArray(env, data);
    QByteArray iv_bytes = fromJniByteArray(env, iv);

    if (result != kResultSuccess && error)
    {
        const char* utf = env->GetStringUTFChars(error, nullptr);
        if (utf)
        {
            LOG(INFO) << "Biometric operation failed:" << result << QString::fromUtf8(utf);
            env->ReleaseStringUTFChars(error, utf);
        }
    }

    finishPending(token, result, data_bytes, iv_bytes);
}

//--------------------------------------------------------------------------------------------------
void ensureRegistered()
{
    QMutexLocker locker(&g_mutex);

    static bool registered = false;
    if (registered)
        return;

    const JNINativeMethod method =
    {
        "nativeOnResult",
        "(JI[B[BLjava/lang/String;)V",
        reinterpret_cast<void*>(nativeOnResult)
    };

    QJniEnvironment env;
    if (!env.registerNativeMethods(kBiometricClass, &method, 1))
        LOG(ERROR) << "Unable to register native methods for BiometricGate";
    else
        registered = true;
}

//--------------------------------------------------------------------------------------------------
BiometricGate::Result toResult(int result)
{
    switch (result)
    {
        case kResultSuccess:
            return BiometricGate::Result::SUCCESS;
        case kResultCanceled:
            return BiometricGate::Result::CANCELED;
        case kResultKeyInvalidated:
            return BiometricGate::Result::KEY_INVALIDATED;
        default:
            return BiometricGate::Result::FAILED;
    }
}

//--------------------------------------------------------------------------------------------------
template <typename Dispatch>
PendingResult runPrompt(qint64 token, Dispatch dispatch)
{
    QEventLoop loop;
    PendingResult* pending = new PendingResult;
    pending->loop = &loop;

    {
        QMutexLocker locker(&g_mutex);
        g_pending.insert(token, pending);
    }

    // The prompt must be shown from the Android main thread; the call only schedules it and returns.
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([dispatch]() -> QVariant
    {
        dispatch();
        return QVariant();
    });

    loop.exec();

    PendingResult result;
    {
        QMutexLocker locker(&g_mutex);
        result = *pending;
        g_pending.remove(token);
    }

    delete pending;
    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
BiometricGate::Status BiometricGate::status()
{
    QFuture<QVariant> future = QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
        []() -> QVariant
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
            return QVariant(kStatusUnavailable);

        jint value = QJniObject::callStaticMethod<jint>(
            kBiometricClass, "status", "(Landroid/content/Context;)I", context.object());
        return QVariant(static_cast<int>(value));
    });

    future.waitForFinished();

    switch (future.result().toInt())
    {
        case kStatusAvailable:
            return Status::AVAILABLE;
        case kStatusNoHardware:
            return Status::NO_HARDWARE;
        case kStatusNoneEnrolled:
            return Status::NONE_ENROLLED;
        default:
            return Status::UNAVAILABLE;
    }
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<BiometricGate::Blob> BiometricGate::enroll(const SecureByteArray& key,
                                                         const Prompt& prompt)
{
    ensureRegistered();

    const qint64 token = nextToken();
    PendingResult pending = runPrompt(token, [token, key, prompt]()
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
        {
            finishPending(token, kResultFailed, QByteArray(), QByteArray());
            return;
        }

        QJniObject j_key = toJniByteArray(key.toByteArray());
        QJniObject j_title = QJniObject::fromString(prompt.title);
        QJniObject j_negative = QJniObject::fromString(prompt.negative_text);

        QJniObject::callStaticMethod<void>(kBiometricClass, "enroll",
            "(Landroid/app/Activity;[BLjava/lang/String;Ljava/lang/String;J)V",
            context.object(), j_key.object<jbyteArray>(), j_title.object<jstring>(),
            j_negative.object<jstring>(), static_cast<jlong>(token));
    });

    if (pending.result != kResultSuccess)
        return std::nullopt;

    return Blob{ pending.iv, pending.data };
}

//--------------------------------------------------------------------------------------------------
// static
BiometricGate::UnlockResult BiometricGate::unlock(const Blob& blob, const Prompt& prompt)
{
    ensureRegistered();

    const qint64 token = nextToken();
    PendingResult pending = runPrompt(token, [token, blob, prompt]()
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid())
        {
            finishPending(token, kResultFailed, QByteArray(), QByteArray());
            return;
        }

        QJniObject j_iv = toJniByteArray(blob.iv);
        QJniObject j_ciphertext = toJniByteArray(blob.ciphertext);
        QJniObject j_title = QJniObject::fromString(prompt.title);
        QJniObject j_negative = QJniObject::fromString(prompt.negative_text);

        QJniObject::callStaticMethod<void>(kBiometricClass, "unlock",
            "(Landroid/app/Activity;[B[BLjava/lang/String;Ljava/lang/String;J)V",
            context.object(), j_iv.object<jbyteArray>(), j_ciphertext.object<jbyteArray>(),
            j_title.object<jstring>(), j_negative.object<jstring>(), static_cast<jlong>(token));
    });

    UnlockResult unlock_result;
    unlock_result.result = toResult(pending.result);
    if (pending.result == kResultSuccess)
        unlock_result.key = SecureByteArray(std::move(pending.data));

    return unlock_result;
}

//--------------------------------------------------------------------------------------------------
// static
void BiometricGate::deleteKey()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() -> QVariant
    {
        QJniObject::callStaticMethod<void>(kBiometricClass, "deleteKey", "()V");
        return QVariant();
    });
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray BiometricGate::pack(const Blob& blob)
{
    QByteArray packed;
    packed.append(static_cast<char>(blob.iv.size()));
    packed.append(blob.iv);
    packed.append(blob.ciphertext);
    return packed;
}

//--------------------------------------------------------------------------------------------------
// static
std::optional<BiometricGate::Blob> BiometricGate::unpack(const QByteArray& packed)
{
    if (packed.isEmpty())
        return std::nullopt;

    const int iv_size = static_cast<quint8>(packed.at(0));
    if (packed.size() < 1 + iv_size)
        return std::nullopt;

    Blob blob;
    blob.iv = packed.mid(1, iv_size);
    blob.ciphertext = packed.mid(1 + iv_size);
    return blob;
}
