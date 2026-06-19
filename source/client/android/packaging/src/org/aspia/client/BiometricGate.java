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

package org.aspia.client;

import android.app.Activity;
import android.content.Context;
import android.content.DialogInterface;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.os.Build;
import android.os.CancellationSignal;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;

import java.security.KeyStore;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

// Wraps the Android Keystore and the framework BiometricPrompt so the C++ side can store the master
// key behind a biometric-gated key and recover it later. The androidx biometric library needs a
// FragmentActivity, which QtActivity is not, so the framework API (API 28+) is used instead.
public final class BiometricGate
{
    // Status values returned by status(). Kept in sync with BiometricGate::Status on the C++ side.
    public static final int STATUS_AVAILABLE     = 0;
    public static final int STATUS_NO_HARDWARE   = 1;
    public static final int STATUS_NONE_ENROLLED = 2;
    public static final int STATUS_UNAVAILABLE   = 3;

    // Result codes passed to nativeOnResult(). Kept in sync with BiometricGate::Result on the C++ side.
    public static final int RESULT_SUCCESS         = 0;
    public static final int RESULT_CANCELED        = 1;
    public static final int RESULT_FAILED          = 2;
    public static final int RESULT_KEY_INVALIDATED = 3;

    private static final String KEYSTORE = "AndroidKeyStore";
    private static final String KEY_ALIAS = "aspia_master_key";
    private static final int GCM_TAG_BITS = 128;

    // Outstanding cancellation signals keyed by the token of the operation, so the C++ side can abort
    // a running prompt (e.g. when its dialog is closed).
    private static final ConcurrentHashMap<Long, CancellationSignal> signals = new ConcurrentHashMap<>();

    private BiometricGate() {}

    // Reports whether a strong biometric is currently usable for key-bound authentication.
    @SuppressWarnings("deprecation")
    public static int status(Context context)
    {
        // BiometricManager is only available from API 29; on older devices the feature stays off.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q)
            return STATUS_UNAVAILABLE;

        BiometricManager manager = context.getSystemService(BiometricManager.class);
        if (manager == null)
            return STATUS_UNAVAILABLE;

        int result;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
            result = manager.canAuthenticate(BiometricManager.Authenticators.BIOMETRIC_STRONG);
        else
            result = manager.canAuthenticate();

        switch (result)
        {
            case BiometricManager.BIOMETRIC_SUCCESS:
                return STATUS_AVAILABLE;
            case BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE:
                return STATUS_NO_HARDWARE;
            case BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED:
                return STATUS_NONE_ENROLLED;
            default:
                return STATUS_UNAVAILABLE;
        }
    }

    // Generates a fresh biometric-gated key, asks the user to authenticate and, on success, encrypts
    // |key|. The result delivers the ciphertext as data and the GCM IV separately.
    public static void enroll(Activity activity, byte[] key, String title, String negativeText,
                              long token)
    {
        try
        {
            deleteKey();

            KeyGenerator generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, KEYSTORE);
            KeyGenParameterSpec.Builder builder = new KeyGenParameterSpec.Builder(
                    KEY_ALIAS, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
                    .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                    .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                    .setKeySize(256)
                    .setUserAuthenticationRequired(true)
                    .setInvalidatedByBiometricEnrollment(true);
            generator.init(builder.build());
            generator.generateKey();

            Cipher cipher = createCipher();
            cipher.init(Cipher.ENCRYPT_MODE, loadKey());

            authenticate(activity, cipher, key, title, negativeText, token);
        }
        catch (Exception ex)
        {
            nativeOnResult(token, RESULT_FAILED, null, null, ex.toString());
        }
    }

    // Asks the user to authenticate and, on success, decrypts the stored ciphertext back into the
    // master key. The result delivers the plaintext key as data.
    public static void unlock(Activity activity, byte[] iv, byte[] ciphertext, String title,
                              String negativeText, long token)
    {
        try
        {
            Cipher cipher = createCipher();
            cipher.init(Cipher.DECRYPT_MODE, loadKey(), new GCMParameterSpec(GCM_TAG_BITS, iv));

            authenticate(activity, cipher, ciphertext, title, negativeText, token);
        }
        catch (android.security.keystore.KeyPermanentlyInvalidatedException ex)
        {
            // The key is gone because the user added or removed a biometric. The caller must drop
            // the stored blob and fall back to the password.
            nativeOnResult(token, RESULT_KEY_INVALIDATED, null, null, ex.toString());
        }
        catch (Exception ex)
        {
            nativeOnResult(token, RESULT_FAILED, null, null, ex.toString());
        }
    }

    // Cancels a running prompt identified by |token|. Has no effect if the operation already finished.
    public static void cancel(long token)
    {
        CancellationSignal signal = signals.get(token);
        if (signal != null)
            signal.cancel();
    }

    // Removes the keystore key, invalidating any previously stored blob.
    public static void deleteKey()
    {
        try
        {
            KeyStore keystore = KeyStore.getInstance(KEYSTORE);
            keystore.load(null);
            if (keystore.containsAlias(KEY_ALIAS))
                keystore.deleteEntry(KEY_ALIAS);
        }
        catch (Exception ex)
        {
            // Nothing can be done if the key cannot be removed; treat it as already absent.
        }
    }

    private static Cipher createCipher() throws Exception
    {
        return Cipher.getInstance(KeyProperties.KEY_ALGORITHM_AES + "/"
                + KeyProperties.BLOCK_MODE_GCM + "/"
                + KeyProperties.ENCRYPTION_PADDING_NONE);
    }

    private static SecretKey loadKey() throws Exception
    {
        KeyStore keystore = KeyStore.getInstance(KEYSTORE);
        keystore.load(null);
        return (SecretKey) keystore.getKey(KEY_ALIAS, null);
    }

    // Runs the biometric prompt bound to |cipher| and, once the user authenticates, applies the
    // cipher to |payload|, reporting the transformed bytes through nativeOnResult().
    private static void authenticate(Activity activity, Cipher cipher, byte[] payload, String title,
                                     String negativeText, long token)
    {
        Executor executor = activity.getMainExecutor();
        CancellationSignal signal = new CancellationSignal();
        signals.put(token, signal);

        // Both the negative button and the error callback can fire for a single cancellation, so the
        // native side must be notified exactly once.
        AtomicBoolean reported = new AtomicBoolean(false);

        // The framework delivers the negative-button press as a no-op listener call; report the
        // cancellation here so it does not depend on a specific error code being public.
        DialogInterface.OnClickListener onNegative = (DialogInterface dialog, int which) ->
        {
            if (reported.compareAndSet(false, true))
            {
                signals.remove(token);
                nativeOnResult(token, RESULT_CANCELED, null, null, null);
            }
        };

        BiometricPrompt prompt = new BiometricPrompt.Builder(activity)
                .setTitle(title)
                .setNegativeButton(negativeText, executor, onNegative)
                .build();

        BiometricPrompt.AuthenticationCallback callback = new BiometricPrompt.AuthenticationCallback()
        {
            @Override
            public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult result)
            {
                if (!reported.compareAndSet(false, true))
                    return;
                signals.remove(token);

                try
                {
                    Cipher authorized = result.getCryptoObject().getCipher();
                    byte[] output = authorized.doFinal(payload);
                    nativeOnResult(token, RESULT_SUCCESS, output, authorized.getIV(), null);
                }
                catch (Exception ex)
                {
                    nativeOnResult(token, RESULT_FAILED, null, null, ex.toString());
                }
            }

            @Override
            public void onAuthenticationError(int code, CharSequence message)
            {
                if (!reported.compareAndSet(false, true))
                    return;
                signals.remove(token);

                int result = isCancel(code) ? RESULT_CANCELED : RESULT_FAILED;
                nativeOnResult(token, result, null, null, message != null ? message.toString() : null);
            }

            // A single non-matching attempt; the prompt stays up, so the native side is not notified.
            @Override
            public void onAuthenticationFailed() {}
        };

        prompt.authenticate(new BiometricPrompt.CryptoObject(cipher), signal, executor, callback);
    }

    private static boolean isCancel(int code)
    {
        return code == BiometricPrompt.BIOMETRIC_ERROR_USER_CANCELED
                || code == BiometricPrompt.BIOMETRIC_ERROR_CANCELED;
    }

    private static native void nativeOnResult(
            long token, int result, byte[] data, byte[] iv, String error);
}
