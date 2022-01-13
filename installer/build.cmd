@echo off

set SDK_VERSION=10.0.18362.0

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
"%WIX%\bin\light" -out "%1\aspia-console-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\console.pt-br.wxl "%1\console.wixobj"
"%WIX%\bin\light" -out "%1\aspia-console-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\console.ru-ru.wxl "%1\console.wixobj"
"%WIX%\bin\light" -out "%1\aspia-console-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\console.uk-ua.wxl "%1\console.wixobj"
"%WIX%\bin\light" -out "%1\aspia-console-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\console.zh-cn.wxl "%1\console.wixobj"

echo "Creating MSI transforms for Aspia Console"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-de-de.msi" -out "%1\console-de-de.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-nl-nl.msi" -out "%1\console-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-pt-br.msi" -out "%1\console-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-ru-ru.msi" -out "%1\console-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-uk-ua.msi" -out "%1\console-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-console-en-us.msi" "%1\aspia-console-zh-cn.msi" -out "%1\console-zh-cn.mst"

echo "Integration of transforms for Aspia Console"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-console-en-us.msi" "%1\console-zh-cn.mst" 2052

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%1\aspia-console-en-us.msi" Package 1033,1031,1043,1046,1049,1058,2052

echo "Creating MSI packages for Aspia Client"
"%WIX%\bin\candle" -out "%1\\" -ext WixUtilExtension -ext WixUIExtension client.wxs
"%WIX%\bin\light" -out "%1\aspia-client-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\client.de-de.wxl "%1\client.wixobj"
"%WIX%\bin\light" -out "%1\aspia-client-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\client.en-us.wxl "%1\client.wixobj"
"%WIX%\bin\light" -out "%1\aspia-client-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\client.nl-nl.wxl "%1\client.wixobj"
"%WIX%\bin\light" -out "%1\aspia-client-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\client.pt-br.wxl "%1\client.wixobj"
"%WIX%\bin\light" -out "%1\aspia-client-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\client.ru-ru.wxl "%1\client.wixobj"
"%WIX%\bin\light" -out "%1\aspia-client-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\client.uk-ua.wxl "%1\client.wixobj"
"%WIX%\bin\light" -out "%1\aspia-client-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\client.zh-cn.wxl "%1\client.wixobj"

echo "Creating MSI transforms for Aspia Client"
"%WIX%\bin\torch" -p -t language "%1\aspia-client-en-us.msi" "%1\aspia-client-de-de.msi" -out "%1\client-de-de.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-client-en-us.msi" "%1\aspia-client-nl-nl.msi" -out "%1\client-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-client-en-us.msi" "%1\aspia-client-pt-br.msi" -out "%1\client-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-client-en-us.msi" "%1\aspia-client-ru-ru.msi" -out "%1\client-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-client-en-us.msi" "%1\aspia-client-uk-ua.msi" -out "%1\client-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-client-en-us.msi" "%1\aspia-client-zh-cn.msi" -out "%1\client-zh-cn.mst"

echo "Integration of transforms for Aspia Client"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-client-en-us.msi" "%1\client-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-client-en-us.msi" "%1\client-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-client-en-us.msi" "%1\client-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-client-en-us.msi" "%1\client-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-client-en-us.msi" "%1\client-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-client-en-us.msi" "%1\client-zh-cn.mst" 2052

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%1\aspia-client-en-us.msi" Package 1033,1031,1043,1046,1049,1058,2052

echo "Creating MSI packages for Aspia Host"
"%WIX%\bin\candle" -out "%1\\" -ext WixUtilExtension -ext WixUIExtension host.wxs
"%WIX%\bin\light" -out "%1\aspia-host-de-de.msi" -cultures:de-de -ext WixUtilExtension -ext WixUIExtension -loc translations\host.de-de.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-en-us.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\host.en-us.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-nl-nl.msi" -cultures:nl-nl -ext WixUtilExtension -ext WixUIExtension -loc translations\host.nl-nl.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-pt-br.msi" -cultures:pt-br -ext WixUtilExtension -ext WixUIExtension -loc translations\host.pt-br.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-ru-ru.msi" -cultures:ru-ru -ext WixUtilExtension -ext WixUIExtension -loc translations\host.ru-ru.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-uk-ua.msi" -cultures:uk-ua -ext WixUtilExtension -ext WixUIExtension -loc translations\host.uk-ua.wxl "%1\host.wixobj"
"%WIX%\bin\light" -out "%1\aspia-host-zh-cn.msi" -cultures:zh-cn -ext WixUtilExtension -ext WixUIExtension -loc translations\host.zh-cn.wxl "%1\host.wixobj"

echo "Creating MSI transforms for Aspia Host"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-de-de.msi" -out "%1\host-de-de.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-nl-nl.msi" -out "%1\host-nl-nl.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-pt-br.msi" -out "%1\host-pt-br.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-ru-ru.msi" -out "%1\host-ru-ru.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-uk-ua.msi" -out "%1\host-uk-ua.mst"
"%WIX%\bin\torch" -p -t language "%1\aspia-host-en-us.msi" "%1\aspia-host-zh-cn.msi" -out "%1\host-zh-cn.mst"

echo "Integration of transforms for Aspia Host"
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-de-de.mst" 1031
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-nl-nl.mst" 1043
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-pt-br.mst" 1046
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-ru-ru.mst" 1049
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-uk-ua.mst" 1058
cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wisubstg.vbs" "%1\aspia-host-en-us.msi" "%1\host-zh-cn.mst" 2052

cscript "%ProgramFiles(x86)%\Windows Kits\10\bin\%SDK_VERSION%\x86\wilangid.vbs" "%1\aspia-host-en-us.msi" Package 1033,1031,1043,1046,1049,1058,2052

echo "Creating MSI packages for Aspia Router"
"%WIX%\bin\candle" -out "%1\\" -ext WixUtilExtension -ext WixUIExtension router.wxs
"%WIX%\bin\light" -out "%1\aspia-router.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\router.en-us.wxl "%1\router.wixobj"

echo "Creating MSI packages for Aspia Relay"
"%WIX%\bin\candle" -out "%1\\" -ext WixUtilExtension -ext WixUIExtension relay.wxs
"%WIX%\bin\light" -out "%1\aspia-relay.msi" -cultures:en-us -ext WixUtilExtension -ext WixUIExtension -loc translations\relay.en-us.wxl "%1\relay.wixobj"

:END
