//
// PROJECT:         Aspia
// FILE:            base/shurtcut.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SHORTCUT_H
#define _ASPIA_BASE__SHORTCUT_H

#include <experimental/filesystem>
#include <cstdint>

#include "base/logging.h"

namespace aspia {

enum ShortcutOperation
{
    // Create a new shortcut (overwriting if necessary).
    SHORTCUT_CREATE_ALWAYS = 0,
    // Overwrite an existing shortcut (fails if the shortcut doesn't exist).
    // If the arguments are not specified on the new shortcut, keep the old
    // shortcut's arguments.
    SHORTCUT_REPLACE_EXISTING,
    // Update specified properties only on an existing shortcut.
    SHORTCUT_UPDATE_EXISTING,
};

// Properties for shortcuts. Properties set will be applied to the shortcut on
// creation/update, others will be ignored.
// Callers are encouraged to use the setters provided which take care of
// setting |options| as desired.
struct ShortcutProperties
{
    enum IndividualProperties
    {
        PROPERTIES_TARGET = 1U << 0,
        PROPERTIES_WORKING_DIR = 1U << 1,
        PROPERTIES_ARGUMENTS = 1U << 2,
        PROPERTIES_DESCRIPTION = 1U << 3,
        PROPERTIES_ICON = 1U << 4,
        // Be sure to update the values below when adding a new property.
        PROPERTIES_ALL = PROPERTIES_TARGET | PROPERTIES_WORKING_DIR |
                         PROPERTIES_ARGUMENTS | PROPERTIES_DESCRIPTION |
                         PROPERTIES_ICON
    };

    ShortcutProperties() = default;
    ShortcutProperties(const ShortcutProperties& other) = default;
    ~ShortcutProperties() = default;

    void set_target(const std::experimental::filesystem::path& target_in)
    {
        target = target_in;
        options |= PROPERTIES_TARGET;
    }

    void set_working_dir(const std::experimental::filesystem::path& working_dir_in)
    {
        working_dir = working_dir_in;
        options |= PROPERTIES_WORKING_DIR;
    }

    void set_arguments(const std::wstring& arguments_in)
    {
        // Size restriction as per MSDN at http://goo.gl/TJ7q5.
        DCHECK(arguments_in.size() < MAX_PATH);
        arguments = arguments_in;
        options |= PROPERTIES_ARGUMENTS;
    }

    void set_description(const std::wstring& description_in)
    {
        // Size restriction as per MSDN at http://goo.gl/OdNQq.
        DCHECK(description_in.size() < MAX_PATH);
        description = description_in;
        options |= PROPERTIES_DESCRIPTION;
    }

    void set_icon(const std::experimental::filesystem::path& icon_in, int icon_index_in)
    {
        icon = icon_in;
        icon_index = icon_index_in;
        options |= PROPERTIES_ICON;
    }

    // The target to launch from this shortcut. This is mandatory when creating a shortcut.
    std::experimental::filesystem::path target;
    // The name of the working directory when launching the shortcut.
    std::experimental::filesystem::path working_dir;
    // The arguments to be applied to |target| when launching from this shortcut.
    // The length of this string must be less than MAX_PATH.
    std::wstring arguments;
    // The localized description of the shortcut.
    // The length of this string must be less than MAX_PATH.
    std::wstring description;
    // The path to the icon (can be a dll or exe, in which case |icon_index| is the resource id).
    std::experimental::filesystem::path icon;
    int icon_index = -1;
    // Bitfield made of IndividualProperties. Properties set in |options| will be
    // set on the shortcut, others will be ignored.
    uint32_t options = 0;
};

// This method creates (or updates) a shortcut link at |shortcut_path| using the
// information given through |properties|.
// Ensure you have initialized COM before calling into this function.
// |operation|: a choice from the ShortcutOperation enum.
// If |operation| is SHORTCUT_REPLACE_EXISTING or SHORTCUT_UPDATE_EXISTING and
// |shortcut_path| does not exist, this method is a no-op and returns false.
bool CreateOrUpdateShortcutLink(
    const std::experimental::filesystem::path& shortcut_path,
    const ShortcutProperties& properties,
    ShortcutOperation operation);

// Resolves Windows shortcut (.LNK file).
// This methods tries to resolve selected properties of a shortcut .LNK file.
// The path of the shortcut to resolve is in |shortcut_path|. |options| is a bit
// field composed of ShortcutProperties::IndividualProperties, to specify which
// properties to read. It should be non-0. The resulting data are read into
// |properties|, which must not be NULL. Note: PROPERTIES_TARGET will retrieve
// the target path as stored in the shortcut but won't attempt to resolve that
// path so it may not be valid. The function returns true if all requested
// properties are successfully read. Otherwise some reads have failed and
// intermediate values written to |properties| should be ignored.
bool ResolveShortcutProperties(const std::experimental::filesystem::path& shortcut_path,
                               uint32_t options,
                               ShortcutProperties* properties);

// Resolves Windows shortcut (.LNK file).
// This is a wrapper to ResolveShortcutProperties() to handle the common use
// case of resolving target and arguments. |target_path| and |args| are
// optional output variables that are ignored if NULL (but at least one must be
// non-NULL). The function returns true if all requested fields are found
// successfully. Callers can safely use the same variable for both
// |shortcut_path| and |target_path|.
bool ResolveShortcut(const std::experimental::filesystem::path& shortcut_path,
                     std::experimental::filesystem::path* target_path,
                     std::wstring* args);

} // namespace aspia

#endif // _ASPIA_BASE__SHORTCUT_H
