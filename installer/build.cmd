@echo off

set SDK_VERSION=10.0.18362.0

set ASPIA_VERSION=%1
set ASPIA_ARCH=%2
set ASPIA_SRC_DIR=%3
set ASPIA_BIN_DIR=%4

if "%ASPIA_VERSION%" == "" ( goto :USAGE )
if "%ASPIA_ARCH%" == "" ( goto :USAGE )
if "%ASPIA_SRC_DIR%" == "" ( goto :USAGE )
if "%ASPIA_BIN_DIR%" == "" ( goto :USAGE )

set SRC_DIR=%ASPIA_SRC_DIR%\installer
set EN_US_POSTFIX=%ASPIA_VERSION%-%ASPIA_ARCH%

if "%ASPIA_ARCH%" == "x86_64" ( set CANDLE_ARCH=x64 )
if "%ASPIA_ARCH%" == "x64" ( set CANDLE_ARCH=x64 )
if "%ASPIA_ARCH%" == "x86" ( set CANDLE_ARCH=x86 )

echo "##################################################"
echo "Aspia Src Dir: %ASPIA_SRC_DIR%"
echo "Aspia Bin Dir: %ASPIA_BIN_DIR%"
echo "Aspia Version: %ASPIA_VERSION%"
echo "Aspia Arch: %ASPIA_ARCH%"
echo "Candle Arch: %CANDLE_ARCH%"
echo "Installer Src Dir: %SRC_DIR%"

goto :MSI

:USAGE
echo "Usage: build.cmd [version] [arch] [aspia_src_dir] [aspia_bin_dir]"
echo "[version] examples: 2.6.0, 2.6.1, etc"
echo "[arch] examples: x86, x86_64, arm64, etc"
echo "[aspia_src_dir] examples: D:\aspia"
echo "[aspia_bin_dir] examples: D:\aspia_bin\Release"
goto :END

:MSI
rem Set working directory
pushd %SRC_DIR%

echo "##################################################"
echo "Creating MSI packages for Aspia Console"
set CONSOLE_EN_US_MSI=%ASPIA_BIN_DIR%\aspia-console-%EN_US_POSTFIX%.msi
"%WIX%\bin\candle" -out "%ASPIA_BIN_DIR%\\" -arch %CANDLE_ARCH% -ext WixUtilExtension -ext WixUIExtension console.wxs
"%WIX%\bin\light" -sval -out "%CONSOLE_EN_US_MSI%" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\console.en-us.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\console.de-de.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-it-it.msi" -cultures:it-it -ext WixUtilExtension -ext WixUIExtension -loc translations\console.it-it.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\console.nl-nl.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\console.pt-br.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\console.ru-ru.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\console.uk-ua.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\console.zh-cn.wxl "%ASPIA_BIN_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-console-zh-tw.msi" -cultures:zh-tw -ext WixUtilExtension -ext WixUIExtension -loc translations\console.zh-tw.wxl "%ASPIA_BIN_DIR%\console.wixobj"

echo "##################################################"
echo "Creating MSI transforms for Aspia Console"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-de-de.msi" -out "%ASPIA_BIN_DIR%\console-de-de.mst"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-it-it.msi" -out "%ASPIA_BIN_DIR%\console-it-it.mst"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-nl-nl.msi" -out "%ASPIA_BIN_DIR%\console-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-pt-br.msi" -out "%ASPIA_BIN_DIR%\console-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-ru-ru.msi" -out "%ASPIA_BIN_DIR%\console-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-uk-ua.msi" -out "%ASPIA_BIN_DIR%\console-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-zh-cn.msi" -out "%ASPIA_BIN_DIR%\console-zh-cn.mst"
"%WIX%\bin\torch" -p -t language "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-console-zh-tw.msi" -out "%ASPIA_BIN_DIR%\console-zh-tw.mst"

echo "##################################################"
echo "Integration of transforms for Aspia Console"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-it-it.mst" 1040
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-zh-cn.mst" 2052
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CONSOLE_EN_US_MSI%" "%ASPIA_BIN_DIR%\console-zh-tw.mst" 1028

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%CONSOLE_EN_US_MSI%" Package 1033,1031,1040,1043,1046,1049,1058,2052,1028,0

echo "##################################################"
echo "Creating MSI packages for Aspia Client"
set CLIENT_EN_US_MSI=%ASPIA_BIN_DIR%\aspia-client-%EN_US_POSTFIX%.msi
"%WIX%\bin\candle" -out "%ASPIA_BIN_DIR%\\" -arch %CANDLE_ARCH% -ext WixUtilExtension -ext WixUIExtension client.wxs
"%WIX%\bin\light" -sval -out "%CLIENT_EN_US_MSI%" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\client.en-us.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\client.de-de.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-it-it.msi" -cultures:it-it -ext WixUtilExtension -ext WixUIExtension -loc translations\client.it-it.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\client.nl-nl.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\client.pt-br.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\client.ru-ru.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\client.uk-ua.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\client.zh-cn.wxl "%ASPIA_BIN_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-client-zh-tw.msi" -cultures:zh-tw -ext WixUtilExtension -ext WixUIExtension -loc translations\client.zh-tw.wxl "%ASPIA_BIN_DIR%\client.wixobj"

echo "##################################################"
echo "Creating MSI transforms for Aspia Client"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-de-de.msi" -out "%ASPIA_BIN_DIR%\client-de-de.mst"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-it-it.msi" -out "%ASPIA_BIN_DIR%\client-it-it.mst"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-nl-nl.msi" -out "%ASPIA_BIN_DIR%\client-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-pt-br.msi" -out "%ASPIA_BIN_DIR%\client-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-ru-ru.msi" -out "%ASPIA_BIN_DIR%\client-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-uk-ua.msi" -out "%ASPIA_BIN_DIR%\client-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-zh-cn.msi" -out "%ASPIA_BIN_DIR%\client-zh-cn.mst"
"%WIX%\bin\torch" -p -t language "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-client-zh-tw.msi" -out "%ASPIA_BIN_DIR%\client-zh-tw.mst"

echo "##################################################"
echo "Integration of transforms for Aspia Client"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-it-it.mst" 1040
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-zh-cn.mst" 2052
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%CLIENT_EN_US_MSI%" "%ASPIA_BIN_DIR%\client-zh-tw.mst" 1028

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%CLIENT_EN_US_MSI%" Package 1033,1031,1040,1043,1046,1049,1058,2052,1028,0

echo "##################################################"
echo "Creating MSI packages for Aspia Host"
set HOST_EN_US_MSI=%ASPIA_BIN_DIR%\aspia-host-%EN_US_POSTFIX%.msi
"%WIX%\bin\candle" -out "%ASPIA_BIN_DIR%\\" -arch %CANDLE_ARCH% -ext WixUtilExtension -ext WixUIExtension host.wxs
"%WIX%\bin\light" -sval -out "%HOST_EN_US_MSI%" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\host.en-us.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\host.de-de.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-it-it.msi" -cultures:it-it -ext WixUtilExtension -ext WixUIExtension -loc translations\host.it-it.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\host.nl-nl.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\host.pt-br.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\host.ru-ru.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\host.uk-ua.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\host.zh-cn.wxl "%ASPIA_BIN_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-host-zh-tw.msi" -cultures:zh-tw -ext WixUtilExtension -ext WixUIExtension -loc translations\host.zh-tw.wxl "%ASPIA_BIN_DIR%\host.wixobj"

echo "##################################################"
echo "Creating MSI transforms for Aspia Host"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-de-de.msi" -out "%ASPIA_BIN_DIR%\host-de-de.mst"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-it-it.msi" -out "%ASPIA_BIN_DIR%\host-it-it.mst"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-nl-nl.msi" -out "%ASPIA_BIN_DIR%\host-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-pt-br.msi" -out "%ASPIA_BIN_DIR%\host-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-ru-ru.msi" -out "%ASPIA_BIN_DIR%\host-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-uk-ua.msi" -out "%ASPIA_BIN_DIR%\host-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-zh-cn.msi" -out "%ASPIA_BIN_DIR%\host-zh-cn.mst"
"%WIX%\bin\torch" -p -t language "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-host-zh-tw.msi" -out "%ASPIA_BIN_DIR%\host-zh-tw.mst"

echo "##################################################"
echo "Integration of transforms for Aspia Host"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-it-it.mst" 1040
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-zh-cn.mst" 2052
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%HOST_EN_US_MSI%" "%ASPIA_BIN_DIR%\host-zh-tw.mst" 1028

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%HOST_EN_US_MSI%" Package 1033,1031,1040,1043,1046,1049,1058,2052,1028,0

echo "##################################################"
echo "Creating MSI packages for Aspia Router"
"%WIX%\bin\candle" -out "%ASPIA_BIN_DIR%\\" -arch %CANDLE_ARCH% -ext WixUtilExtension -ext WixUIExtension router.wxs
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-router-%ASPIA_VERSION%-%ASPIA_ARCH%.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\router.en-us.wxl "%ASPIA_BIN_DIR%\router.wixobj"

echo "##################################################"
echo "Creating MSI packages for Aspia Relay"
"%WIX%\bin\candle" -out "%ASPIA_BIN_DIR%\\" -arch %CANDLE_ARCH% -ext WixUtilExtension -ext WixUIExtension relay.wxs
"%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-relay-%ASPIA_VERSION%-%ASPIA_ARCH%.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\relay.en-us.wxl "%ASPIA_BIN_DIR%\relay.wixobj"

echo "##################################################"
echo "Calculate SHA256 for binaries"
%ASPIA_BIN_DIR%\aspia_sha256.exe > %ASPIA_BIN_DIR%\windows-%ASPIA_ARCH%-sha256.txt

rem Restore working directory
popd

:END
