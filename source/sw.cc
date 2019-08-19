#pragma sw require header org.sw.demo.google.protobuf.protoc-3
#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc-*

void configure(Build &s)
{
    if (s.isConfigSelected("mt"))
    {
        auto ss = s.createSettings();
        ss.Native.LibrariesType = LibraryType::Static;
        ss.Native.MT = true;
        s.addSettings(ss);
    }
}

void build(Solution &s)
{
    auto &aspia = s.addProject("aspia", "master");
    aspia += Git("https://github.com/dchapyshev/aspia", "", "{v}");

    auto setup_target = [&aspia](auto &t, const String &name) -> decltype(auto)
    {
        t.CPPVersion = CPPLanguageStandard::CPP17;
        t.Public += "."_idir;
        t.setRootDirectory(name);
        t += ".*"_rr;
        t.AllowEmptyRegexes = true;
        t -= ".*_unittest.*"_rr;
        t -= ".*_tests.*"_rr;
        t.AllowEmptyRegexes = false;
        return t;
    };

    auto add_lib = [&aspia, &setup_target](const String &name) -> decltype(auto)
    {
        return setup_target(aspia.addStaticLibrary(name), name);
    };

    auto &base = aspia.addStaticLibrary("base");
    base -= "build/.*"_rr;
    setup_target(base, "base");
    base.Public += "UNICODE"_def;
    base.Public += "WIN32_LEAN_AND_MEAN"_def;
    base.Public += "NOMINMAX"_def;
    base.Public += "USE_PCG_GENERATOR"_def;
    base.Public += "org.sw.demo.qtproject.qt.base.widgets-*"_dep;
    base.Public += "org.sw.demo.qtproject.qt.base.network-*"_dep;
    base.Public += "org.sw.demo.qtproject.qt.base.xml-*"_dep;
    base.Public += "org.sw.demo.boost.align"_dep;
    base.Public += "org.sw.demo.imneme.pcg_cpp-master"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, base);

    auto &desktop_capture = add_lib("desktop");
    desktop_capture.Public += "USE_TBB"_def;
    desktop_capture.Public += base;
    desktop_capture.Public += "org.sw.demo.qtproject.qt.base.gui-*"_dep;
    desktop_capture.Public += "org.sw.demo.chromium.libyuv-master"_dep;
    desktop_capture.Public += "org.sw.demo.intel.tbb"_dep;
    desktop_capture.Public += "org.sw.demo.intel.tbb.malloc.proxy"_dep;
    desktop_capture += "dxgi.lib"_slib;

    auto &protocol = aspia.addStaticLibrary("proto");
    protocol += "proto/.*\\.proto"_rr;
    for (const auto &[p, _] : protocol[FileRegex(protocol.SourceDir / "proto", ".*\\.proto", false)])
        gen_protobuf("org.sw.demo.google.protobuf-3"_dep, protocol, p, true, "proto");

    auto &codec = add_lib("codec");
    codec.Public += protocol, desktop_capture;
    codec.Public += "org.sw.demo.facebook.zstd.zstd-*"_dep;
    codec.Public += "org.sw.demo.webmproject.vpx-1"_dep;

    auto &crypto = add_lib("crypto");
    crypto.Public += base;
    crypto.Public += "org.sw.demo.openssl.crypto-*.*.*.*"_dep;

    auto &ipc = add_lib("ipc");
    ipc.Public += base;
    ipc.Public += "org.sw.demo.qtproject.qt.base.network-*"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, ipc);

    auto qt_progs_and_tr = [](auto &t)
    {
        automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, t);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc-*"_dep, t, t.SourceDir / ("resources/" + t.getPackage().ppath.back() + ".qrc"));
        qt_uic("org.sw.demo.qtproject.qt.base.tools.uic-*"_dep, t);

        // trs
        qt_tr("org.sw.demo.qtproject.qt-*"_dep, t);
        t.configureFile(t.SourceDir / ("translations/" + t.getPackage().ppath.back() + "_translations.qrc"),
            t.BinaryDir / (t.getPackage().ppath.back() + "_translations.qrc"), ConfigureFlags::CopyOnly);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc-*"_dep, t,
            t.BinaryDir / (t.getPackage().ppath.back() + "_translations.qrc"))
            .c->working_directory = t.BinaryDir;
    };

    auto &common = add_lib("common");
    if (common.getSettings().TargetOS.Type == OSType::Windows)
        common.Public += "Shlwapi.lib"_lib;
    common.Public += codec, protocol;
    common.Public += "org.sw.demo.openssl.crypto-*.*.*.*"_dep;
    common.Public += "org.sw.demo.qtproject.qt.base.widgets-*"_dep;
    common.Public += "org.sw.demo.qtproject.qt.winextras-*"_dep;
    qt_progs_and_tr(common);
    qt_translations_rcc("org.sw.demo.qtproject.qt-*"_dep, aspia, common, "translations/qt_translations.qrc");

    auto &network = add_lib("net");
    network.Public += crypto, common;
    network.Public += "org.sw.demo.qtproject.qt.base.network-*"_dep;
    if (network.getSettings().TargetOS.Type == OSType::Windows)
        network.Public += "Setupapi.lib"_lib, "Winspool.lib"_lib;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, network);

    auto &updater = add_lib("updater");
    updater.Public += network;
    updater.Public += "org.sw.demo.qtproject.qt.base.widgets-*"_dep;
    qt_progs_and_tr(updater);

    auto &host = aspia.addSharedLibrary("host");
    setup_target(host, "host");
    host -= ".*_entry_point.cc"_rr, ".*\\.rc"_rr;
    host += "host.rc";
    host += "HOST_IMPLEMENTATION"_def;
    if (host.getSettings().TargetOS.Type == OSType::Windows)
        host.Public += "comsuppw.lib"_lib, "sas.lib"_lib;
    host.Public += common, ipc, updater;
    host.Public += "org.sw.demo.boost.property_tree-1"_dep;
    host.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows-*"_dep;
    host.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista-*"_dep;
    qt_progs_and_tr(host);

    auto setup_exe = [](auto &t) -> decltype(auto)
    {
        if (auto L = t.getSelectedTool()->as<VisualStudioLinker>(); L)
            L->Subsystem = vs::Subsystem::Windows;
        t += "org.sw.demo.qtproject.qt.base.winmain-*"_dep;
        return t;
    };

    auto add_exe = [&setup_exe](auto &base, const String &name) -> decltype(auto)
    {
        return setup_exe(base.addExecutable(name));
    };

    auto &host_config = add_exe(host, "config");
    host_config += "host/host_entry_point.cc";
    host_config += "host/host_core.rc";
    host_config += host;

    auto &host_service = add_exe(host, "service");
    host_service += "host/win/host_service_entry_point.cc";
    host_service += "host/win/host_service.rc";
    host_service += host;

    auto &host_session = add_exe(host, "session");
    host_session += "host/win/host_session_entry_point.cc";
    host_session += "host/win/host_session.rc";
    host_session += host;

    //
    auto &client = add_lib("client");
    client.Public += common, updater;
    if (client.getSettings().TargetOS.Type == OSType::Windows)
        client.Public += "org.sw.demo.qtproject.qt.base.plugins.printsupport.windows-*"_dep;
    qt_progs_and_tr(client);

    //
    auto &console = add_exe(aspia, "console");
    setup_target(console, "console");
    console.Public += client;
    if (console.getSettings().TargetOS.Type == OSType::Windows)
    {
        console.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows-*"_dep;
        console.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista-*"_dep;
    }
    qt_progs_and_tr(console);
}
