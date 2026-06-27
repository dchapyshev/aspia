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

#ifndef BASE_LINUX_LIBX11_H
#define BASE_LINUX_LIBX11_H

#include <QtClassHelperMacros>

#include "base/linux/x11_headers.h"

// Thin wrapper over the subset of the core Xlib (libX11) used by the project. The library is loaded
// dynamically on first use and the resolved symbols are cached, so the binary does not link against
// libX11. Every call is a no-op (returning a zero result) if the library or a symbol is unavailable.
class LibX11
{
    Q_DISABLE_COPY_MOVE(LibX11)

public:
    // Loads libX11 and resolves its symbols once. Returns false if it is not available.
    static bool ensureLoaded();

    static Display* openDisplay(const char* display_name);
    static int closeDisplay(Display* display);
    static int sync(Display* display, int discard);
    static int flush(Display* display);
    static int free(void* data);
    static int pending(Display* display);
    static int nextEvent(Display* display, XEvent* event);

    static Atom internAtom(Display* display, const char* atom_name, int only_if_exists);
    static int internAtoms(
        Display* display, char** names, int count, int only_if_exists, Atom* atoms_return);
    static char* getAtomName(Display* display, Atom atom);

    static int changeProperty(Display* display, Window window, Atom property, Atom type, int format,
                              int mode, const unsigned char* data, int nelements);
    static int getWindowProperty(
        Display* display, Window window, Atom property, long long_offset, long long_length, int del,
        Atom req_type, Atom* actual_type, int* actual_format, unsigned long* nitems,
        unsigned long* bytes_after, unsigned char** prop_return);
    static int getWindowAttributes(Display* display, Window window, XWindowAttributes* attributes);

    static Window rootWindow(Display* display, int screen_number);
    static Window createSimpleWindow(
        Display* display, Window parent, int x, int y, unsigned int width, unsigned int height,
        unsigned int border_width, unsigned long border, unsigned long background);
    static int selectInput(Display* display, Window window, long event_mask);
    static int sendEvent(Display* display, Window window, int propagate, long event_mask, XEvent* event);
    static int translateCoordinates(
        Display* display, Window src, Window dest, int src_x, int src_y, int* dest_x, int* dest_y,
        Window* child);
    static int queryPointer(
        Display* display, Window window, Window* root, Window* child, int* root_x, int* root_y,
        int* win_x, int* win_y, unsigned int* mask);
    static int queryExtension(
        Display* display, const char* name, int* major_opcode, int* first_event, int* first_error);

    static GC createGc(Display* display, Drawable drawable, unsigned long valuemask, XGCValues* values);
    static int freeGc(Display* display, GC gc);
    static int freePixmap(Display* display, Pixmap pixmap);
    static XImage* getImage(Display* display, Drawable drawable, int x, int y, unsigned int width,
                            unsigned int height, unsigned long plane_mask, int format);
    static int copyArea(Display* display, Drawable src, Drawable dest, GC gc, int src_x, int src_y,
                        unsigned int width, unsigned int height, int dest_x, int dest_y);

    static Window getSelectionOwner(Display* display, Atom selection);
    static int setSelectionOwner(Display* display, Atom selection, Window owner, Time time);
    static int convertSelection(
        Display* display, Atom selection, Atom target, Atom property, Window requestor, Time time);

    static int getPointerMapping(Display* display, unsigned char* map, int nmap);
    static int displayKeycodes(Display* display, int* min_keycodes, int* max_keycodes);
    static KeySym* getKeyboardMapping(
        Display* display, KeyCode first_keycode, int keycode_count, int* keysyms_per_keycode);
    static int changeKeyboardMapping(
        Display* display, int first_keycode, int keysyms_per_keycode, KeySym* keysyms, int num_codes);
    static int getKeyboardControl(Display* display, XKeyboardState* values);
    static int changeKeyboardControl(Display* display, unsigned long value_mask, XKeyboardControl* values);

    static int xkbGetState(Display* display, unsigned int device_spec, XkbStatePtr state);
    static int xkbGetIndicatorState(Display* display, unsigned int device_spec, unsigned int* state);
    static unsigned int xkbKeysymToModifiers(Display* display, KeySym ks);
    static int xkbLockModifiers(
        Display* display, unsigned int device_spec, unsigned int affect, unsigned int values);
    static int xkbLookupKeySym(
        Display* display, KeyCode keycode, unsigned int modifiers, unsigned int* modifiers_return,
        KeySym* keysym_return);

    static XErrorHandler setErrorHandler(XErrorHandler handler);
    static XIOErrorHandler setIoErrorHandler(XIOErrorHandler handler);
};

#endif // BASE_LINUX_LIBX11_H
