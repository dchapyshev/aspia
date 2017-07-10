@ECHO OFF

if "%1" == "" (
  echo "Usage: wintest.bat <Release | Debug"
	goto :ERROR
)

if not exist sodium_version.c (
	CD test\default
	if not exist sodium_version.c (
		echo "Are you on the right path?" %CD%
		goto :ERROR
	)
)

if "%2" == "x64" (SET ARCH=x64) else (SET ARCH=Win32)
SET CFLAGS=/nologo /DTEST_SRCDIR=\".\" /I..\..\src\libsodium\include\sodium /I..\..\src\libsodium\include /I..\quirks
SET LDFLAGS=/link /LTCG advapi32.lib ..\..\..\..\%1\%ARCH%\libsodium.lib
if "%1" == "Release"   ( goto :Release )
if "%1" == "Debug"   ( goto :Debug )
echo "Invalid build type"
goto :ERROR
:Release
	SET CFLAGS=%CFLAGS% /MT /Ox /DSODIUM_STATIC /DSODIUM_EXPORT=
	goto :COMPILE
:Debug
	SET CFLAGS=%CFLAGS% /GS /MTd /Od /DSODIUM_STATIC /DSODIUM_EXPORT=
	goto :COMPILE
:COMPILE
echo Running the test suite:
FOR %%f in (*.c) DO (
	cl %CFLAGS% %%f %LDFLAGS% /OUT:%%f.exe > NUL 2>&1
	if not exist %%f.exe (
		echo %%f compile failed
		goto :ERROR
	)
	%%f.exe
	if errorlevel 1 ( 
		echo %%f failed
		goto :ERROR
	) else (
		echo %%f ok
	)
)
REM Remove temporary files
del *.exe *.obj *.res 
EXIT /B 0
:ERROR
EXIT /B 1
