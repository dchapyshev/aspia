#pragma sw require header org.sw.demo.google.protobuf.protoc
#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc-6

#define QT_VERSION_NUMBER 6
#define QT_VERSION "-6"

/*void configure(Build &s)
{
    if (s.isConfigSelected("mt"))
    {
        auto ss = s.createSettings();
        ss.Native.LibrariesType = LibraryType::Static;
        ss.Native.MT = true;
        s.addSettings(ss);
    }
}*/

void build(Solution &s) {
    auto &aspia = s.addProject("aspia", "2.7.0");
    aspia += Git("https://github.com/dchapyshev/aspia", "v{v}");

    constexpr auto cppstd = cpp17;

    auto setup_target = [&](auto &t, const String &name, bool add_tests = false, String dir = {}) -> decltype(auto) {
        if (dir.empty())
            dir = name;

        t += cppstd;
        t.Public += "."_idir;
        t.setRootDirectory(dir);
        t += IncludeDirectory("."s);
        t += ".*"_rr;

        //
        t.AllowEmptyRegexes = true;

        // os specific
        t -= ".*_win\\..*"_rr;
        t -= ".*/win/.*"_rr;
        t -= ".*_linux.*"_rr;
        t -= ".*/linux/.*"_rr;
        t -= ".*_pulse.*"_rr;
        t -= ".*_mac.*"_rr;
        t -= ".*/mac/.*"_rr;
        t -= ".*_posix.*"_rr;
        t -= ".*_x11.*"_rr;
        t -= ".*/x11/.*"_rr;
        t -= ".*x11/.*"_rr;
        if (t.getBuildSettings().TargetOS.Type == OSType::Windows) {
            t += ".*_win.*"_rr;
            t += ".*/win/.*"_rr;
        } else if (t.getBuildSettings().TargetOS.isApple()) {
            t += ".*_mac.*"_rr;
            t += ".*/mac/.*"_rr;
        } else         {
            t += ".*_pulse.*"_rr;
            t += ".*_linux.*"_rr;
            t += ".*/linux/.*"_rr;
            t += ".*_x11.*"_rr;
            t += ".*/x11/.*"_rr;
            t += ".*x11/.*"_rr;
        }
        if (t.getBuildSettings().TargetOS.Type != OSType::Windows) {
            t.ExportAllSymbols = true;
            t += ".*_posix.*"_rr;
        }

        t -= ".*_unittest.*"_rr;
        t -= ".*tests.*"_rr;

        //
        t.AllowEmptyRegexes = false;

        // test
        if (add_tests) {
            auto &bt = t.addExecutable("test");
            bt += cppstd;
            bt += FileRegex(dir, ".*_unittest.*", true);
            bt += t;
            bt += "org.sw.demo.google.googletest.gmock"_dep;
            bt += "org.sw.demo.google.googletest.gtest.main"_dep;
            t.addTest(bt);
        }

        return t;
    };

    auto add_lib = [&aspia, &setup_target](const String &name, bool add_tests = false, String dir = {}) -> decltype(auto) {
        return setup_target(aspia.addStaticLibrary(name), name, add_tests, dir);
    };

    auto &protocol = aspia.addStaticLibrary("proto");
    protocol += cppstd;
    protocol += "proto/.*\\.proto"_rr;
    for (const auto &[p, _] : protocol[FileRegex(protocol.SourceDir / "proto", ".*\\.proto", false)]) {
        ProtobufData d;
        d.outdir = protocol.BinaryDir / "proto";
        d.public_protobuf = true;
        d.addIncludeDirectory(protocol.SourceDir / "proto");
        gen_protobuf_cpp("org.sw.demo.google.protobuf"_dep, protocol, p, d);
    }

    auto &base = aspia.addStaticLibrary("base");
    {
        base += cppstd;
        base -= "third_party/.*"_rr;
        base += "third_party/modp_b64/.*\\.[hc]"_rr;
        base += "third_party/x11region/.*\\.[hc]"_rr;
        if (base.getBuildSettings().TargetOS.Type == OSType::Linux)
            base += "third_party/xdg_user_dirs/.*"_rr;
        if (base.getBuildSettings().TargetOS.isApple())
            base += "third_party/portaudio/.*"_rr;
        base -= "build/.*"_rr;
        setup_target(base, "base", false);
        base -= "peer/stun_server.cc";
        base -= "peer/stun_peer.cc";
        if (base.getBuildSettings().TargetOS.Type == OSType::Windows) {
            base.Public += "UNICODE"_def;
            base.Public += "WIN32_LEAN_AND_MEAN"_def;
            base.Public += "NOMINMAX"_def;
        }
        base.Public += protocol;
        base.Public += "org.sw.demo.qtproject.qt.base.widgets" QT_VERSION ""_dep;
        base.Public += "org.sw.demo.qtproject.qt.base.network" QT_VERSION ""_dep;
        base.Public += "org.sw.demo.qtproject.qt.base.xml" QT_VERSION ""_dep;
        base.Public += "org.sw.demo.boost.align"_dep;
        base.Public += "org.sw.demo.imneme.pcg_cpp-master"_dep;
        base.Public += "org.sw.demo.chriskohlhoff.asio"_dep;
        base.Public += "org.sw.demo.rapidxml"_dep;
        base.Public += "org.sw.demo.miloyip.rapidjson"_dep;
        base.Public += "org.sw.demo.google.protobuf.protobuf"_dep; // should be protobuf_lite actually?
        base.Public += "org.sw.demo.chromium.libyuv-master"_dep;
        base.Public += "org.sw.demo.webmproject.vpx"_dep;
        base.Public += "org.sw.demo.webmproject.webm"_dep;
        base.Public += "org.sw.demo.xiph.opus"_dep;
        base.Public += "org.sw.demo.sqlite3"_dep;
        base.Public += "org.sw.demo.badger.curl.libcurl"_dep;
        if (base.getBuildSettings().TargetOS.Type == OSType::Windows) {
            base.Public += "com.Microsoft.Windows.SDK.winrt"_dep;
            base +=
                "Dbghelp.lib"_slib,
                "Mswsock.lib"_slib,
                "Avrt.lib"_slib,
                "comsuppw.lib"_slib,
                "Winspool.lib"_slib,
                "Setupapi.lib"_slib,
                "Wtsapi32.lib"_slib
                ;
        } else {
            base -= "win/.*"_rr;
            base -= "desktop/frame_dib.cc";
            base -= "desktop/screen_capturer_dxgi.cc";
            base -= "desktop/screen_capturer_wrapper.cc";
            base -= "desktop/screen_capturer_mirror.cc";
            base -= "desktop/screen_capturer_gdi.cc";
            base -= "net/connect_enumerator.cc";
            base -= "net/firewall_manager.cc";
            base -= "net/route_enumerator.cc";
            base -= "net/open_files_enumerator.cc";
        }
        if (base.getBuildSettings().TargetOS.Type == OSType::Linux) {
            base += "X11"_slib;
            base += "Xext"_slib;
            base += "Xdamage"_slib;
            base += "Xfixes"_slib;
            base += "Xtst"_slib;
            base += "Xrandr"_slib;
        }
        if (base.getBuildSettings().TargetOS.isApple()) {
            base.Protected += "AudioToolbox"_framework;
            base.Protected += "CoreAudio"_framework;
            // for shared build
            base.Protected += "AppKit"_framework;
            base.Protected += "CoreFoundation"_framework;
            base.Protected += "CoreGraphics"_framework;
            base.Protected += "Foundation"_framework;
        }
        automoc("org.sw.demo.qtproject.qt.base.tools.moc" QT_VERSION ""_dep, base);

        base.Public += Definition("ASPIA_VERSION_MAJOR=" + base.Variables["PACKAGE_VERSION_MAJOR"].toString());
        base.Public += Definition("ASPIA_VERSION_MINOR=" + base.Variables["PACKAGE_VERSION_MINOR"].toString());
        base.Public += Definition("ASPIA_VERSION_PATCH=" + base.Variables["PACKAGE_VERSION_PATCH"].toString());
        base.Public += Definition("GIT_COMMIT_HASH=\"\"");
        base.Public += Definition("GIT_COMMIT_COUNT=0");
    }

    auto &relay = aspia.addExecutable("relay");
    {
        relay += cppstd;
        relay -= "relay/.*"_rr;
        relay += "relay/.*"_r;
        if (relay.getBuildSettings().TargetOS.Type == OSType::Windows) {
            relay += "relay/win/.*"_rr;
        } else {
            //relay -= "relay/service.cc";
        }
        if (relay.getBuildSettings().TargetOS.Type == OSType::Linux) {
            relay += "relay/linux/.*"_rr;
        }
        relay += base;
    }

    auto &router = aspia.addExecutable("router");
    {
        router += cppstd;
        router -= "router/.*"_rr;
        router += "router/.*"_r;
        if (router.getBuildSettings().TargetOS.Type == OSType::Windows) {
            router += "router/win/.*"_rr;
        } else {
            //router -= "router/service.cc";
        }
        if (router.getBuildSettings().TargetOS.Type == OSType::Linux) {
            router += "router/linux/.*"_rr;
        }
        router += base;
    }

    auto qt_progs = [](auto &t, const String &name_override = {}, const path &path_override = {}) {
        auto name = name_override.empty() ? t.getPackage().getPath().back() : name_override;
        automoc("org.sw.demo.qtproject.qt.base.tools.moc" QT_VERSION ""_dep, t);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc" QT_VERSION ""_dep, t, t.SourceDir / path_override / ("resources/" + name + ".qrc"));
        qt_uic("org.sw.demo.qtproject.qt.base.tools.uic" QT_VERSION ""_dep, t);
    };

    auto qt_progs2 = [](auto &t) {
        automoc("org.sw.demo.qtproject.qt.base.tools.moc" QT_VERSION ""_dep, t);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc" QT_VERSION ""_dep, t, t.SourceDir / "ui/resources.qrc");
        qt_uic("org.sw.demo.qtproject.qt.base.tools.uic" QT_VERSION ""_dep, t);
    };

    auto qt_progs_and_tr = [&qt_progs](auto &t, const String &name_override = {}, const path &path_override = {}) {
        auto name = name_override.empty() ? t.getPackage().getPath().back() : name_override;
        qt_tr("org.sw.demo.qtproject.qt" QT_VERSION ""_dep, t);
        qt_progs(t, name_override, path_override); // after tr
        t.configureFile(t.SourceDir / path_override / ("translations/" + name + "_translations.qrc"),
            t.BinaryDir / (name + "_translations.qrc"), ConfigureFlags::CopyOnly);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc" QT_VERSION ""_dep, t,
            t.BinaryDir / (name + "_translations.qrc"))
            ->working_directory = t.BinaryDir;
    };

    auto qt_progs_and_tr2 = [&qt_progs2](auto &t) {
        qt_tr("org.sw.demo.qtproject.qt" QT_VERSION ""_dep, t);
        qt_progs2(t); // after tr
        t.configureFile(t.SourceDir / "ui/translations.qrc",
            t.BinaryDir / "translations.qrc", ConfigureFlags::CopyOnly);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc" QT_VERSION ""_dep, t,
            t.BinaryDir / "translations.qrc")->working_directory = t.BinaryDir;
    };

    auto &common = add_lib("common");
    {
        if (common.getBuildSettings().TargetOS.Type == OSType::Windows) {
            common -= "file_enumerator_fs.cc";
            common.Public += "Shlwapi.lib"_slib;
        }
        common.Public += base, protocol;
        common.Public += "org.sw.demo.openssl.crypto"_dep;
        common.Public += "org.sw.demo.qtproject.qt.base.widgets" QT_VERSION ""_dep;
        if (common.getBuildSettings().TargetOS.Type == OSType::Windows && QT_VERSION_NUMBER == 5) {
            common.Public += "org.sw.demo.qtproject.qt.winextras" QT_VERSION ""_dep;
        }
        qt_progs_and_tr(common);
    }

    auto &qt_base = add_lib("qt_base");
    {
        qt_base.Public += base;
        qt_base.Public += "org.sw.demo.qtproject.qt.base.widgets" QT_VERSION ""_dep;
        automoc("org.sw.demo.qtproject.qt.base.tools.moc" QT_VERSION ""_dep, qt_base);
    }

    //
    auto &client_core = add_lib("client");
    {
        client_core += ".*"_rr;
        client_core.Public += common;
        client_core.Public += "org.sw.demo.qtproject.qt.base.printsupport" QT_VERSION ""_dep;
        if (client_core.getBuildSettings().TargetOS.Type == OSType::Windows && QT_VERSION_NUMBER == 5)
            client_core.Public += "org.sw.demo.qtproject.qt.base.plugins.printsupport.windows" QT_VERSION ""_dep;
        else if (client_core.getBuildSettings().TargetOS.Type == OSType::Linux)
            client_core.Public += "org.sw.demo.qtproject.qt.base.plugins.printsupport.cups" QT_VERSION ""_dep;
        qt_progs_and_tr(client_core);
    }

    auto setup_exe = [&](auto &t) -> decltype(auto) {
        t += cppstd;
        if (t.getBuildSettings().TargetOS.Type == OSType::Windows) {
            if (auto L = t.getSelectedTool()->template as<VisualStudioLinker*>(); L)
                L->Subsystem = vs::Subsystem::Windows;
            t += "org.sw.demo.qtproject.qt.base.winmain" QT_VERSION ""_dep;
        }
        if (t.getBuildSettings().TargetOS.Type == OSType::Windows) {
            t.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows" QT_VERSION ""_dep;
            t.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista" QT_VERSION ""_dep;
        }
        if (t.getBuildSettings().TargetOS.Type == OSType::Linux) {
            t.Public += "org.sw.demo.qtproject.qt.wayland.plugins.platforms.qwayland.generic" QT_VERSION ""_dep;
            t.Public += "org.sw.demo.qtproject.qt.wayland.plugins.platforms.qwayland.egl" QT_VERSION ""_dep;
            t.Public += "org.sw.demo.qtproject.qt.wayland.plugins.hardwareintegration.client.wayland_egl" QT_VERSION ""_dep;
            t.Public += "org.sw.demo.qtproject.qt.wayland.plugins.shellintegration.xdg" QT_VERSION ""_dep;
            t.Public += "org.sw.demo.qtproject.qt.wayland.plugins.decorations.bradient" QT_VERSION ""_dep;
        }
        if (t.getBuildSettings().TargetOS.Type == OSType::Macos) {
            t.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.cocoa" QT_VERSION ""_dep;
        }
        return t;
    };
    auto add_exe = [&](auto &base, const String &name) -> decltype(auto) {
        return setup_exe(base.addExecutable(name));
    };

    //
    auto &console = add_exe(aspia, "console");
    setup_target(console, "console");
    console.Public += client_core, qt_base;
    qt_progs_and_tr(console);

    auto &client = add_exe(aspia, "client_exe");
    client += "client/client_entry_point.cc";
    client += "client/client.rc";
    client += client_core, qt_base;

    if (router.getBuildSettings().TargetOS.Type == OSType::Windows) {
        auto &host = aspia.addExecutable("host");
        auto &core = host.addSharedLibrary("core");
        setup_target(core, "host");
        core -= ".*_entry_point.cc"_rr, ".*\\.rc"_rr;
        core += "HOST_IMPLEMENTATION"_def;
        if (core.getBuildSettings().TargetOS.Type == OSType::Windows)
            core.Public += "sas.lib"_slib;
        core.Public += common, qt_base;
        core.Public += "org.sw.demo.boost.property_tree"_dep;
        if (core.getBuildSettings().TargetOS.Type == OSType::Windows) {
            core += "DXGI.lib"_slib;
            core += "d3d11.lib"_slib;
        }
        qt_progs_and_tr2(core);
        if (core.getBuildSettings().TargetOS.Type == OSType::Windows) {
            core.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows" QT_VERSION ""_dep;
            core.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista" QT_VERSION ""_dep;
        }

        setup_exe(host);
        host += "host/ui/host_entry_point.cc";
        host += "host/ui/host.rc";
        host += core;

        auto &service = add_exe(host, "service");
        service += "host/service_entry_point.cc";
        service += "host/service.rc";
        service += core;

        auto &desktop_agent = add_exe(aspia, "desktop_agent");
        desktop_agent += "host/desktop_agent_entry_point.cc";
        desktop_agent += "host/desktop_agent.rc";
        desktop_agent += core;
    }
}
