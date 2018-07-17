@echo off

set SDK_VERSION=8.1

if "%2" == "" ( goto :USAGE )

if not exist "%2" (
    echo "Output directory does not exist. Create it."
    mkdir "%2"
)

if "%1" == "all" ( goto :MSI )
if "%1" == "msi" ( goto :MSI )
if "%1" == "exe" ( goto :EXE )

:USAGE
echo "Usage: build.cmd [all | msi | exe] [out_dir]"
goto :END

:MSI
echo "Creating MSI packages"
"%WIX%\bin\candle" -out "%2\\" -ext WixUtilExtension -ext WixUIExtension components.wxs product.wxs
"%WIX%\bin\light" -out "%2\aspia-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\lang.de-de.wxl "%2\components.wixobj" "%2\product.wixobj"
"%WIX%\bin\light" -out "%2\aspia-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\lang.en-us.wxl "%2\components.wixobj" "%2\product.wixobj"
"%WIX%\bin\light" -out "%2\aspia-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\lang.nl-nl.wxl "%2\components.wixobj" "%2\product.wixobj"
"%WIX%\bin\light" -out "%2\aspia-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\lang.ru-ru.wxl "%2\components.wixobj" "%2\product.wixobj"

echo "Creating MSI transforms"
"%WIX%\bin\torch" -p -t language "%2\aspia-en-us.msi" "%2\aspia-de-de.msi" -out "%2\de-de.mst"
"%WIX%\bin\torch" -p -t language "%2\aspia-en-us.msi" "%2\aspia-ru-ru.msi" -out "%2\ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%2\aspia-en-us.msi" "%2\aspia-nl-nl.msi" -out "%2\nl-nl.mst"

echo "Integration of transforms to aspia-en-us.msi"
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%2\aspia-en-us.msi" "%2\de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%2\aspia-en-us.msi" "%2\ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%2\aspia-en-us.msi" "%2\nl-nl.mst" 1043

if "%1" == "msi" ( goto :END )

:EXE
echo "Creating an executable file"
"%DNIDIR%\bin\InstallerLinker.exe" /a:"%2" /o:"%2\aspia-setup.exe" /t:"%DNIDIR%\bin\dotNetInstaller.exe" /c:dotnetinstaller.xml /f:aspia-en-us.msi /v

:END
