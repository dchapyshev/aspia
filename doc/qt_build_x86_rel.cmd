@echo off

rem We assume that Visual Studio 2017 is installed to the default path.
set VS_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build"

rem Qt source directory
if "%1" == "" ( goto :USAGE )

rem Qt output directory
if "%2" == "" ( goto :USAGE )

rem OpenSSL directory
if "%3" == "" ( goto :USAGE )
goto :START

:USAGE
echo Usage: qt_build_x86_rel.cmd [qt_src_dir] [qt_out_dir] [openssl_dir]
goto :END

:START
call %VS_PATH%\vcvars32.bat

cd /d %1

call configure ^
    -opensource ^
    -confirm-license ^
    -platform win32-msvc2017 ^
    -debug-and-release ^
    -static ^
    -static-runtime ^
    -mp ^
    -ltcg ^
    -ssl ^
    -openssl-linked ^
    -skip qt3d ^
    -skip qtactiveqt ^
    -skip qtcanvas3d ^
    -skip qtcharts ^
    -skip qtdatavis3d ^
    -skip qtdeclarative ^
    -skip qtdoc ^
    -skip qtconnectivity ^
    -skip qtgamepad ^
    -skip qtlocation ^
    -skip qtmultimedia ^
    -skip qtnetworkauth ^
    -skip qtpurchasing ^
    -skip qtquickcontrols ^
    -skip qtquickcontrols2 ^
    -skip qtremoteobjects ^
    -skip qtscript ^
    -skip qtsensors ^
    -skip qtserialbus ^
    -skip qtserialport ^
    -skip qtspeech ^
    -skip qtsvg ^
    -skip qtvirtualkeyboard ^
    -skip qtwebchannel ^
    -skip qtwebengine ^
    -skip qtwebsockets ^
    -skip qtwebview ^
    -skip qtxmlpatterns ^
    -nomake tools ^
    -nomake examples ^
    -nomake tests ^
    -no-feature-accessibility ^
    -no-feature-concurrent ^
    -no-feature-graphicseffect ^
    -no-feature-keysequenceedit ^
    -no-feature-textodfwriter ^
    -no-feature-ftp ^
    -no-feature-networkdiskcache ^
    -no-feature-sharedmemory ^
    -no-feature-udpsocket ^
    -no-freetype ^
    -no-harfbuzz ^
    -no-icu ^
    -no-opengl ^
    -no-angle ^
    -no-dbus ^
    -no-sql-sqlite2 ^
    -no-sql-psql ^
    -no-sql-mysql ^
    -no-sql-odbc ^
    -no-sql-oci ^
    -no-sql-ibase ^
    -no-sql-db2 ^
    -I "%3\include" ^
    -L "%3\lib" ^
    -prefix %2 ^
    OPENSSL_LIBS="-llibcrypto -llibssl -lgdi32 -luser32 -lws2_32 -ladvapi32 -lcrypt32"

call jom -j %NUMBER_OF_PROCESSORS%
call jom install

:END