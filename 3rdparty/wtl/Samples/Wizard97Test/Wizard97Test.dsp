# Microsoft Developer Studio Project File - Name="Wizard97Test" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=Wizard97Test - Win32 Unicode Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Wizard97Test.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Wizard97Test.mak" CFG="Wizard97Test - Win32 Unicode Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Wizard97Test - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "Wizard97Test - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "Wizard97Test - Win32 Unicode Debug" (based on "Win32 (x86) Application")
!MESSAGE "Wizard97Test - Win32 Unicode Release" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Wizard97Test - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W4 /Zi /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "STRICT" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib HtmlHelp.lib /nologo /subsystem:windows /debug /machine:I386
# Begin Special Build Tool
OutDir=.\Release
SOURCE="$(InputPath)"
PostBuild_Cmds=if not exist .\help\Wizard97Test.chm echo Error - help file not compiled. Please build the HTML Help Project Wizard97Test.hhp using HTML Help Workshop.	xcopy /y /d /q .\help\Wizard97Test.chm $(OutDir)
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Wizard97Test - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "STRICT" /FR /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib HtmlHelp.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# Begin Special Build Tool
OutDir=.\Debug
SOURCE="$(InputPath)"
PostBuild_Cmds=if not exist .\help\Wizard97Test.chm echo Error - help file not compiled. Please build the HTML Help Project Wizard97Test.hhp using HTML Help Workshop.	xcopy /y /d /q .\help\Wizard97Test.chm $(OutDir)
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Wizard97Test - Win32 Unicode Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Wizard97Test___Win32_Unicode_Debug"
# PROP BASE Intermediate_Dir "Wizard97Test___Win32_Unicode_Debug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Unicode_Debug"
# PROP Intermediate_Dir "Unicode_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "STRICT" /FR /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W4 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_UNICODE" /D "STRICT" /FR /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib HtmlHelp.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib HtmlHelp.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# Begin Special Build Tool
OutDir=.\Unicode_Debug
SOURCE="$(InputPath)"
PostBuild_Cmds=if not exist .\help\Wizard97Test.chm echo Error - help file not compiled. Please build the HTML Help Project Wizard97Test.hhp using HTML Help Workshop.	xcopy /y /d /q .\help\Wizard97Test.chm $(OutDir)
# End Special Build Tool

!ELSEIF  "$(CFG)" == "Wizard97Test - Win32 Unicode Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Wizard97Test___Win32_Unicode_Release"
# PROP BASE Intermediate_Dir "Wizard97Test___Win32_Unicode_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Unicode_Release"
# PROP Intermediate_Dir "Unicode_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "STRICT" /D "_ATL_MIN_CRT" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W4 /Zi /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_UNICODE" /D "STRICT" /FR /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib HtmlHelp.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib shlwapi.lib HtmlHelp.lib /nologo /subsystem:windows /debug /machine:I386
# Begin Special Build Tool
OutDir=.\Unicode_Release
SOURCE="$(InputPath)"
PostBuild_Cmds=if not exist .\help\Wizard97Test.chm echo Error - help file not compiled. Please build the HTML Help Project Wizard97Test.hhp using HTML Help Workshop.	xcopy /y /d /q .\help\Wizard97Test.chm $(OutDir)
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "Wizard97Test - Win32 Release"
# Name "Wizard97Test - Win32 Debug"
# Name "Wizard97Test - Win32 Unicode Debug"
# Name "Wizard97Test - Win32 Unicode Release"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "Wizard Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Wizard\TestWizard.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardCompletionPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardFilePreviewPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardOutputPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardPathFilterPage.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardSheet.cpp
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardWelcomePage.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\stdafx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=.\Wizard97Test.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Group "Wizard Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Wizard\FolderDialogStatusText.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizard.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardCompletionPage.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardFilePreviewPage.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardInfo.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardOutputPage.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardPathFilterPage.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardSheet.h
# End Source File
# Begin Source File

SOURCE=.\Wizard\TestWizardWelcomePage.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\stdafx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Group "Image Resource Files"

# PROP Default_Filter "bmp;ico"
# Begin Source File

SOURCE=.\res\header.bmp
# End Source File
# Begin Source File

SOURCE=.\res\watermark.bmp
# End Source File
# Begin Source File

SOURCE=.\res\Wizard97Test.ico
# End Source File
# End Group
# Begin Group "Help Resource Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\resource.hm
# End Source File
# End Group
# Begin Source File

SOURCE=.\resource.h
# End Source File
# Begin Source File

SOURCE=.\res\Wizard97Test.exe.manifest
# End Source File
# Begin Source File

SOURCE=.\Wizard97Test.rc
# End Source File
# End Group
# End Target
# End Project
