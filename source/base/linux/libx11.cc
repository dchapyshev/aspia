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

#include "base/linux/libx11.h"

#include <dlfcn.h>

#include "base/logging.h"

namespace {

// Pointer types are taken from the real Xlib declarations; only the addresses are resolved dynamically.
void* g_handle = nullptr;
bool g_load_failed = false;

decltype(&XOpenDisplay) g_open_display = nullptr;
decltype(&XCloseDisplay) g_close_display = nullptr;
decltype(&XSync) g_sync = nullptr;
decltype(&XFlush) g_flush = nullptr;
decltype(&XFree) g_free = nullptr;
decltype(&XPending) g_pending = nullptr;
decltype(&XNextEvent) g_next_event = nullptr;
decltype(&XInternAtom) g_intern_atom = nullptr;
decltype(&XInternAtoms) g_intern_atoms = nullptr;
decltype(&XGetAtomName) g_get_atom_name = nullptr;
decltype(&XChangeProperty) g_change_property = nullptr;
decltype(&XGetWindowProperty) g_get_window_property = nullptr;
decltype(&XGetWindowAttributes) g_get_window_attributes = nullptr;
decltype(&XRootWindow) g_root_window = nullptr;
decltype(&XCreateSimpleWindow) g_create_simple_window = nullptr;
decltype(&XSelectInput) g_select_input = nullptr;
decltype(&XSendEvent) g_send_event = nullptr;
decltype(&XTranslateCoordinates) g_translate_coordinates = nullptr;
decltype(&XQueryPointer) g_query_pointer = nullptr;
decltype(&XQueryExtension) g_query_extension = nullptr;
decltype(&XCreateGC) g_create_gc = nullptr;
decltype(&XFreeGC) g_free_gc = nullptr;
decltype(&XFreePixmap) g_free_pixmap = nullptr;
decltype(&XGetImage) g_get_image = nullptr;
decltype(&XCopyArea) g_copy_area = nullptr;
decltype(&XGetSelectionOwner) g_get_selection_owner = nullptr;
decltype(&XSetSelectionOwner) g_set_selection_owner = nullptr;
decltype(&XConvertSelection) g_convert_selection = nullptr;
decltype(&XGetPointerMapping) g_get_pointer_mapping = nullptr;
decltype(&XDisplayKeycodes) g_display_keycodes = nullptr;
decltype(&XGetKeyboardMapping) g_get_keyboard_mapping = nullptr;
decltype(&XChangeKeyboardMapping) g_change_keyboard_mapping = nullptr;
decltype(&XGetKeyboardControl) g_get_keyboard_control = nullptr;
decltype(&XChangeKeyboardControl) g_change_keyboard_control = nullptr;
decltype(&XkbGetState) g_xkb_get_state = nullptr;
decltype(&XkbGetIndicatorState) g_xkb_get_indicator_state = nullptr;
decltype(&XkbKeysymToModifiers) g_xkb_keysym_to_modifiers = nullptr;
decltype(&XkbLockModifiers) g_xkb_lock_modifiers = nullptr;
decltype(&XkbLookupKeySym) g_xkb_lookup_keysym = nullptr;
decltype(&XSetErrorHandler) g_set_error_handler = nullptr;
decltype(&XSetIOErrorHandler) g_set_io_error_handler = nullptr;

//--------------------------------------------------------------------------------------------------
template <typename T>
bool resolve(T& fn, const char* name)
{
    fn = reinterpret_cast<T>(dlsym(g_handle, name));
    return fn != nullptr;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool LibX11::ensureLoaded()
{
    if (g_handle)
        return true;

    // Do not retry: a missing library will not appear later and dlopen is not free.
    if (g_load_failed)
        return false;

    g_handle = dlopen("libX11.so.6", RTLD_LAZY);
    if (!g_handle)
    {
        LOG(ERROR) << "Unable to load libX11.so.6:" << dlerror();
        g_load_failed = true;
        return false;
    }

    bool ok = true;
    ok &= resolve(g_open_display, "XOpenDisplay");
    ok &= resolve(g_close_display, "XCloseDisplay");
    ok &= resolve(g_sync, "XSync");
    ok &= resolve(g_flush, "XFlush");
    ok &= resolve(g_free, "XFree");
    ok &= resolve(g_pending, "XPending");
    ok &= resolve(g_next_event, "XNextEvent");
    ok &= resolve(g_intern_atom, "XInternAtom");
    ok &= resolve(g_intern_atoms, "XInternAtoms");
    ok &= resolve(g_get_atom_name, "XGetAtomName");
    ok &= resolve(g_change_property, "XChangeProperty");
    ok &= resolve(g_get_window_property, "XGetWindowProperty");
    ok &= resolve(g_get_window_attributes, "XGetWindowAttributes");
    ok &= resolve(g_root_window, "XRootWindow");
    ok &= resolve(g_create_simple_window, "XCreateSimpleWindow");
    ok &= resolve(g_select_input, "XSelectInput");
    ok &= resolve(g_send_event, "XSendEvent");
    ok &= resolve(g_translate_coordinates, "XTranslateCoordinates");
    ok &= resolve(g_query_pointer, "XQueryPointer");
    ok &= resolve(g_query_extension, "XQueryExtension");
    ok &= resolve(g_create_gc, "XCreateGC");
    ok &= resolve(g_free_gc, "XFreeGC");
    ok &= resolve(g_free_pixmap, "XFreePixmap");
    ok &= resolve(g_get_image, "XGetImage");
    ok &= resolve(g_copy_area, "XCopyArea");
    ok &= resolve(g_get_selection_owner, "XGetSelectionOwner");
    ok &= resolve(g_set_selection_owner, "XSetSelectionOwner");
    ok &= resolve(g_convert_selection, "XConvertSelection");
    ok &= resolve(g_get_pointer_mapping, "XGetPointerMapping");
    ok &= resolve(g_display_keycodes, "XDisplayKeycodes");
    ok &= resolve(g_get_keyboard_mapping, "XGetKeyboardMapping");
    ok &= resolve(g_change_keyboard_mapping, "XChangeKeyboardMapping");
    ok &= resolve(g_get_keyboard_control, "XGetKeyboardControl");
    ok &= resolve(g_change_keyboard_control, "XChangeKeyboardControl");
    ok &= resolve(g_xkb_get_state, "XkbGetState");
    ok &= resolve(g_xkb_get_indicator_state, "XkbGetIndicatorState");
    ok &= resolve(g_xkb_keysym_to_modifiers, "XkbKeysymToModifiers");
    ok &= resolve(g_xkb_lock_modifiers, "XkbLockModifiers");
    ok &= resolve(g_xkb_lookup_keysym, "XkbLookupKeySym");
    ok &= resolve(g_set_error_handler, "XSetErrorHandler");
    ok &= resolve(g_set_io_error_handler, "XSetIOErrorHandler");

    if (!ok)
    {
        LOG(ERROR) << "Unable to resolve libX11 symbols";
        dlclose(g_handle);
        g_handle = nullptr;
        g_load_failed = true;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
Display* LibX11::openDisplay(const char* display_name)
{
    if (!ensureLoaded())
        return nullptr;
    return g_open_display(display_name);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::closeDisplay(Display* display)
{
    if (!ensureLoaded())
        return 0;
    return g_close_display(display);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::sync(Display* display, int discard)
{
    if (!ensureLoaded())
        return 0;
    return g_sync(display, discard);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::flush(Display* display)
{
    if (!ensureLoaded())
        return 0;
    return g_flush(display);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::free(void* data)
{
    if (!ensureLoaded())
        return 0;
    return g_free(data);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::pending(Display* display)
{
    if (!ensureLoaded())
        return 0;
    return g_pending(display);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::nextEvent(Display* display, XEvent* event)
{
    if (!ensureLoaded())
        return 0;
    return g_next_event(display, event);
}

//--------------------------------------------------------------------------------------------------
// static
Atom LibX11::internAtom(Display* display, const char* atom_name, int only_if_exists)
{
    if (!ensureLoaded())
        return 0;
    return g_intern_atom(display, atom_name, only_if_exists);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::internAtoms(
    Display* display, char** names, int count, int only_if_exists, Atom* atoms_return)
{
    if (!ensureLoaded())
        return 0;
    return g_intern_atoms(display, names, count, only_if_exists, atoms_return);
}

//--------------------------------------------------------------------------------------------------
// static
char* LibX11::getAtomName(Display* display, Atom atom)
{
    if (!ensureLoaded())
        return nullptr;
    return g_get_atom_name(display, atom);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::changeProperty(Display* display, Window window, Atom property, Atom type, int format,
                           int mode, const unsigned char* data, int nelements)
{
    if (!ensureLoaded())
        return 0;
    return g_change_property(display, window, property, type, format, mode, data, nelements);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::getWindowProperty(
    Display* display, Window window, Atom property, long long_offset, long long_length, int del,
    Atom req_type, Atom* actual_type, int* actual_format, unsigned long* nitems,
    unsigned long* bytes_after, unsigned char** prop_return)
{
    if (!ensureLoaded())
        return 0;
    return g_get_window_property(display, window, property, long_offset, long_length, del, req_type,
                                 actual_type, actual_format, nitems, bytes_after, prop_return);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::getWindowAttributes(Display* display, Window window, XWindowAttributes* attributes)
{
    if (!ensureLoaded())
        return 0;
    return g_get_window_attributes(display, window, attributes);
}

//--------------------------------------------------------------------------------------------------
// static
Window LibX11::rootWindow(Display* display, int screen_number)
{
    if (!ensureLoaded())
        return 0;
    return g_root_window(display, screen_number);
}

//--------------------------------------------------------------------------------------------------
// static
Window LibX11::createSimpleWindow(
    Display* display, Window parent, int x, int y, unsigned int width, unsigned int height,
    unsigned int border_width, unsigned long border, unsigned long background)
{
    if (!ensureLoaded())
        return 0;
    return g_create_simple_window(
        display, parent, x, y, width, height, border_width, border, background);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::selectInput(Display* display, Window window, long event_mask)
{
    if (!ensureLoaded())
        return 0;
    return g_select_input(display, window, event_mask);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::sendEvent(Display* display, Window window, int propagate, long event_mask, XEvent* event)
{
    if (!ensureLoaded())
        return 0;
    return g_send_event(display, window, propagate, event_mask, event);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::translateCoordinates(
    Display* display, Window src, Window dest, int src_x, int src_y, int* dest_x, int* dest_y,
    Window* child)
{
    if (!ensureLoaded())
        return 0;
    return g_translate_coordinates(display, src, dest, src_x, src_y, dest_x, dest_y, child);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::queryPointer(
    Display* display, Window window, Window* root, Window* child, int* root_x, int* root_y,
    int* win_x, int* win_y, unsigned int* mask)
{
    if (!ensureLoaded())
        return 0;
    return g_query_pointer(display, window, root, child, root_x, root_y, win_x, win_y, mask);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::queryExtension(
    Display* display, const char* name, int* major_opcode, int* first_event, int* first_error)
{
    if (!ensureLoaded())
        return 0;
    return g_query_extension(display, name, major_opcode, first_event, first_error);
}

//--------------------------------------------------------------------------------------------------
// static
GC LibX11::createGc(Display* display, Drawable drawable, unsigned long valuemask, XGCValues* values)
{
    if (!ensureLoaded())
        return nullptr;
    return g_create_gc(display, drawable, valuemask, values);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::freeGc(Display* display, GC gc)
{
    if (!ensureLoaded())
        return 0;
    return g_free_gc(display, gc);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::freePixmap(Display* display, Pixmap pixmap)
{
    if (!ensureLoaded())
        return 0;
    return g_free_pixmap(display, pixmap);
}

//--------------------------------------------------------------------------------------------------
// static
XImage* LibX11::getImage(Display* display, Drawable drawable, int x, int y, unsigned int width,
                         unsigned int height, unsigned long plane_mask, int format)
{
    if (!ensureLoaded())
        return nullptr;
    return g_get_image(display, drawable, x, y, width, height, plane_mask, format);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::copyArea(Display* display, Drawable src, Drawable dest, GC gc, int src_x, int src_y,
                     unsigned int width, unsigned int height, int dest_x, int dest_y)
{
    if (!ensureLoaded())
        return 0;
    return g_copy_area(display, src, dest, gc, src_x, src_y, width, height, dest_x, dest_y);
}

//--------------------------------------------------------------------------------------------------
// static
Window LibX11::getSelectionOwner(Display* display, Atom selection)
{
    if (!ensureLoaded())
        return 0;
    return g_get_selection_owner(display, selection);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::setSelectionOwner(Display* display, Atom selection, Window owner, Time time)
{
    if (!ensureLoaded())
        return 0;
    return g_set_selection_owner(display, selection, owner, time);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::convertSelection(
    Display* display, Atom selection, Atom target, Atom property, Window requestor, Time time)
{
    if (!ensureLoaded())
        return 0;
    return g_convert_selection(display, selection, target, property, requestor, time);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::getPointerMapping(Display* display, unsigned char* map, int nmap)
{
    if (!ensureLoaded())
        return 0;
    return g_get_pointer_mapping(display, map, nmap);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::displayKeycodes(Display* display, int* min_keycodes, int* max_keycodes)
{
    if (!ensureLoaded())
        return 0;
    return g_display_keycodes(display, min_keycodes, max_keycodes);
}

//--------------------------------------------------------------------------------------------------
// static
KeySym* LibX11::getKeyboardMapping(
    Display* display, KeyCode first_keycode, int keycode_count, int* keysyms_per_keycode)
{
    if (!ensureLoaded())
        return nullptr;
    return g_get_keyboard_mapping(display, first_keycode, keycode_count, keysyms_per_keycode);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::changeKeyboardMapping(
    Display* display, int first_keycode, int keysyms_per_keycode, KeySym* keysyms, int num_codes)
{
    if (!ensureLoaded())
        return 0;
    return g_change_keyboard_mapping(display, first_keycode, keysyms_per_keycode, keysyms, num_codes);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::getKeyboardControl(Display* display, XKeyboardState* values)
{
    if (!ensureLoaded())
        return 0;
    return g_get_keyboard_control(display, values);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::changeKeyboardControl(Display* display, unsigned long value_mask, XKeyboardControl* values)
{
    if (!ensureLoaded())
        return 0;
    return g_change_keyboard_control(display, value_mask, values);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::xkbGetState(Display* display, unsigned int device_spec, XkbStatePtr state)
{
    if (!ensureLoaded())
        return 0;
    return g_xkb_get_state(display, device_spec, state);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::xkbGetIndicatorState(Display* display, unsigned int device_spec, unsigned int* state)
{
    if (!ensureLoaded())
        return 0;
    return g_xkb_get_indicator_state(display, device_spec, state);
}

//--------------------------------------------------------------------------------------------------
// static
unsigned int LibX11::xkbKeysymToModifiers(Display* display, KeySym ks)
{
    if (!ensureLoaded())
        return 0;
    return g_xkb_keysym_to_modifiers(display, ks);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::xkbLockModifiers(
    Display* display, unsigned int device_spec, unsigned int affect, unsigned int values)
{
    if (!ensureLoaded())
        return 0;
    return g_xkb_lock_modifiers(display, device_spec, affect, values);
}

//--------------------------------------------------------------------------------------------------
// static
int LibX11::xkbLookupKeySym(
    Display* display, KeyCode keycode, unsigned int modifiers, unsigned int* modifiers_return,
    KeySym* keysym_return)
{
    if (!ensureLoaded())
        return 0;
    return g_xkb_lookup_keysym(display, keycode, modifiers, modifiers_return, keysym_return);
}

//--------------------------------------------------------------------------------------------------
// static
XErrorHandler LibX11::setErrorHandler(XErrorHandler handler)
{
    if (!ensureLoaded())
        return nullptr;
    return g_set_error_handler(handler);
}

//--------------------------------------------------------------------------------------------------
// static
XIOErrorHandler LibX11::setIoErrorHandler(XIOErrorHandler handler)
{
    if (!ensureLoaded())
        return nullptr;
    return g_set_io_error_handler(handler);
}
