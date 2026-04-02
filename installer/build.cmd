@echo off

set SDK_VERSION=10.0.26100.0

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

rem Locale list (excluding en-us which is always the base).
rem Format: culture:lcid
set LOCALES=cs-cz:1029 da-dk:1030 de-de:1031 el-gr:1032 es-es:1034 fr-fr:1036 he-il:1037 hu-hu:1038 it-it:1040 ja-jp:1041 ko-kr:1042 nl-nl:1043 nb-no:1044 pl-pl:1045 pt-br:1046 pt-pt:2070 ru-ru:1049 sv-se:1053 tr-tr:1055 uk-ua:1058 zh-cn:2052 zh-tw:1028

rem LCID list for wilangid.vbs (en-us=1033 + all others + 0 for neutral).
set ALL_LCIDS=1033,1029,1030,1031,1032,1034,1036,1037,1038,1040,1041,1042,1043,1044,1045,1046,2070,1049,1053,1055,1058,2052,1028,0

set WISUBSTG="%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs"
set WILANGID="%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs"

rem Build multilingual MSI for Console, Client, and Host.
call :BUILD_MULTILINGUAL_MSI console
call :BUILD_MULTILINGUAL_MSI client
call :BUILD_MULTILINGUAL_MSI host

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
goto :END

rem ============================================================================
rem Subroutine: BUILD_MULTILINGUAL_MSI
rem Usage: call :BUILD_MULTILINGUAL_MSI <product>
rem   where <product> is console, client, or host.
rem ============================================================================
:BUILD_MULTILINGUAL_MSI
setlocal
set PRODUCT=%~1
set PRODUCT_EN_US_MSI=%ASPIA_BIN_DIR%\aspia-%PRODUCT%-%EN_US_POSTFIX%.msi

echo "##################################################"
echo "Creating MSI packages for Aspia %PRODUCT%"
"%WIX%\bin\candle" -out "%ASPIA_BIN_DIR%\\" -arch %CANDLE_ARCH% -ext WixUtilExtension -ext WixUIExtension %PRODUCT%.wxs
"%WIX%\bin\light" -sval -out "%PRODUCT_EN_US_MSI%" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\%PRODUCT%.en-us.wxl "%ASPIA_BIN_DIR%\%PRODUCT%.wixobj"

for %%L in (%LOCALES%) do (
    for /f "tokens=1,2 delims=:" %%A in ("%%L") do (
        "%WIX%\bin\light" -sval -out "%ASPIA_BIN_DIR%\aspia-%PRODUCT%-%%A.msi" -cultures:%%A -ext WixUtilExtension -ext WixUIExtension -loc translations\%PRODUCT%.%%A.wxl "%ASPIA_BIN_DIR%\%PRODUCT%.wixobj"
    )
)

echo "##################################################"
echo "Creating MSI transforms for Aspia %PRODUCT%"
for %%L in (%LOCALES%) do (
    for /f "tokens=1,2 delims=:" %%A in ("%%L") do (
        "%WIX%\bin\torch" -p -t language "%PRODUCT_EN_US_MSI%" "%ASPIA_BIN_DIR%\aspia-%PRODUCT%-%%A.msi" -out "%ASPIA_BIN_DIR%\%PRODUCT%-%%A.mst"
    )
)

echo "##################################################"
echo "Integration of transforms for Aspia %PRODUCT%"
for %%L in (%LOCALES%) do (
    for /f "tokens=1,2 delims=:" %%A in ("%%L") do (
        cscript %WISUBSTG% "%PRODUCT_EN_US_MSI%" "%ASPIA_BIN_DIR%\%PRODUCT%-%%A.mst" %%B
    )
)

cscript %WILANGID% "%PRODUCT_EN_US_MSI%" Package %ALL_LCIDS%

endlocal
goto :eof

:END
