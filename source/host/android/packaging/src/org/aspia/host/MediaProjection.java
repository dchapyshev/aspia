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

package org.aspia.host;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjectionConfig;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;

import java.nio.ByteBuffer;

// MediaProjection-based capture for the Android host. A single projection (obtained through the consent
// flow and the mediaProjection foreground service) backs both screen capture (a VirtualDisplay feeding an
// ImageReader) and system audio capture (an AudioRecord with AudioPlaybackCaptureConfiguration); each can
// be turned on independently. One session is active at a time (the host serves one desktop), so the state
// is static and there is a single native capturer of each kind (no token needed to route callbacks). Uses
// the fully qualified android.media.projection.MediaProjection to avoid clashing with this class name.
public final class MediaProjection
{
    private static final String TAG = "Aspia";

    private static final int SAMPLE_RATE = 48000;

    private static android.media.projection.MediaProjection sProjection = null;

    // Screen capture state.
    private static VirtualDisplay sVirtualDisplay = null;
    private static ImageReader sReader = null;
    private static HandlerThread sHandlerThread = null;
    private static Handler sHandler = null;

    // Serialises the ImageReader listener (runs on the capture HandlerThread) with sReader.close()
    // (runs on the Android main thread via the native stop path). Without it, acquiring an image while
    // the reader is being closed throws IllegalStateException on the listener thread.
    private static final Object sReaderLock = new Object();

    // Audio capture state. Unlike the screen capture there is only ever one audio capturer, so no token
    // is needed to route frames back to the native side.
    private static AudioRecord sRecord = null;
    private static Thread sAudioThread = null;
    private static volatile boolean sAudioRunning = false;

    // Native callbacks implemented in screen_capturer_android.cc and audio_capturer_android.cc.
    private static native void nativeOnStarted(boolean success, int width, int height, int dpi);
    private static native void nativeOnFrame(ByteBuffer buffer, int width, int height,
                                             int pixelStride, int rowStride);
    private static native void nativeOnAudioFrame(ByteBuffer buffer, int size);

    private MediaProjection()
    {
        // Static-only.
    }

    // Returns the Intent that asks the user for screen capture consent. On Android 14+ the consent dialog
    // is configured to capture the whole display, so it defaults to (and is locked on) the entire screen
    // instead of a single app - the host needs the full screen.
    public static Intent createPermissionIntent(Activity activity)
    {
        MediaProjectionManager manager = (MediaProjectionManager)
            activity.getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
        {
            return manager.createScreenCaptureIntent(
                MediaProjectionConfig.createConfigForDefaultDisplay());
        }

        return manager.createScreenCaptureIntent();
    }

    // Real pixel geometry of the default display; usable before the projection exists so the native side
    // can report a screen resolution as soon as the capturer is created.
    private static DisplayMetrics realMetrics(Context context)
    {
        DisplayManager manager = (DisplayManager) context.getSystemService(Context.DISPLAY_SERVICE);
        Display display = manager.getDisplay(Display.DEFAULT_DISPLAY);
        DisplayMetrics metrics = new DisplayMetrics();
        display.getRealMetrics(metrics);
        return metrics;
    }

    public static int displayWidth(Context context)
    {
        return realMetrics(context).widthPixels;
    }

    public static int displayHeight(Context context)
    {
        return realMetrics(context).heightPixels;
    }

    public static int displayDpi(Context context)
    {
        return realMetrics(context).densityDpi;
    }

    // Launches the transparent consent Activity. On consent it starts the foreground service that owns the
    // projection; on refusal notifyDenied() reports the failure to the native side.
    public static void requestCapture(Context context)
    {
        Intent intent = new Intent(context, MediaProjectionPermissionActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);
    }

    // Reports that the user declined the screen capture consent.
    public static void notifyDenied()
    {
        Log.i(TAG, "Screen capture consent denied");
        nativeOnStarted(false, 0, 0, 0);
    }

    // Obtains the projection and begins delivering screen frames. Called from the foreground service so the
    // projection is obtained while a mediaProjection-type service is running. Reports the outcome and the
    // chosen virtual display geometry through nativeOnStarted().
    public static synchronized void startProjection(Context context, int resultCode, Intent data)
    {
        try
        {
            if (data == null)
            {
                Log.e(TAG, "Screen capture consent data is missing");
                nativeOnStarted(false, 0, 0, 0);
                return;
            }

            MediaProjectionManager manager = (MediaProjectionManager)
                context.getSystemService(Context.MEDIA_PROJECTION_SERVICE);
            sProjection = manager.getMediaProjection(resultCode, data);
            if (sProjection == null)
            {
                Log.e(TAG, "getMediaProjection returned null");
                nativeOnStarted(false, 0, 0, 0);
                return;
            }

            sHandlerThread = new HandlerThread("AspiaScreenCapture");
            sHandlerThread.start();
            sHandler = new Handler(sHandlerThread.getLooper());

            // A callback is mandatory; the projection is torn down from the native stop() path. The
            // callback captures its own projection instance so onProjectionStopped() can tell a live,
            // system-initiated stop from a stale callback of an already-released session.
            final android.media.projection.MediaProjection projection = sProjection;
            sProjection.registerCallback(new android.media.projection.MediaProjection.Callback()
            {
                @Override
                public void onStop()
                {
                    Log.i(TAG, "MediaProjection stopped");
                    onProjectionStopped(projection);
                }
            }, sHandler);

            DisplayMetrics metrics = realMetrics(context);
            final int width = metrics.widthPixels;
            final int height = metrics.heightPixels;
            final int dpi = metrics.densityDpi;

            sReader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 2);
            sReader.setOnImageAvailableListener(reader ->
            {
                // Hold sReaderLock so this cannot run while releaseInternal() closes the reader. The
                // identity check skips frames once the reader has been closed (sReader nulled) or
                // replaced by a later session.
                synchronized (sReaderLock)
                {
                    if (sReader != reader)
                        return;

                    Image image = reader.acquireLatestImage();
                    if (image == null)
                        return;

                    try
                    {
                        Image.Plane[] planes = image.getPlanes();
                        ByteBuffer buffer = planes[0].getBuffer();
                        nativeOnFrame(buffer, image.getWidth(), image.getHeight(),
                                      planes[0].getPixelStride(), planes[0].getRowStride());
                    }
                    finally
                    {
                        image.close();
                    }
                }
            }, sHandler);

            sVirtualDisplay = sProjection.createVirtualDisplay("AspiaScreenCapture", width, height, dpi,
                DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR, sReader.getSurface(), null, sHandler);

            Log.i(TAG, "Screen capture started: " + width + "x" + height + " @ " + dpi + " dpi");
            nativeOnStarted(true, width, height, dpi);
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to start screen capture", t);
            releaseInternal();
            nativeOnStarted(false, 0, 0, 0);
        }
    }

    // Stops the capture: stops the foreground service (which releases the projection in its onDestroy) and
    // releases directly in case the service was never started.
    public static void stopCapture(Context context)
    {
        try
        {
            context.stopService(new Intent(context, MediaProjectionService.class));
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to stop screen capture service", t);
        }

        stop();
    }

    // Releases all projection resources.
    public static synchronized void stop()
    {
        releaseInternal();
    }

    // Handles MediaProjection.Callback.onStop() for |projection|, delivered on the capture
    // HandlerThread. releaseInternal() nulls sProjection before stopping it, so a self-inflicted
    // callback (and any stale one from a previous session) no longer matches sProjection here and is
    // ignored; only a system-initiated stop of the live session (consent revoked, user switch) passes.
    // Synchronized so the teardown cannot race a concurrent stop()/startProjection() on another thread.
    private static synchronized void onProjectionStopped(android.media.projection.MediaProjection projection)
    {
        if (projection != sProjection)
            return;

        // Release the now-dead projection resources and tell native the capture is gone, otherwise the
        // host keeps treating the session as active and streams a frozen frame.
        releaseInternal();
        nativeOnStarted(false, 0, 0, 0);
    }

    // Releases all projection resources. Must be called with the class monitor held: every caller
    // (stop(), the startProjection() failure path, onProjectionStopped()) is synchronized, so the
    // teardown never races another lifecycle operation.
    private static void releaseInternal()
    {
        if (sVirtualDisplay != null)
        {
            sVirtualDisplay.release();
            sVirtualDisplay = null;
        }

        if (sReader != null)
        {
            // Close under sReaderLock so the listener is not mid-acquire on the HandlerThread.
            synchronized (sReaderLock)
            {
                sReader.close();
                sReader = null;
            }
        }

        if (sProjection != null)
        {
            // Null the field before stop(): the onStop() callback it triggers then fails the identity
            // check in onProjectionStopped() and the self-inflicted stop is ignored.
            android.media.projection.MediaProjection projection = sProjection;
            sProjection = null;
            projection.stop();
        }

        if (sHandlerThread != null)
        {
            sHandlerThread.quitSafely();
            sHandlerThread = null;
            sHandler = null;
        }
    }

    // Captures the device's playback (system) audio through an AudioRecord built from the projection. The
    // screen capture must already be running (that is what obtains the projection). PCM is read on a
    // dedicated thread and pushed through nativeOnAudioFrame().
    public static synchronized void startAudioCapture(Context context)
    {
        if (sRecord != null)
            return;

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q)
        {
            Log.w(TAG, "Playback capture requires Android 10; audio unavailable");
            return;
        }

        if (sProjection == null)
        {
            Log.e(TAG, "No MediaProjection for audio capture");
            return;
        }

        try
        {
            AudioPlaybackCaptureConfiguration config =
                new AudioPlaybackCaptureConfiguration.Builder(sProjection)
                    .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                    .addMatchingUsage(AudioAttributes.USAGE_GAME)
                    .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                    .build();

            AudioFormat format = new AudioFormat.Builder()
                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .setSampleRate(SAMPLE_RATE)
                .setChannelMask(AudioFormat.CHANNEL_IN_STEREO)
                .build();

            int min_buffer = AudioRecord.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT);

            sRecord = new AudioRecord.Builder()
                .setAudioFormat(format)
                .setBufferSizeInBytes(min_buffer * 2)
                .setAudioPlaybackCaptureConfig(config)
                .build();

            sRecord.startRecording();

            sAudioRunning = true;

            final int buffer_size = min_buffer;
            sAudioThread = new Thread(() -> readLoop(buffer_size));
            sAudioThread.start();

            Log.i(TAG, "Audio capture started");
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to start audio capture", t);
            stopAudioCapture();
        }
    }

    private static void readLoop(int min_buffer)
    {
        // Capture the instance once. stopAudioCapture() releases and nulls sRecord only after joining
        // this thread, so a captured reference is never released while this loop is still using it.
        final AudioRecord record = sRecord;
        if (record == null)
            return;

        // A 10 ms stereo 16-bit chunk (480 frames * 2 channels * 2 bytes) matches the encoder granularity.
        final int chunk = SAMPLE_RATE / 100 * 2 * 2;
        ByteBuffer buffer = ByteBuffer.allocateDirect(Math.max(chunk, min_buffer));

        while (sAudioRunning)
        {
            buffer.clear();
            int read = record.read(buffer, buffer.capacity());
            if (read > 0)
            {
                nativeOnAudioFrame(buffer, read);
            }
            else if (read < 0)
            {
                Log.e(TAG, "AudioRecord.read error: " + read);
                break;
            }
        }
    }

    public static synchronized void stopAudioCapture()
    {
        sAudioRunning = false;

        // Stop recording first: this unblocks a reader thread that may be parked inside a blocking
        // AudioRecord.read(), letting it observe sAudioRunning == false and leave readLoop().
        if (sRecord != null)
        {
            try
            {
                sRecord.stop();
            }
            catch (Throwable t)
            {
                // Ignore.
            }
        }

        // Wait for the reader thread to fully exit before releasing. A bounded join() could return
        // while read() is still in flight, and releasing the AudioRecord under it would crash the
        // native process; stop() above guarantees read() returns promptly, so the join completes.
        if (sAudioThread != null)
        {
            try
            {
                sAudioThread.join();
            }
            catch (InterruptedException e)
            {
                Thread.currentThread().interrupt();
            }
            sAudioThread = null;
        }

        // The reader thread has exited; releasing now cannot race with read().
        if (sRecord != null)
        {
            sRecord.release();
            sRecord = null;
        }
    }
}
