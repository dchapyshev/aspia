//
// PROJECT:         Aspia
// FILE:            base/shurtcut.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/shortcut.h"

#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>

#include "base/scoped_comptr.h"

namespace aspia {

namespace {

// Initializes |i_shell_link| and |i_persist_file| (releasing them first if they
// are already initialized).
// If |shortcut| is not NULL, loads |shortcut| into |i_persist_file|.
// If any of the above steps fail, both |i_shell_link| and |i_persist_file| will
// be released.
void InitializeShortcutInterfaces(const wchar_t* shortcut,
                                  ScopedComPtr<IShellLinkW>& i_shell_link,
                                  ScopedComPtr<IPersistFile>& i_persist_file)
{
    i_shell_link.Reset();
    i_persist_file.Reset();

    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(i_shell_link.GetAddressOf()))) ||
        FAILED(i_shell_link.CopyTo(i_persist_file.GetAddressOf())) ||
        (shortcut && FAILED((i_persist_file)->Load(shortcut, STGM_READWRITE))))
    {
        i_shell_link.Reset();
        i_persist_file.Reset();
    }
}

}  // namespace

bool CreateOrUpdateShortcutLink(const std::experimental::filesystem::path& shortcut_path,
                                const ShortcutProperties& properties,
                                ShortcutOperation operation)
{
    // A target is required unless |operation| is SHORTCUT_UPDATE_EXISTING.
    if (operation != SHORTCUT_UPDATE_EXISTING &&
        !(properties.options & ShortcutProperties::PROPERTIES_TARGET))
    {
        NOTREACHED();
        return false;
    }

    std::error_code ignored_code;
    bool shortcut_existed = std::experimental::filesystem::exists(shortcut_path, ignored_code);

    // Interfaces to the old shortcut when replacing an existing shortcut.
    ScopedComPtr<IShellLinkW> old_i_shell_link;
    ScopedComPtr<IPersistFile> old_i_persist_file;

    // Interfaces to the shortcut being created/updated.
    ScopedComPtr<IShellLinkW> i_shell_link;
    ScopedComPtr<IPersistFile> i_persist_file;

    switch (operation)
    {
        case SHORTCUT_CREATE_ALWAYS:
            InitializeShortcutInterfaces(nullptr, i_shell_link, i_persist_file);
            break;

        case SHORTCUT_UPDATE_EXISTING:
            InitializeShortcutInterfaces(shortcut_path.c_str(), i_shell_link, i_persist_file);
            break;

        case SHORTCUT_REPLACE_EXISTING:
            InitializeShortcutInterfaces(shortcut_path.c_str(),
                                         old_i_shell_link,
                                         old_i_persist_file);
            // Confirm |shortcut_path| exists and is a shortcut by verifying
            // |old_i_persist_file| was successfully initialized in the call above. If
            // so, initialize the interfaces to begin writing a new shortcut (to
            // overwrite the current one if successful).
            if (old_i_persist_file.Get())
                InitializeShortcutInterfaces(nullptr, i_shell_link, i_persist_file);
            break;

        default:
            NOTREACHED();
    }

    // Return false immediately upon failure to initialize shortcut interfaces.
    if (!i_persist_file.Get())
        return false;

    if ((properties.options & ShortcutProperties::PROPERTIES_TARGET) &&
         FAILED(i_shell_link->SetPath(properties.target.c_str())))
    {
        return false;
    }

    if ((properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) &&
        FAILED(i_shell_link->SetWorkingDirectory(properties.working_dir.c_str())))
    {
        return false;
    }

    if (properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS)
    {
        if (FAILED(i_shell_link->SetArguments(properties.arguments.c_str())))
            return false;
    }
    else if (old_i_persist_file.Get())
    {
        wchar_t current_arguments[MAX_PATH] = { 0 };

        if (SUCCEEDED(old_i_shell_link->GetArguments(current_arguments,
                                                     _countof(current_arguments))))
        {
            i_shell_link->SetArguments(current_arguments);
        }
    }

    if ((properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) &&
        FAILED(i_shell_link->SetDescription(properties.description.c_str())))
    {
        return false;
    }

    if ((properties.options & ShortcutProperties::PROPERTIES_ICON) &&
        FAILED(i_shell_link->SetIconLocation(properties.icon.c_str(), properties.icon_index)))
    {
        return false;
    }

    // Release the interfaces to the old shortcut to make sure it doesn't prevent
    // overwriting it if needed.
    old_i_persist_file.Reset();
    old_i_shell_link.Reset();

    HRESULT result = i_persist_file->Save(shortcut_path.c_str(), TRUE);

    // Release the interfaces in case the SHChangeNotify call below depends on
    // the operations above being fully completed.
    i_persist_file.Reset();
    i_shell_link.Reset();

    // If we successfully created/updated the icon, notify the shell that we have done so.
    const bool succeeded = SUCCEEDED(result);
    if (succeeded)
    {
        if (shortcut_existed)
        {
            // TODO(gab): SHCNE_UPDATEITEM might be sufficient here; further testing required.
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        }
        else
        {
            SHChangeNotify(SHCNE_CREATE, SHCNF_PATH, shortcut_path.c_str(), nullptr);
        }
    }

    return succeeded;
}

bool ResolveShortcutProperties(const std::experimental::filesystem::path& shortcut_path,
                               uint32_t options,
                               ShortcutProperties* properties)
{
    DCHECK(options && properties);

    if (options & ~ShortcutProperties::PROPERTIES_ALL)
        NOTREACHED() << "Unhandled property is used.";

    ScopedComPtr<IShellLink> i_shell_link;

    // Get pointer to the IShellLink interface.
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(i_shell_link.GetAddressOf()))))
    {
        return false;
    }

    ScopedComPtr<IPersistFile> persist;
    // Query IShellLink for the IPersistFile interface.
    if (FAILED(i_shell_link.CopyTo(persist.GetAddressOf())))
        return false;

    // Load the shell link.
    if (FAILED(persist->Load(shortcut_path.c_str(), STGM_READ)))
        return false;

    // Reset |properties|.
    properties->options = 0;

    wchar_t temp[MAX_PATH];
    if (options & ShortcutProperties::PROPERTIES_TARGET)
    {
        if (FAILED(i_shell_link->GetPath(temp, _countof(temp), nullptr, SLGP_UNCPRIORITY)))
            return false;

        properties->set_target(std::experimental::filesystem::path(temp));
    }

    if (options & ShortcutProperties::PROPERTIES_WORKING_DIR)
    {
        if (FAILED(i_shell_link->GetWorkingDirectory(temp, MAX_PATH)))
            return false;

        properties->set_working_dir(std::experimental::filesystem::path(temp));
    }

    if (options & ShortcutProperties::PROPERTIES_ARGUMENTS)
    {
        if (FAILED(i_shell_link->GetArguments(temp, MAX_PATH)))
            return false;
        properties->set_arguments(temp);
    }

    if (options & ShortcutProperties::PROPERTIES_DESCRIPTION)
    {
        // Note: description length constrained by MAX_PATH.
        if (FAILED(i_shell_link->GetDescription(temp, _countof(temp))))
            return false;
        properties->set_description(temp);
    }

    if (options & ShortcutProperties::PROPERTIES_ICON)
    {
        int temp_index;
        if (FAILED(i_shell_link->GetIconLocation(temp, _countof(temp), &temp_index)))
            return false;
        properties->set_icon(std::experimental::filesystem::path(temp), temp_index);
    }

    return true;
}

bool ResolveShortcut(const std::experimental::filesystem::path& shortcut_path,
                     std::experimental::filesystem::path* target_path,
                     std::wstring* args)
{
    uint32_t options = 0;
    if (target_path)
        options |= ShortcutProperties::PROPERTIES_TARGET;
    if (args)
        options |= ShortcutProperties::PROPERTIES_ARGUMENTS;
    DCHECK(options);

    ShortcutProperties properties;
    if (!ResolveShortcutProperties(shortcut_path, options, &properties))
        return false;

    if (target_path)
        *target_path = properties.target;
    if (args)
        *args = properties.arguments;

    return true;
}

} // namespace aspia
