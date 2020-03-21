@echo off

set SDK_VERSION=8.1

if "%1" == "" ( goto :USAGE )

if not exist "%1" (
    echo "Output directory does not exist. Create it."
    mkdir "%1"
)

goto :MSI

:USAGE
echo "Usage: build.cmd [out_dir]"
goto :END

:MSI
echo "Creating MSI packages for Aspia Console"
"%WIX%\bin\candle" -out "%1\\" -ext WixUtilExtension -ext WixUIExtension console.wxs
"%WIX%\bin\light" -out "%1\aspia-console-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\console.de-de.wxl "%1\console.wixobj"
"%WIX%\bin\light" -out "%1\aspia-console-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\console.en-us.wxl "%1\console.wixobj"
"%WIX%\bin\light" -out "%1\aspia-console-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\console.nl-nl.wxl "%1\console.wixobj"
"%WIX%\bin\light" -out "%1\aspia-console-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\console.ru-ru.wxl "%1\console.wixobj"
"%WIX%\bin\light" -out "%1\aspia-console-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\console.uk-ua.wxl "%1\console.wixobj"

echo "Creating MSI transforms for Aspia Console"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-de-de.msi" -out "%1\console-de-de.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-ru-ru.msi" -out "%1\console-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-nl-nl.msi" -out "%1\console-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-uk-ua.msi" -out "%1\console-uk-ua.mst"

echo "Integration of transforms for Aspia Console"
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-uk-ua.mst" 1058

cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wilangid.vbs" "%1\aspia-console-en-us.msi" Package 1033,1031,1049,1043,1058

echo "Creating MSI packages for Aspia Host"
"%WIX%\bin\candle" -out "%1\\" -ext WixUtilExtension -ext WixUIExtension host.wxs
"%WIX%\bin\light" -out "%1\aspia-host-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\host.de-de.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\host.en-us.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\host.nl-nl.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\host.ru-ru.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\host.uk-ua.wxl "%1\host.wixobj"

echo "Creating MSI transforms for Aspia Host"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-de-de.msi" -out "%1\host-de-de.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-ru-ru.msi" -out "%1\host-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-nl-nl.msi" -out "%1\host-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-uk-ua.msi" -out "%1\host-uk-ua.mst"

echo "Integration of transforms for Aspia Host"
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-uk-ua.mst" 1058

cscript "%ProgramFiles(x86)%\Windows Kits\%SDK_VERSION%\bin\x86\wilangid.vbs" "%1\aspia-host-en-us.msi" Package 1033,1031,1049,1043,1058

:END
