#pragma sw require header org.sw.demo.google.protobuf.protoc-3
#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc-*
#pragma sw require header org.sw.demo.qtproject.qt.tools.linguist.release-*

void build(Solution &s)
{
    auto &aspia = s.addProject("aspia", "master");
    aspia += Git("https://github.com/dchapyshev/aspia", "", "{v}");

    auto setup_target = [&aspia](auto &t, const String &name) -> decltype(auto)
    {
        t.CPPVersion = CPPLanguageStandard::CPP17;
        t += "source"_idir;
        t.setRootDirectory("source/" + name);
        t += ".*"_rr;
        t -= ".*_unittest.*"_rr;
        return t;
    };

    auto add_lib = [&aspia, &setup_target](const String &name) -> decltype(auto)
    {
        return setup_target(aspia.addStaticLibrary(name), name);
    };

    auto &base = add_lib("base");
    base.Public += "UNICODE"_def;
    base.Public += "org.sw.demo.qtproject.qt.base.core-*"_dep;
    base.Public += "org.sw.demo.boost.align-1"_dep;

    auto &desktop_capture = add_lib("desktop_capture");
    desktop_capture.Public += base;
    desktop_capture.Public += "org.sw.demo.qtproject.qt.base.gui-*"_dep;
    desktop_capture.Public += "org.sw.demo.chromium.libyuv-master"_dep;

    auto &protocol = aspia.addStaticLibrary("protocol");
    protocol += "source/protocol/.*\\.proto"_rr;
    for (const auto &[p, _] : protocol[FileRegex(protocol.SourceDir / "source/protocol", std::regex(".*\\.proto"))])
        gen_protobuf(protocol, p, true, "protocol");

    auto &codec = add_lib("codec");
    codec.Public += base, protocol;
    codec.Public += "org.sw.demo.facebook.zstd.zstd-*"_dep;
    codec.Public += "org.sw.demo.chromium.libyuv-master"_dep;
    codec.Public += "org.sw.demo.webmproject.vpx-1"_dep;

    auto &crypto = add_lib("crypto");
    crypto.Public += base;
    crypto.Public += "org.sw.demo.openssl.crypto-*.*.*.*"_dep;

    auto &ipc = add_lib("ipc");
    ipc.Public += base;
    ipc.Public += "org.sw.demo.qtproject.qt.base.network-*"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, ipc);

    auto &network = add_lib("network");
    network.Public += crypto, protocol;
    network.Public += "org.sw.demo.qtproject.qt.base.network-*"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, network);

    auto &updater = add_lib("updater");
    updater.Public += network;
    updater.Public += "org.sw.demo.qtproject.qt.base.widgets-*"_dep;
    updater.Public += "org.sw.demo.qtproject.qt.base.xml-*"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, updater);
    rcc("org.sw.demo.qtproject.qt.base.tools.rcc-*"_dep, updater, updater.SourceDir / "resources/updater.qrc");
    for (const auto &[p, _] : updater[".*\\.ui"_r])
        uic("org.sw.demo.qtproject.qt.base.tools.uic-*"_dep, updater, p);
    qt_create_translation("org.sw.demo.qtproject.qt.tools.linguist.update-*"_dep, "org.sw.demo.qtproject.qt.tools.linguist.release-*"_dep, updater);

    auto &common = add_lib("common");
    if (s.Settings.TargetOS.Type == OSType::Windows)
        common.Public += "Shlwapi.lib"_lib;
    common.Public += codec, protocol;
    common.Public += "org.sw.demo.qtproject.qt.base.gui-*"_dep;
    common.Public += "org.sw.demo.qtproject.qt.winextras-*"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, common);

    auto &host = aspia.addSharedLibrary("host");
    setup_target(host, "host");
    host -= ".*_entry_point.cc"_rr;
    host += "HOST_IMPLEMENTATION"_def;
    if (s.Settings.TargetOS.Type == OSType::Windows)
        host.Public += "comsuppw.lib"_lib, "sas.lib"_lib;
    host.Public += common, desktop_capture, ipc, updater;
    host.Public += "org.sw.demo.boost.property_tree-1"_dep;
    host.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows-*"_dep;
    host.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista-*"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, host);
    rcc("org.sw.demo.qtproject.qt.base.tools.rcc-*"_dep, host, host.SourceDir / "resources/host.qrc");
    for (const auto &[p, _] : host[".*\\.ui"_r])
        uic("org.sw.demo.qtproject.qt.base.tools.uic-*"_dep, host, p);
    qt_create_translation("org.sw.demo.qtproject.qt.tools.linguist.update-*"_dep, "org.sw.demo.qtproject.qt.tools.linguist.release-*"_dep, host);

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
    host_config += "host_config_entry_point.cc";
    host_config += host;

    auto &host_service = add_exe(host, "service");
    host_service += "win/host_service_entry_point.cc";
    host_service += host;

    auto &host_session = add_exe(host, "session");
    host_session += "win/host_session_entry_point.cc";
    host_session += host;

    //
    auto &client = add_lib("client");
    client.Public += common, desktop_capture, updater;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, client);
    rcc("org.sw.demo.qtproject.qt.base.tools.rcc-*"_dep, client, client.SourceDir / "resources/client.qrc");
    for (const auto &[p, _] : client[".*\\.ui"_r])
        uic("org.sw.demo.qtproject.qt.base.tools.uic-*"_dep, client, p);
    qt_create_translation("org.sw.demo.qtproject.qt.tools.linguist.update-*"_dep, "org.sw.demo.qtproject.qt.tools.linguist.release-*"_dep, client);

    //
    auto &console = add_exe(aspia, "console");
    setup_target(console, "console");
    console.Public += client;
    console.Public += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows-*"_dep;
    console.Public += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista-*"_dep;
    automoc("org.sw.demo.qtproject.qt.base.tools.moc-*"_dep, console);
    rcc("org.sw.demo.qtproject.qt.base.tools.rcc-*"_dep, console, console.SourceDir / "resources/console.qrc");
    for (const auto &[p, _] : console[".*\\.ui"_r])
        uic("org.sw.demo.qtproject.qt.base.tools.uic-*"_dep, console, p);
    qt_create_translation("org.sw.demo.qtproject.qt.tools.linguist.update-*"_dep, "org.sw.demo.qtproject.qt.tools.linguist.release-*"_dep, console);
}
