#pragma sw require header org.sw.demo.google.protobuf.protoc
#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc

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

    auto setup_target = [&aspia](auto &t, const String &name, bool add_tests = false) -> decltype(auto)
    {
        t += cpp17;
        t.Public += "."_idir;
        t.setRootDirectory(name);
        t += IncludeDirectory("."s);
        t += ".*"_rr;

        //
        t.AllowEmptyRegexes = true;

        // os specific
        t -= ".*_win.*"_rr;
        t -= ".*_linux.*"_rr;
        t -= ".*_mac.*"_rr;
        t -= ".*_posix.*"_rr;
        t -= ".*_x11.*"_rr;
        if (t.getBuildSettings().TargetOS.Type == OSType::Windows)
            t += ".*_win.*"_rr;
        else if (t.getBuildSettings().TargetOS.isApple())
            t += ".*_mac.*"_rr;
        else
        {
            t += ".*_linux.*"_rr;
            t += ".*_x11.*"_rr;
        }
        if (t.getBuildSettings().TargetOS.Type != OSType::Windows)
            t += ".*_posix.*"_rr;

        t -= ".*_unittest.*"_rr;
        t -= ".*tests.*"_rr;

        //
        t.AllowEmptyRegexes = false;

        // test
        if (add_tests)
        {
            auto &bt = t.addExecutable("test");
            bt += cpp17;
            bt += FileRegex(name, ".*_unittest.*", true);
            bt += t;
            bt += "org.sw.demo.google.googletest.gmock"_dep;
            bt += "org.sw.demo.google.googletest.gtest.main"_dep;
            t.addTest(bt);
        }

        return t;
    };

    auto add_lib = [&aspia, &setup_target](const String &name, bool add_tests = false) -> decltype(auto)
    {
        return setup_target(aspia.addStaticLibrary(name), name, add_tests);
    };

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

    auto &base = aspia.addStaticLibrary("base");
    base += "third_party/modp_b64/.*\\.[hc]"_rr;
    base += "third_party/x11region/.*\\.[hc]"_rr;
    base -= "build/.*"_rr;
    setup_target(base, "base", false);
    base.Public += "UNICODE"_def;
    base.Public += "WIN32_LEAN_AND_MEAN"_def;
    base.Public += "NOMINMAX"_def;
    base.Public += protocol;
    base.Public += "org.sw.demo.qtproject.qt.base.widgets"_dep;
    base.Public += "org.sw.demo.qtproject.qt.base.network"_dep;
    base.Public += "org.sw.demo.qtproject.qt.base.xml"_dep;
    base.Public += "org.sw.demo.boost.align"_dep;
    base.Public += "org.sw.demo.imneme.pcg_cpp-master"_dep;
    base.Public += "org.sw.demo.chriskohlhoff.asio"_dep;
    base.Public += "org.sw.demo.rapidxml"_dep;
    base.Public += "org.sw.demo.miloyip.rapidjson"_dep;
    base.Public += "org.sw.demo.google.protobuf.protobuf_lite"_dep;
    base.Public += "org.sw.demo.chromium.libyuv-master"_dep;
    base.Public += "org.sw.demo.webmproject.vpx"_dep;
    if (base.getBuildSettings().TargetOS.Type == OSType::Windows)
    {
        base -= "x11/.*"_rr;
        base.Public += "com.Microsoft.Windows.SDK.winrt"_dep;
        base += "comsuppw.lib"_slib, "Winspool.lib"_slib;
    }
    automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, base);

    auto &relay = aspia.addExecutable("relay");
    relay += cpp17;
    relay += "relay/.*"_rr;
    relay += base;

    auto &router = aspia.addExecutable("router");
    router += cpp17;
    router += "router/.*"_rr;
    router -= "router/keygen.*"_rr;
    router -= "router/manager.*"_rr;
    router += base;
    router += "org.sw.demo.sqlite3"_dep;

    auto qt_progs = [](auto &t, const String &name_override = {}, const path &path_override = {})
    {
        auto name = name_override.empty() ? t.getPackage().getPath().back() : name_override;

        automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, t);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc"_dep, t, t.SourceDir / path_override / ("resources/" + name + ".qrc"));
        qt_uic("org.sw.demo.qtproject.qt.base.tools.uic"_dep, t);
    };

    auto qt_progs2 = [](auto &t)
    {
        automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, t);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc"_dep, t, t.SourceDir / "ui/resources.qrc");
        qt_uic("org.sw.demo.qtproject.qt.base.tools.uic"_dep, t);
    };

    auto qt_progs_and_tr = [&qt_progs](auto &t, const String &name_override = {}, const path &path_override = {})
    {
        auto name = name_override.empty() ? t.getPackage().getPath().back() : name_override;

        qt_progs(t, name_override, path_override);

        // trs
        qt_tr("org.sw.demo.qtproject.qt"_dep, t);
        t.configureFile(t.SourceDir / path_override / ("translations/" + name + "_translations.qrc"),
            t.BinaryDir / (name + "_translations.qrc"), ConfigureFlags::CopyOnly);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc"_dep, t,
            t.BinaryDir / (name + "_translations.qrc"))
            ->working_directory = t.BinaryDir;
    };

    auto qt_progs_and_tr2 = [&qt_progs2](auto &t)
    {
        qt_progs2(t);

        // trs
        qt_tr("org.sw.demo.qtproject.qt"_dep, t);
        t.configureFile(t.SourceDir / "ui/translations.qrc",
            t.BinaryDir / "translations.qrc", ConfigureFlags::CopyOnly);
        rcc("org.sw.demo.qtproject.qt.base.tools.rcc"_dep, t,
            t.BinaryDir / "translations.qrc")
            ->working_directory = t.BinaryDir;
    };

    auto &common = add_lib("common");
    if (common.getBuildSettings().TargetOS.Type == OSType::Windows)
    {
        common -= "file_enumerator_fs.cc";
        common.Public += "Shlwapi.lib"_slib;
    }
    common.Public += base, protocol;
    common.Public += "org.sw.demo.openssl.crypto"_dep;
    common.Public += "org.sw.demo.qtproject.qt.base.widgets"_dep;
    common.Public += "org.sw.demo.qtproject.qt.winextras"_dep;
    qt_progs_and_tr(common);

    auto &qt_base = add_lib("qt_base");
    qt_base.Public += base;
    qt_base.Public += "org.sw.demo.qtproject.qt.base.widgets"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc"_dep, qt_base);
    qt_translations_rcc("org.sw.demo.qtproject.qt"_dep, aspia, qt_base, "qt_translations.qrc");

    auto setup_exe = [](auto &t) -> decltype(auto)
    {
        if (auto L = t.getSelectedTool()->as<VisualStudioLinker*>(); L)
            L->Subsystem = vs::Subsystem::Windows;
        t += "org.sw.demo.qtproject.qt.base.winmain"_dep;
        return t;
    };

    auto &keygen = router.addExecutable("keygen");
    setup_exe(keygen);
    keygen += cpp17;
    keygen.setRootDirectory("router/keygen");
    keygen += ".*"_rr;
    keygen += qt_base;
    keygen.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows"_dep;
    keygen.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista"_dep;
    qt_progs(keygen);

    //
    auto &manager = router.addExecutable("manager");
    setup_exe(manager);
    manager += cpp17;
    manager.setRootDirectory("router/manager");
    manager += ".*"_rr;
    manager += qt_base;
    manager.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows"_dep;
    manager.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista"_dep;
    qt_progs_and_tr(manager, "router_manager");

    //
    auto &client = add_lib("client");
    client.Public += common;
    if (client.getBuildSettings().TargetOS.Type == OSType::Windows)
        client.Public += "org.sw.demo.qtproject.qt.base.plugins.printsupport.windows"_dep;
    qt_progs_and_tr(client);

    auto add_exe = [&setup_exe](auto &base, const String &name) -> decltype(auto)
    {
        return setup_exe(base.addExecutable(name));
    };

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

    auto &host = aspia.addExecutable("host");
    auto &core = host.addSharedLibrary("core");
    setup_target(core, "host");
    core -= ".*_entry_point.cc"_rr, ".*\\.rc"_rr;
    core += "HOST_IMPLEMENTATION"_def;
    if (core.getBuildSettings().TargetOS.Type == OSType::Windows)
        core.Public += "sas.lib"_slib;
    core.Public += common, qt_base;
    core.Public += "org.sw.demo.boost.property_tree"_dep;
    core.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows"_dep;
    core.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista"_dep;
    qt_progs_and_tr2(core);

    setup_exe(host);
    host += "host/host_entry_point.cc";
    host += "host/host.rc";
    host += core;

    auto &service = add_exe(host, "service");
    service += "host/win/service_entry_point.cc";
    service += "host/win/service.rc";
    service += core;

    auto &desktop_agent = add_exe(aspia, "desktop_agent");
    desktop_agent += "host/desktop_agent_entry_point.cc";
    desktop_agent += "host/desktop_agent.rc";
    desktop_agent += core;
}
