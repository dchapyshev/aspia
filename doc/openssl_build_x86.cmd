@echo off

rem We assume that Visual Studio 2017 is installed to the default path.
set VS_PATH="C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build"

rem OpenSSL source directory
if "%1" == "" ( goto :USAGE )

rem OpenSSL output directory
if "%2" == "" ( goto :USAGE )
goto :START

:USAGE
echo Usage: openssl_build_x86.cmd [openssl_src_dir] [openssl_out_dir]
goto :END

:START
call %VS_PATH%\vcvars32.bat

cd /d %1

call perl Configure VC-WIN32 --prefix=%2 no-shared no-zlib
call nmake
call nmake test
call nmake install

:END