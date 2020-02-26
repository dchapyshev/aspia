#pragma sw require header org.sw.demo.google.protobuf.protoc-3
#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc-*

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
    base.Public += "USE_TBB"_def;
    base.Public += "org.sw.demo.qtproject.qt.base.widgets"_dep;
    base.Public += "org.sw.demo.qtproject.qt.base.network"_dep;
    base.Public += "org.sw.demo.qtproject.qt.base.xml"_dep;
    base.Public += "org.sw.demo.boost.align"_dep;
    base.Public += "org.sw.demo.imneme.pcg_cpp-master"_dep;
    base.Public += "org.sw.demo.chriskohlhoff.asio"_dep;
    base.Public += "org.sw.demo.rapidxml"_dep;
    base.Public += "org.sw.demo.intel.tbb"_dep;
    base.Public += "org.sw.demo.intel.tbb.malloc.proxy"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, base);

    auto &desktop_capture = add_lib("desktop");
    desktop_capture.Public += base;
    desktop_capture.Public += "org.sw.demo.qtproject.qt.base.gui"_dep;
    desktop_capture.Public += "org.sw.demo.chromium.libyuv-master"_dep;
    desktop_capture.Public += "com.Microsoft.Windows.SDK.winrt"_dep;
    desktop_capture += "dxgi.lib"_slib;

    auto &protocol = aspia.addStaticLibrary("proto");
    protocol += "proto/.*\\.proto"_rr;
    for (const auto &[p, _] : protocol[FileRegex(protocol.SourceDir / "proto", ".*\\.proto", false)])
    {
        ProtobufData d;
        d.outdir = protocol.BinaryDir / "proto";
        d.public_protobuf = true;
        d.addIncludeDirectory(protocol.SourceDir / "proto");
        gen_protobuf_cpp("org.sw.demo.google.protobuf"_dep, protocol, p, d);
    }

    auto &codec = add_lib("codec");
    codec.Public += protocol, desktop_capture;
    codec.Public += "org.sw.demo.facebook.zstd.zstd"_dep;
    codec.Public += "org.sw.demo.webmproject.vpx"_dep;

    auto &crypto = add_lib("crypto");
    crypto.Public += base;
    crypto.Public += "org.sw.demo.openssl.crypto"_dep;

    auto &ipc = add_lib("ipc");
    ipc.Public += base;
    ipc.Public += "org.sw.demo.qtproject.qt.base.network"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, ipc);

    auto qt_progs_and_tr = [](auto &t, const String &name_override = {})
    {
        auto name = name_override.empty() ? t.getPackage().getPath().back() : name_override;

        automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, t);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc"_dep, t, t.SourceDir / ("resources/" + name + ".qrc"));
        qt_uic("org.sw.demo.qtproject.qt.base.tools.uic"_dep, t);

        // trs
        qt_tr("org.sw.demo.qtproject.qt"_dep, t);
        t.configureFile(t.SourceDir / ("translations/" + name + "_translations.qrc"),
            t.BinaryDir / (name + "_translations.qrc"), ConfigureFlags::CopyOnly);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc"_dep, t,
            t.BinaryDir / (name + "_translations.qrc"))
            .c->working_directory = t.BinaryDir;
    };

    auto &common = add_lib("common");
    if (common.getBuildSettings().TargetOS.Type == OSType::Windows)
        common.Public += "Shlwapi.lib"_slib;
    common.Public += codec, protocol;
    common.Public += "org.sw.demo.openssl.crypto"_dep;
    common.Public += "org.sw.demo.qtproject.qt.base.widgets"_dep;
    common.Public += "org.sw.demo.qtproject.qt.winextras"_dep;
    qt_progs_and_tr(common);
    qt_translations_rcc("org.sw.demo.qtproject.qt"_dep, aspia, common, "translations/qt_translations.qrc");

    auto &network = add_lib("net");
    network.Public += crypto, common;
    network.Public += "org.sw.demo.qtproject.qt.base.network"_dep;
    if (network.getBuildSettings().TargetOS.Type == OSType::Windows)
        network.Public += "Setupapi.lib"_slib, "Winspool.lib"_slib;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, network);

    auto &updater = add_lib("updater");
    updater.Public += network;
    updater.Public += "org.sw.demo.qtproject.qt.base.widgets"_dep;
    qt_progs_and_tr(updater);

    auto &qt_base = add_lib("qt_base");
    qt_base.Public += base;
    qt_base.Public += "org.sw.demo.qtproject.qt.base.widgets"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, qt_base);

    auto &host = aspia.addExecutable("host");
    auto &core = host.addSharedLibrary("core");
    setup_target(core, "host");
    core -= ".*_entry_point.cc"_rr, ".*\\.rc"_rr;
    core += "HOST_IMPLEMENTATION"_def;
    if (core.getBuildSettings().TargetOS.Type == OSType::Windows)
        core.Public += "comsuppw.lib"_slib, "sas.lib"_slib;
    core.Public += ipc, qt_base, updater;
    core.Public += "org.sw.demo.boost.property_tree"_dep;
    core.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows"_dep;
    core.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista"_dep;
    qt_progs_and_tr(core, "host");

    auto setup_exe = [](auto &t) -> decltype(auto)
    {
        if (auto L = t.getSelectedTool()->as<VisualStudioLinker*>(); L)
            L->Subsystem = vs::Subsystem::Windows;
        t += "org.sw.demo.qtproject.qt.base.winmain"_dep;
        return t;
    };

    auto add_exe = [&setup_exe](auto &base, const String &name) -> decltype(auto)
    {
        return setup_exe(base.addExecutable(name));
    };

    setup_exe(host);
    host += "host/host_entry_point.cc";
    host += "host/host.rc";
    host += core;

    auto &service = add_exe(host, "service");
    service += "host/win/host_service_entry_point.cc";
    service += "host/win/host_service.rc";
    service += core;

    auto &desktop_agent = add_exe(aspia, "desktop_agent");
    desktop_agent += "host/desktop_agent_entry_point.cc";
    desktop_agent += "host/desktop_agent.rc";
    desktop_agent += core;

    //
    auto &client = add_lib("client");
    client.Public += common, updater;
    if (client.getBuildSettings().TargetOS.Type == OSType::Windows)
        client.Public += "org.sw.demo.qtproject.qt.base.plugins.printsupport.windows"_dep;
    qt_progs_and_tr(client);

    //
    auto &console = add_exe(aspia, "console");
    setup_target(console, "console");
    console.Public += client, qt_base;
    if (console.getBuildSettings().TargetOS.Type == OSType::Windows)
    {
        console.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows"_dep;
        console.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista"_dep;
    }
    qt_progs_and_tr(console);
}
