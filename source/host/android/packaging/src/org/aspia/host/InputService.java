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
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.graphics.Path;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.provider.Settings;
import android.text.InputType;
import android.util.Log;
import android.view.KeyEvent;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.EditText;

// Accessibility service backing InputInjectorAndroid. It is bound by the system once the user enables it
// in the accessibility settings; the native side calls the static entry points to dispatch gestures and
// global actions. Gestures must be dispatched from the service instance, so all calls are posted to the
// main looper.
public final class InputService extends AccessibilityService
{
    private static final String TAG = "Aspia";

    private static volatile InputService sInstance = null;
    private static final Handler sHandler = new Handler(Looper.getMainLooper());

    // Off-window EditText used to turn injected key events into text edits: it applies Android's own key
    // character map and text editing (characters, backspace, cursor movement), and the result is copied
    // back to the focused field. Never attached to a window.
    private EditText fake_edit_text = null;

    @Override
    public void onServiceConnected()
    {
        super.onServiceConnected();
        sInstance = this;

        fake_edit_text = new EditText(this);
        fake_edit_text.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE);

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

    // True if the user has enabled this service in the accessibility settings. Unlike isRunning() this
    // reads the settings directly, so it is reliable at startup before the system has bound the service.
    public static boolean isEnabled(Context context)
    {
        String services = Settings.Secure.getString(
            context.getContentResolver(), Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (services == null || services.isEmpty())
            return false;

        ComponentName expected = new ComponentName(context, InputService.class);

        for (String entry : services.split(":"))
        {
            ComponentName parsed = ComponentName.unflattenFromString(entry);
            if (parsed != null && parsed.equals(expected))
                return true;
        }

        return false;
    }

    // Opens the system accessibility settings at this service so the user can enable it. Tries, in
    // order: the per-service details page (stock Android 10+), then the accessibility list scrolled to
    // the service via the fragment-args extras (honoured by several skins), then the plain list. Skins
    // that ignore the deep link (e.g. some Samsung builds) just land on the list.
    public static void openSettings(Context context)
    {
        String component = new ComponentName(context, InputService.class).flattenToString();

        Bundle args = new Bundle();
        args.putString(":settings:fragment_args_key", component);

        // 1. Per-service details page. The action constant is not exposed by every compile SDK, so it
        //    is used as a literal; the details fragment reads the target service from EXTRA_COMPONENT_NAME.
        //    Requires OPEN_ACCESSIBILITY_DETAILS_SETTINGS (declared in the manifest, Android 14+).
        //    resolveActivity avoids throwing on builds that do not handle the action.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
        {
            Bundle details_args = new Bundle();
            details_args.putString(Intent.EXTRA_COMPONENT_NAME, component);

            Intent details = new Intent("android.settings.ACCESSIBILITY_DETAILS_SETTINGS");
            details.putExtra(Intent.EXTRA_COMPONENT_NAME, component);
            details.putExtra(":settings:show_fragment_args", details_args);
            details.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

            if (details.resolveActivity(context.getPackageManager()) != null && startSafely(context, details))
                return;
        }

        // 2. Accessibility list, asking it to scroll to the service.
        Intent list = new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS);
        list.putExtra(":settings:fragment_args_key", component);
        list.putExtra(":settings:show_fragment_args", args);
        list.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (startSafely(context, list))
            return;

        // 3. Plain accessibility list.
        Intent plain = new Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS);
        plain.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        startSafely(context, plain);
    }

    private static boolean startSafely(Context context, Intent intent)
    {
        try
        {
            context.startActivity(intent);
            return true;
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to start accessibility settings: " + intent.getAction(), t);
            return false;
        }
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

    // Injects a key (Android KeyEvent.KEYCODE_* with a meta state) into the focused editable field. The
    // key is dispatched to an off-window EditText so Android applies its key character map and editing
    // behavior, then the resulting text and caret are copied back to the field.
    public static void injectKey(int keyCode, int metaState)
    {
        final InputService service = sInstance;
        if (service == null)
            return;

        sHandler.post(() -> service.processKey(keyCode, metaState));
    }

    private void processKey(int keyCode, int metaState)
    {
        if (fake_edit_text == null)
            return;

        AccessibilityNodeInfo root = getRootInActiveWindow();
        if (root == null)
            return;

        AccessibilityNodeInfo node = root.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null || !node.isEditable())
            return;

        // Enter on a single-line field submits it (Go/Search/Done in address bars, search boxes, etc.)
        // instead of typing a newline. Multi-line fields fall through and get a real newline.
        if (keyCode == KeyEvent.KEYCODE_ENTER && !node.isMultiLine())
        {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R)
                node.performAction(AccessibilityNodeInfo.AccessibilityAction.ACTION_IME_ENTER.getId());
            return;
        }

        // An empty field can report its placeholder as the text; treat that as empty so typing replaces
        // the hint instead of appending to it.
        String current = node.isShowingHintText() ? "" : textOf(node);

        int start = node.getTextSelectionStart();
        int end = node.getTextSelectionEnd();
        if (start < 0 || end < 0 || start > current.length() || end > current.length())
        {
            start = current.length();
            end = current.length();
        }

        // Seed the off-window editor with the field's current state, then let it process the key.
        fake_edit_text.setText(current);
        fake_edit_text.setSelection(Math.min(start, end), Math.max(start, end));

        long now = SystemClock.uptimeMillis();
        KeyEvent down = new KeyEvent(now, now, KeyEvent.ACTION_DOWN, keyCode, 0, metaState);
        KeyEvent up = new KeyEvent(now, now, KeyEvent.ACTION_UP, keyCode, 0, metaState);
        fake_edit_text.onKeyDown(keyCode, down);
        fake_edit_text.onKeyUp(keyCode, up);

        writeText(node, fake_edit_text.getText().toString(), fake_edit_text.getSelectionEnd());
    }

    private static void insertText(InputService service, String text)
    {
        AccessibilityNodeInfo root = service.getRootInActiveWindow();
        if (root == null)
            return;

        AccessibilityNodeInfo node = root.findFocus(AccessibilityNodeInfo.FOCUS_INPUT);
        if (node == null)
            return;

        // An empty field can report its placeholder as the text; treat that as empty.
        String current = node.isShowingHintText() ? "" : textOf(node);

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

        // Place the caret just after the inserted text.
        writeText(node, updated, start + text.length());
    }

    // Replaces the field's whole text (ACTION_SET_TEXT replaces everything) and places the caret.
    private static void writeText(AccessibilityNodeInfo node, String text, int caret)
    {
        Bundle text_args = new Bundle();
        text_args.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, text);
        if (!node.performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, text_args))
            Log.w(TAG, "ACTION_SET_TEXT was not accepted by the focused node");

        Bundle selection_args = new Bundle();
        selection_args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, caret);
        selection_args.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, caret);
        node.performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection_args);
    }

    private static String textOf(AccessibilityNodeInfo node)
    {
        CharSequence text = node.getText();
        return (text != null) ? text.toString() : "";
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
