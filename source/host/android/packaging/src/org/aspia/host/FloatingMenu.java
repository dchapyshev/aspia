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

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

// Draggable floating button shown while a desktop session is active. Tapping it opens a small action
// menu (clipboard sync, Back, Home, Recents). The menu window is focusable, so while it is open this app
// counts as focused and may access the clipboard (Android blocks clipboard access for background apps).
//
// The overlay lives on its own thread rather than the main thread: when the host app is backgrounded Qt
// blocks the Android main thread (waiting on its gui thread for the destroyed surface), so main-thread
// work would not run until the app is reopened. A dedicated HandlerThread keeps its own Looper running, so
// the button appears and, crucially, disappears immediately even while the app is in the background.
public final class FloatingMenu
{
    private static final String TAG = "Aspia";

    private static HandlerThread sThread = null;
    private static Handler sHandler = null;

    private static WindowManager sWindowManager = null;
    private static View sButton = null;
    private static WindowManager.LayoutParams sButtonParams = null;
    private static View sMenu = null;

    // Latest remote clipboard text, applied to the device the next time the menu syncs the clipboard.
    private static volatile String sPending = null;

    // Last value synced with the device clipboard, to avoid echoing it back to the remote.
    private static String sLastSynced = null;

    // Implemented in floating_menu_bridge.cc; delivers device clipboard text to the session.
    private static native void nativeOnClipboardText(String text);

    private FloatingMenu()
    {
        // Static-only.
    }

    // Posts view work to the dedicated overlay thread (see the class comment). All operations that touch
    // the overlay windows must run on this thread, since that is where they are created.
    private static synchronized void post(Runnable action)
    {
        if (sThread == null)
        {
            sThread = new HandlerThread("AspiaFloatingMenu");
            sThread.start();
            sHandler = new Handler(sThread.getLooper());
        }
        sHandler.post(action);
    }

    public static boolean canDraw(Context context)
    {
        return Settings.canDrawOverlays(context);
    }

    // Opens the "display over other apps" settings so the user can grant the overlay permission.
    public static void openPermissionSettings(Context context)
    {
        Intent intent = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                Uri.parse("package:" + context.getPackageName()));
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        try
        {
            context.startActivity(intent);
        }
        catch (Throwable t)
        {
            Intent fallback = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
            fallback.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            try
            {
                context.startActivity(fallback);
            }
            catch (Throwable t2)
            {
                Log.e(TAG, "Unable to open overlay settings", t2);
            }
        }
    }

    // Shows the floating button (no-op if already shown or the overlay permission is not granted).
    public static void show(final Context context)
    {
        post(() -> showImpl(context));
    }

    private static void showImpl(Context context)
    {
        if (sButton != null)
            return;

        if (!Settings.canDrawOverlays(context))
        {
            Log.w(TAG, "Overlay permission not granted; floating menu unavailable");
            return;
        }

        final float density = context.getResources().getDisplayMetrics().density;
        final int size = Math.round(52 * density);

        // Reproduce the launcher icon as a circle: a solid circle in the icon background color with only
        // the logo (the adaptive icon foreground layer) centered on top. Using the foreground layer keeps
        // the logo at its designed size and avoids cropping.
        ImageView button = new ImageView(context);
        button.setScaleType(ImageView.ScaleType.FIT_CENTER);

        int foregroundId = context.getResources().getIdentifier(
                "ic_launcher_foreground", "drawable", context.getPackageName());
        int backgroundId = context.getResources().getIdentifier(
                "ic_launcher_background", "color", context.getPackageName());

        if (foregroundId != 0)
            button.setImageResource(foregroundId);

        GradientDrawable circle = new GradientDrawable();
        circle.setShape(GradientDrawable.OVAL);
        circle.setColor(backgroundId != 0 ? context.getColor(backgroundId) : Color.WHITE);
        button.setBackground(circle);
        button.setClipToOutline(true);
        button.setElevation(6 * density);

        // Circular ripple drawn on top of the icon for tap feedback.
        GradientDrawable rippleMask = new GradientDrawable();
        rippleMask.setShape(GradientDrawable.OVAL);
        rippleMask.setColor(Color.WHITE);
        button.setForeground(new RippleDrawable(
                ColorStateList.valueOf(Color.argb(80, 255, 255, 255)), null, rippleMask));

        // The button can be dragged; a touch that barely moves is treated as a tap (opens the menu).
        final int touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        button.setOnTouchListener(new View.OnTouchListener()
        {
            private int startX;
            private int startY;
            private float downX;
            private float downY;
            private boolean dragging;

            @Override
            public boolean onTouch(View view, MotionEvent event)
            {
                switch (event.getActionMasked())
                {
                    case MotionEvent.ACTION_DOWN:
                        startX = sButtonParams.x;
                        startY = sButtonParams.y;
                        downX = event.getRawX();
                        downY = event.getRawY();
                        dragging = false;
                        view.setPressed(true);
                        return true;

                    case MotionEvent.ACTION_MOVE:
                    {
                        int dx = Math.round(event.getRawX() - downX);
                        int dy = Math.round(event.getRawY() - downY);
                        if (!dragging && Math.hypot(dx, dy) < touchSlop)
                            return true;
                        if (!dragging)
                        {
                            dragging = true;
                            view.setPressed(false);
                        }
                        sButtonParams.x = startX + dx;
                        sButtonParams.y = startY + dy;
                        if (sWindowManager != null)
                        {
                            try { sWindowManager.updateViewLayout(view, sButtonParams); } catch (Throwable t) {}
                        }
                        return true;
                    }

                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        view.setPressed(false);
                        if (!dragging && event.getActionMasked() == MotionEvent.ACTION_UP)
                            openMenu(context);
                        return true;

                    default:
                        return false;
                }
            }
        });

        final int type = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        sButtonParams = new WindowManager.LayoutParams(
                size, size, type,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
                PixelFormat.TRANSLUCENT);
        sButtonParams.gravity = Gravity.TOP | Gravity.START;

        final DisplayMetrics metrics = context.getResources().getDisplayMetrics();
        sButtonParams.x = metrics.widthPixels - size - Math.round(10 * density);
        sButtonParams.y = metrics.heightPixels / 3;

        sWindowManager = (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        sButton = button;

        try
        {
            sWindowManager.addView(sButton, sButtonParams);
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to add floating button", t);
            sButton = null;
        }
    }

    public static void hide()
    {
        post(FloatingMenu::hideImpl);
    }

    private static void hideImpl()
    {
        closeMenu();

        if (sWindowManager != null && sButton != null)
        {
            try
            {
                sWindowManager.removeView(sButton);
            }
            catch (Throwable t)
            {
                Log.e(TAG, "Unable to remove floating button", t);
            }
        }

        sWindowManager = null;
        sButton = null;
        sButtonParams = null;
        sPending = null;
        sLastSynced = null;
    }

    // Stores the latest remote clipboard text; applied to the device when the menu syncs the clipboard.
    public static void setPending(String text)
    {
        sPending = text;
    }

    private static void openMenu(Context context)
    {
        if (sMenu != null || sWindowManager == null)
            return;

        final float density = context.getResources().getDisplayMetrics().density;

        LinearLayout panel = new LinearLayout(context);
        panel.setOrientation(LinearLayout.VERTICAL);

        GradientDrawable background = new GradientDrawable();
        background.setCornerRadius(14 * density);
        background.setColor(Color.argb(242, 40, 40, 40));
        panel.setBackground(background);
        panel.setElevation(10 * density);
        int pad = Math.round(6 * density);
        panel.setPadding(pad, pad, pad, pad);

        addItem(panel, context, "ic_menu_clipboard", str(context, "menu_sync_clipboard"),
                () -> { syncClipboard(context); closeMenu(); });
        addItem(panel, context, "ic_menu_back", str(context, "menu_back"),
                () -> { InputService.globalAction(1); closeMenu(); });
        addItem(panel, context, "ic_menu_home", str(context, "menu_home"),
                () -> { InputService.globalAction(2); closeMenu(); });
        addItem(panel, context, "ic_menu_recents", str(context, "menu_recents"),
                () -> { InputService.globalAction(3); closeMenu(); });

        panel.setOnTouchListener((view, event) ->
        {
            if (event.getActionMasked() == MotionEvent.ACTION_OUTSIDE)
            {
                closeMenu();
                return true;
            }
            return false;
        });

        final int type = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O)
                ? WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY
                : WindowManager.LayoutParams.TYPE_PHONE;

        // Focusable (no FLAG_NOT_FOCUSABLE) so the clipboard is accessible while the menu is open.
        // FLAG_WATCH_OUTSIDE_TOUCH needs FLAG_NOT_TOUCH_MODAL to deliver ACTION_OUTSIDE, used to close it.
        WindowManager.LayoutParams params = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.WRAP_CONTENT,
                WindowManager.LayoutParams.WRAP_CONTENT,
                type,
                WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
                PixelFormat.TRANSLUCENT);
        params.gravity = Gravity.CENTER;

        sMenu = panel;

        try
        {
            sWindowManager.addView(sMenu, params);
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Unable to add floating menu", t);
            sMenu = null;
        }
    }

    private static void closeMenu()
    {
        if (sWindowManager != null && sMenu != null)
        {
            try
            {
                sWindowManager.removeView(sMenu);
            }
            catch (Throwable t)
            {
                Log.e(TAG, "Unable to remove floating menu", t);
            }
        }

        sMenu = null;
    }

    // Resolves a localized string resource by name (falls back to the name if it is missing).
    private static String str(Context context, String name)
    {
        int id = context.getResources().getIdentifier(name, "string", context.getPackageName());
        return id != 0 ? context.getString(id) : name;
    }

    private static void addItem(LinearLayout panel, Context context, String iconName, String label,
                                Runnable action)
    {
        final float density = context.getResources().getDisplayMetrics().density;

        TextView item = new TextView(context);
        item.setText(label);
        item.setTextColor(Color.WHITE);
        item.setTextSize(16);
        item.setGravity(Gravity.CENTER_VERTICAL);
        item.setPadding(Math.round(20 * density), Math.round(14 * density),
                        Math.round(28 * density), Math.round(14 * density));

        int iconId = context.getResources().getIdentifier(
                iconName, "drawable", context.getPackageName());
        if (iconId != 0)
        {
            item.setCompoundDrawablesWithIntrinsicBounds(iconId, 0, 0, 0);
            item.setCompoundDrawablePadding(Math.round(16 * density));
        }

        item.setBackground(new RippleDrawable(
                ColorStateList.valueOf(Color.argb(120, 255, 255, 255)),
                null, new ColorDrawable(Color.WHITE)));
        item.setOnClickListener(v -> action.run());

        panel.addView(item, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
    }

    // Runs while the (focusable) menu is open, so the app is focused and may touch the clipboard.
    private static void syncClipboard(Context context)
    {
        try
        {
            ClipboardManager manager =
                (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
            if (manager == null)
                return;

            // Send the device clipboard to the remote.
            ClipData clip = manager.getPrimaryClip();
            if (clip != null && clip.getItemCount() > 0)
            {
                CharSequence text = clip.getItemAt(0).coerceToText(context);
                String value = (text != null) ? text.toString() : "";
                if (!value.isEmpty() && !value.equals(sLastSynced))
                {
                    sLastSynced = value;
                    nativeOnClipboardText(value);
                }
            }

            // Apply the remote clipboard to the device.
            if (sPending != null)
            {
                manager.setPrimaryClip(ClipData.newPlainText("", sPending));
                sLastSynced = sPending;
                sPending = null;
            }
        }
        catch (Throwable t)
        {
            Log.e(TAG, "Clipboard sync failed", t);
        }
    }
}
