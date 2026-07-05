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

import android.accessibilityservice.AccessibilityService;
import android.accessibilityservice.GestureDescription;
import android.content.Intent;
import android.graphics.Path;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;

// Accessibility service backing InputInjectorAndroid. It is bound by the system once the user enables it
// in the accessibility settings; the native side calls the static entry points to dispatch gestures and
// global actions. Gestures must be dispatched from the service instance, so all calls are posted to the
// main looper.
public final class InputService extends AccessibilityService
{
    private static final String TAG = "Aspia";

    private static volatile InputService sInstance = null;
    private static final Handler sHandler = new Handler(Looper.getMainLooper());

    @Override
    public void onServiceConnected()
    {
        super.onServiceConnected();
        sInstance = this;
        Log.i(TAG, "Accessibility input service connected");
    }

    @Override
    public boolean onUnbind(Intent intent)
    {
        sInstance = null;
        Log.i(TAG, "Accessibility input service unbound");
        return super.onUnbind(intent);
    }

    @Override
    public void onDestroy()
    {
        sInstance = null;
        super.onDestroy();
    }

    @Override
    public void onAccessibilityEvent(AccessibilityEvent event)
    {
        // The service is used only to dispatch gestures, not to observe events.
    }

    @Override
    public void onInterrupt()
    {
        // Nothing.
    }

    // True while the user has the service enabled and the system has it bound.
    public static boolean isRunning()
    {
        return sInstance != null;
    }

    // Taps at (|x|, |y|). |durationMs| distinguishes a click from a long press.
    public static void tap(float x, float y, int durationMs)
    {
        dispatch(buildStroke(x, y, x, y, durationMs));
    }

    // Swipes (drags) from (|x1|, |y1|) to (|x2|, |y2|) over |durationMs|.
    public static void swipe(float x1, float y1, float x2, float y2, int durationMs)
    {
        dispatch(buildStroke(x1, y1, x2, y2, durationMs));
    }

    // Performs a global navigation action (AccessibilityService.GLOBAL_ACTION_*).
    public static void globalAction(int action)
    {
        final InputService service = sInstance;
        if (service == null)
            return;

        sHandler.post(() -> service.performGlobalAction(action));
    }

    // Commits |text| into the focused editable field at the caret. ACTION_SET_TEXT replaces the whole
    // field, so the current content is spliced with the new text over the current selection.
    public static void commitText(String text)
    {
        final InputService service = sInstance;
        if (service == null || text == null || text.isEmpty())
            return;

        sHandler.post(() -> insertText(service, text));
    }

    private static void insertText(InputService service, String text)
    {
        AccessibilityNodeInfo root = service.getRootInActiveWindow();
        if (root == null)
            return;

        AccessibilityNodeInfo node = root.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null)
            return;

        CharSequence existing = node.getText();
        String current = (existing != null) ? existing.toString() : "";

        int start = node.getTextSelectionStart();
        int end = node.getTextSelectionEnd();

        // No usable selection reported: append at the end.
        if (start < 0 || end < 0 || start > current.length() || end > current.length())
        {
            start = current.length();
            end = current.length();
        }
        if (start > end)
        {
            int tmp = start;
            start = end;
            end = tmp;
        }

        String updated = current.substring(0, start) + text + current.substring(end);

        Bundle text_args = new Bundle();
        text_args.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, updated);
        if (!node.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, text_args))
            Log.w(TAG, "ACTION_SET_TEXT was not accepted by the focused node");

        // Place the caret just after the inserted text.
        int caret = start + text.length();
        Bundle selection_args = new Bundle();
        selection_args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, caret);
        selection_args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, caret);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection_args);
    }

    private static GestureDescription.StrokeDescription buildStroke(float x1, float y1, float x2,
                                                                    float y2, int durationMs)
    {
        Path path = new Path();
        path.moveTo(x1, y1);
        if (x1 != x2 || y1 != y2)
            path.lineTo(x2, y2);

        return new GestureDescription.StrokeDescription(path, 0, Math.max(1, durationMs));
    }

    private static void dispatch(GestureDescription.StrokeDescription stroke)
    {
        final InputService service = sInstance;
        if (service == null || stroke == null)
            return;

        final GestureDescription gesture = new GestureDescription.Builder().addStroke(stroke).build();
        sHandler.post(() -> service.dispatchGesture(gesture, null, null));
    }
}
