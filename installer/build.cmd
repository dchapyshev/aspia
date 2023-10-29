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

set TMP_DIR=%ASPIA_BIN_DIR%\temp
set SRC_DIR=%ASPIA_SRC_DIR%\installer

if not exist "%TMP_DIR%" (
    echo "Output directory does not exist. Create it."
    mkdir %TMP_DIR%
)

echo "##################################################"
echo "Aspia Src Dir: %ASPIA_SRC_DIR%"
echo "Aspia Bin Dir: %ASPIA_BIN_DIR%"
echo "Aspia Version: %ASPIA_VERSION%"
echo "Aspia Arch: %ASPIA_ARCH%"
echo "Installer Src Dir: %SRC_DIR%"
echo "Installer Build Dir: %TMP_DIR%"

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
"%WIX%\bin\candle" -out "%TMP_DIR%\\" -ext WixUtilExtension -ext WixUIExtension console.wxs
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\console.de-de.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\console.en-us.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-it-it.msi" -cultures:it-it -ext WixUtilExtension -ext WixUIExtension -loc translations\console.it-it.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\console.nl-nl.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\console.pt-br.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\console.ru-ru.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\console.uk-ua.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\console.zh-cn.wxl "%TMP_DIR%\console.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-console-zh-tw.msi" -cultures:zh-tw -ext WixUtilExtension -ext WixUIExtension -loc translations\console.zh-tw.wxl "%TMP_DIR%\console.wixobj"

echo "##################################################"
echo "Creating MSI transforms for Aspia Console"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-de-de.msi" -out "%TMP_DIR%\console-de-de.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-it-it.msi" -out "%TMP_DIR%\console-it-it.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-nl-nl.msi" -out "%TMP_DIR%\console-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-pt-br.msi" -out "%TMP_DIR%\console-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-ru-ru.msi" -out "%TMP_DIR%\console-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-uk-ua.msi" -out "%TMP_DIR%\console-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-zh-cn.msi" -out "%TMP_DIR%\console-zh-cn.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\aspia-console-zh-tw.msi" -out "%TMP_DIR%\console-zh-tw.mst"

echo "##################################################"
echo "Integration of transforms for Aspia Console"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-it-it.mst" 1040
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-zh-cn.mst" 2052
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-console-en-us.msi" "%TMP_DIR%\console-zh-tw.mst" 1028

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%TMP_DIR%\aspia-console-en-us.msi" Package 1033,1031,1040,1043,1046,1049,1058,2052,1028,0
xcopy %TMP_DIR%\aspia-console-en-us.msi %ASPIA_BIN_DIR%\
ren %ASPIA_BIN_DIR%\aspia-console-en-us.msi aspia-console-%ASPIA_VERSION%-%ASPIA_ARCH%.msi

echo "##################################################"
echo "Creating MSI packages for Aspia Client"
"%WIX%\bin\candle" -out "%TMP_DIR%\\" -ext WixUtilExtension -ext WixUIExtension client.wxs
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\client.de-de.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\client.en-us.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-it-it.msi" -cultures:it-it -ext WixUtilExtension -ext WixUIExtension -loc translations\client.it-it.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\client.nl-nl.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\client.pt-br.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\client.ru-ru.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\client.uk-ua.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\client.zh-cn.wxl "%TMP_DIR%\client.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-client-zh-tw.msi" -cultures:zh-tw -ext WixUtilExtension -ext WixUIExtension -loc translations\client.zh-tw.wxl "%TMP_DIR%\client.wixobj"

echo "##################################################"
echo "Creating MSI transforms for Aspia Client"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-de-de.msi" -out "%TMP_DIR%\client-de-de.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-it-it.msi" -out "%TMP_DIR%\client-it-it.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-nl-nl.msi" -out "%TMP_DIR%\client-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-pt-br.msi" -out "%TMP_DIR%\client-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-ru-ru.msi" -out "%TMP_DIR%\client-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-uk-ua.msi" -out "%TMP_DIR%\client-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-zh-cn.msi" -out "%TMP_DIR%\client-zh-cn.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\aspia-client-zh-tw.msi" -out "%TMP_DIR%\client-zh-tw.mst"

echo "##################################################"
echo "Integration of transforms for Aspia Client"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-it-it.mst" 1040
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-zh-cn.mst" 2052
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-client-en-us.msi" "%TMP_DIR%\client-zh-tw.mst" 1028

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%TMP_DIR%\aspia-client-en-us.msi" Package 1033,1031,1040,1043,1046,1049,1058,2052,1028,0
xcopy %TMP_DIR%\aspia-client-en-us.msi %ASPIA_BIN_DIR%\
ren %ASPIA_BIN_DIR%\aspia-client-en-us.msi aspia-client-%ASPIA_VERSION%-%ASPIA_ARCH%.msi

echo "##################################################"
echo "Creating MSI packages for Aspia Host"
"%WIX%\bin\candle" -out "%TMP_DIR%\\" -ext WixUtilExtension -ext WixUIExtension host.wxs
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\host.de-de.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\host.en-us.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-it-it.msi" -cultures:it-it -ext WixUtilExtension -ext WixUIExtension -loc translations\host.it-it.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\host.nl-nl.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\host.pt-br.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\host.ru-ru.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\host.uk-ua.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\host.zh-cn.wxl "%TMP_DIR%\host.wixobj"
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-host-zh-tw.msi" -cultures:zh-tw -ext WixUtilExtension -ext WixUIExtension -loc translations\host.zh-tw.wxl "%TMP_DIR%\host.wixobj"

echo "##################################################"
echo "Creating MSI transforms for Aspia Host"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-de-de.msi" -out "%TMP_DIR%\host-de-de.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-it-it.msi" -out "%TMP_DIR%\host-it-it.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-nl-nl.msi" -out "%TMP_DIR%\host-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-pt-br.msi" -out "%TMP_DIR%\host-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-ru-ru.msi" -out "%TMP_DIR%\host-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-uk-ua.msi" -out "%TMP_DIR%\host-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-zh-cn.msi" -out "%TMP_DIR%\host-zh-cn.mst"
"%WIX%\bin\torch" -p -t language "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\aspia-host-zh-tw.msi" -out "%TMP_DIR%\host-zh-tw.mst"

echo "##################################################"
echo "Integration of transforms for Aspia Host"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-it-it.mst" 1040
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-zh-cn.mst" 2052
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%TMP_DIR%\aspia-host-en-us.msi" "%TMP_DIR%\host-zh-tw.mst" 1028

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%TMP_DIR%\aspia-host-en-us.msi" Package 1033,1031,1040,1043,1046,1049,1058,2052,1028,0
xcopy %TMP_DIR%\aspia-host-en-us.msi %ASPIA_BIN_DIR%\
ren %ASPIA_BIN_DIR%\aspia-host-en-us.msi aspia-host-%ASPIA_VERSION%-%ASPIA_ARCH%.msi

echo "##################################################"
echo "Creating MSI packages for Aspia Router"
"%WIX%\bin\candle" -out "%TMP_DIR%\\" -ext WixUtilExtension -ext WixUIExtension router.wxs
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-router.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\router.en-us.wxl "%TMP_DIR%\router.wixobj"
xcopy %TMP_DIR%\aspia-router.msi %ASPIA_BIN_DIR%\
ren %ASPIA_BIN_DIR%\aspia-router.msi aspia-router-%ASPIA_VERSION%-%ASPIA_ARCH%.msi

echo "##################################################"
echo "Creating MSI packages for Aspia Relay"
"%WIX%\bin\candle" -out "%TMP_DIR%\\" -ext WixUtilExtension -ext WixUIExtension relay.wxs
"%WIX%\bin\light" -sval -out "%TMP_DIR%\aspia-relay.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\relay.en-us.wxl "%TMP_DIR%\relay.wixobj"
xcopy %TMP_DIR%\aspia-relay.msi %ASPIA_BIN_DIR%\
ren %ASPIA_BIN_DIR%\aspia-relay.msi aspia-relay-%ASPIA_VERSION%-%ASPIA_ARCH%.msi

rem Restore working directory
popd

:END
