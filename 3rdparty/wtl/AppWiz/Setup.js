// Windows Template Library - WTL version 9.10
// Copyright (C) Microsoft Corporation, WTL Team. All rights reserved.
//
// This file is a part of the Windows Template Library.
// The use and distribution terms for this software are covered by the
// Microsoft Public License (http://opensource.org/licenses/MS-PL)
// which can be found in the file MS-PL.txt at the root folder.

// WTL App Wizard universal setup program for Visual Studio

"use strict";

main();

function main()
{
	// Decode command line arguments
	var bDebug = false;
	var bElevated = false;
	var strVersion = "";
	var bCopyFiles = false;

	var Args = WScript.Arguments;
	for(var i = 0; i < Args.length; i++)
	{
		if(Args(i) == "/debug")
			bDebug = true;
		else if(Args(i) == "/elevated")
			bElevated = true;
		else if(Args(i).substr(0, 5) == "/ver:")
			strVersion = Args(i).substr(5);
		else if(Args(i) == "/copyfiles")
			bCopyFiles = true;
	}

	// See if UAC is enabled
	var Shell = WScript.CreateObject("Shell.Application");
	if(!bElevated && Shell.IsRestricted("System", "EnableLUA"))
	{
		// Check that the script is being run interactively.
		if(!WScript.Interactive)
		{
			WScript.Echo("ERROR: Elevation required.");
			return;
		}
		
		// Now relaunch the script, using the "RunAs" verb to elevate
		var strParams = "\"" + WScript.ScriptFullName + "\"";
		if(bDebug)
			strParams += " /debug";
		if(strVersion)
			strParams += " /ver:" + strVersion;
		if(bCopyFiles)
			strParams += " /copyfiles";
		strParams += " /elevated";
		Shell.ShellExecute(WScript.FullName, strParams, null, "RunAs");
		return;
	}

	// Create shell object
	var WSShell = WScript.CreateObject("WScript.Shell");
	// Create file system object
	var FileSys = WScript.CreateObject("Scripting.FileSystemObject");

	// Get the folder containing the script file
	var strValue = FileSys.GetParentFolderName(WScript.ScriptFullName);
	if(strValue == null || strValue == "")
		strValue = ".";

	var strSourceFolder = FileSys.BuildPath(strValue, "Files");
	if(bDebug)
		WScript.Echo("Source: " + strSourceFolder);

	if(!FileSys.FolderExists(strSourceFolder))
	{
		WScript.Echo("ERROR: Cannot find Wizard folder (should be: " + strSourceFolder + ")");
		return;
	}

	if(!strVersion) 
		MessageBox(WSShell, "Setup will search for installed versions of Visual Studio,\nand ask to add the WTL App Wizard for each of them.");
	
	var strRegKey_32 = "HKLM\\Software\\";
	var strRegKey_64 = "HKLM\\Software\\Wow6432Node\\";

	var nVersions = 14;

	var astrRegKeyVer = new Array();
	astrRegKeyVer[0] = "Microsoft\\VisualStudio\\7.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[1] = "Microsoft\\VisualStudio\\7.1\\Setup\\VC\\ProductDir";
	astrRegKeyVer[2] = "Microsoft\\VisualStudio\\8.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[3] = "Microsoft\\VisualStudio\\9.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[4] = "Microsoft\\VisualStudio\\10.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[5] = "Microsoft\\VisualStudio\\11.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[6] = "Microsoft\\VisualStudio\\12.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[7] = "Microsoft\\VisualStudio\\14.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[8] = "Microsoft\\VCExpress\\8.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[9] = "Microsoft\\VCExpress\\9.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[10] = "Microsoft\\VCExpress\\10.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[11] = "Microsoft\\VCExpress\\11.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[12] = "Microsoft\\VCExpress\\12.0\\Setup\\VC\\ProductDir";
	astrRegKeyVer[13] = "Microsoft\\VisualStudio\\14.0\\Setup\\VC\\ProductDir";

	var astrFolder = new Array();
	astrFolder[0] = "vcprojects";
	astrFolder[1] = "vcprojects";
	astrFolder[2] = "vcprojects";
	astrFolder[3] = "vcprojects";
	astrFolder[4] = "vcprojects";
	astrFolder[5] = "vcprojects";
	astrFolder[6] = "vcprojects";
	astrFolder[7] = "vcprojects";
	astrFolder[8] = "Express\\vcprojects";
	astrFolder[9] = "Express\\vcprojects";
	astrFolder[10] = "Express\\vcprojects";
	astrFolder[11] = "vcprojects_WDExpress";
	astrFolder[12] = "vcprojects_WDExpress";
	astrFolder[13] = "vcprojects_WDExpress";

	var astrVersions = new Array();
	astrVersions[0] = "Visual Studio 2002 (7.0)";
	astrVersions[1] = "Visual Studio 2003 (7.1)";
	astrVersions[2] = "Visual Studio 2005 (8.0)";
	astrVersions[3] = "Visual Studio 2008 (9.0)";
	astrVersions[4] = "Visual Studio 2010 (10.0)";
	astrVersions[5] = "Visual Studio 2012 (11.0)";
	astrVersions[6] = "Visual Studio 2013 (12.0)";
	astrVersions[7] = "Visual Studio 2015 (14.0)";
	astrVersions[8] = "Visual Studio 2005 Express (8.0)";
	astrVersions[9] = "Visual Studio 2008 Express (9.0)";
	astrVersions[10] = "Visual Studio 2010 Express (10.0)";
	astrVersions[11] = "Visual Studio 2012 Express (11.0)";
	astrVersions[12] = "Visual Studio 2013 Express (12.0)";
	astrVersions[13] = "Visual Studio 2015 Express (14.0)";

	var abExpress = new Array();
	abExpress[0] = false;
	abExpress[1] = false;
	abExpress[2] = false;
	abExpress[3] = false;
	abExpress[4] = false;
	abExpress[5] = false;
	abExpress[6] = false;
	abExpress[7] = false;
	abExpress[8] = true;
	abExpress[9] = true;
	abExpress[10] = true;
	abExpress[11] = true;
	abExpress[12] = true;
	abExpress[13] = true;

	var astrWizVer = new Array();
	astrWizVer[0] = "7.0";
	astrWizVer[1] = "7.1";
	astrWizVer[2] = "8.0";
	astrWizVer[3] = "9.0";
	astrWizVer[4] = "10.0";
	astrWizVer[5] = "11.0";
	astrWizVer[6] = "12.0";
	astrWizVer[7] = "14.0";
	astrWizVer[8] = "8.0";
	astrWizVer[9] = "9.0";
	astrWizVer[10] = "10.0";
	astrWizVer[11] = "11.0";
	astrWizVer[12] = "12.0";
	astrWizVer[13] = "14.0";

	var astrParamVer = new Array();
	astrParamVer[4] = "10";
	astrParamVer[5] = "11";
	astrParamVer[6] = "12";
	astrParamVer[7] = "14";
	astrParamVer[11] = "11E";
	astrParamVer[12] = "12E";
	astrParamVer[13] = "14E";

	var nSpecial = 4;

	var nWizSpecial = 0;

	var bFound = false;
	for(var i = 0; i < nVersions; i++)
	{
		if(strVersion && (strVersion != astrParamVer[i]))
			continue;

		if(bDebug)
			WScript.Echo("Looking for: " + astrVersions[i]);

		try
		{
			var strVCKey = strRegKey_32 + astrRegKeyVer[i];
			strValue = WSShell.RegRead(strVCKey);
		}
		catch(e)
		{
			try
			{
				var strVCKey_x64 = strRegKey_64 + astrRegKeyVer[i];
				strValue = WSShell.RegRead(strVCKey_x64);
			}
			catch(e)
			{
				continue;
			}
		}

		var strDestFolder = FileSys.BuildPath(strValue, astrFolder[i]);
		if(bDebug)
			WScript.Echo("Destination: " + strDestFolder);
		if(!FileSys.FolderExists(strDestFolder))
			continue;

		if(i == nSpecial)   // special case for VS2010
		{
			var strCheckFile = FileSys.BuildPath(strDestFolder, "vc.vsdir");
			if(!FileSys.FileExists(strCheckFile))
				continue;
		}

		var strDataDestFolder = "";
		if(bCopyFiles)
		{
			strDataDestFolder = FileSys.BuildPath(strValue, "VCWizards");
			if(bDebug)
				WScript.Echo("Data Destination: " + strDataDestFolder);
			if(!FileSys.FolderExists(strDataDestFolder))
				continue;

			if(i < 2)   // special case for VS2002/2003
				strDataDestFolder = FileSys.BuildPath(strDataDestFolder, "WTL");
			else
				strDataDestFolder = FileSys.BuildPath(strDataDestFolder, "AppWiz\\WTL");
		}

		bFound = true;
		var bRet = true;
		if(!strVersion) 
		{
			var strMsg = "Found: " + astrVersions[i] + "\n\nInstall WTL App Wizard?";
			bRet = MessageBox(WSShell, strMsg, true);
		}
		if(bRet)
		{
			var bWizSpecial = (i == nWizSpecial);   // special case for VS2002
			SetupWizard(WSShell, FileSys, strSourceFolder, strDestFolder, strDataDestFolder, astrWizVer[i], bWizSpecial, abExpress[i], bDebug);
		}
	}

	if(!strVersion)
	{
		if(bFound)
			MessageBox(WSShell, "Done!");
		else
			MessageBox(WSShell, "Setup could not find Visual Studio installed");
	}
}

function MessageBox(WSShell, strText, bYesNo)
{
	var nType = bYesNo ? (4 + 32) : 0;   // 4 = Yes/No buttons, 32 = Questionmark icon, 0 = OK button
	var nRetBtn = WSShell.Popup(strText, 0, "WTL App Wizard Setup", nType);
	return (nRetBtn == 6);   // 6 = Yes;
}

function SetupWizard(WSShell, FileSys, strSourceFolder, strDestFolder, strDataDestFolder, strWizVer, bWizSpecial, bExpress, bDebug)
{
	// Copy files
	try
	{
		var strSrc = FileSys.BuildPath(strSourceFolder, "WTLAppWiz.ico");
		var strDest = FileSys.BuildPath(strDestFolder, "WTLAppWiz.ico");
		FileSys.CopyFile(strSrc, strDest);

		strSrc = FileSys.BuildPath(strSourceFolder, "WTLAppWiz.vsdir");
		strDest = FileSys.BuildPath(strDestFolder, "WTLAppWiz.vsdir");
		FileSys.CopyFile(strSrc, strDest);

		if(strDataDestFolder != "")
			FileSys.CopyFolder(strSourceFolder, strDataDestFolder, true);
	}
	catch(e)
	{
		var strError = "no info";
		if(e.description.length != 0)
			strError = e.description;
		WScript.Echo("ERROR: Cannot copy file (" + strError + ")");
		return;
	}

	// Read and write WTLAppWiz.vsz, add engine version and replace path when found
	try
	{
		var strSrc = FileSys.BuildPath(strSourceFolder, "WTLAppWiz.vsz");
		var strDest = FileSys.BuildPath(strDestFolder, "WTLAppWiz.vsz");

		var ForReading = 1;
		var fileSrc = FileSys.OpenTextFile(strSrc, ForReading);
		if(fileSrc == null)
		{
			WScript.Echo("ERROR: Cannot open source file " + strSrc);
			return;
		}

		var ForWriting = 2;
		var fileDest = FileSys.OpenTextFile(strDest, ForWriting, true);
		if(fileDest == null)
		{
			WScript.Echo("ERROR: Cannot open destination file" + strDest);
			return;
		}

		while(!fileSrc.AtEndOfStream)
		{
			var strLine = fileSrc.ReadLine();
			if(!bWizSpecial && (strLine.indexOf("Wizard=VsWizard.VsWizardEngine") != -1))
			{
				strLine += "." + strWizVer;
			}
			else if(strLine.indexOf("WIZARD_VERSION") != -1)
			{
				strLine = "Param=\"WIZARD_VERSION = " + strWizVer + "\"";
			}
			else if(strLine.indexOf("ABSOLUTE_PATH") != -1)
			{
				if(strDataDestFolder == "")
					strLine = "Param=\"ABSOLUTE_PATH = " + strSourceFolder + "\"";
				else
					strLine = "Param=\"ABSOLUTE_PATH = " + strDataDestFolder + "\"";
			}

			fileDest.WriteLine(strLine);
		}

		if(bExpress)
			fileDest.WriteLine("Param=\"VC_EXPRESS = 1\"");

		fileSrc.Close();
		fileDest.Close();
	}
	catch(e)
	{
		var strError = "no info";
		if(e.description.length != 0)
			strError = e.description;
		WScript.Echo("ERROR: Cannot read and write WTLAppWiz.vsz (" + strError + ")");
		return;
	}

	// Create WTL folder
	var strDestWTLFolder = "";
	try
	{
		strDestWTLFolder = FileSys.BuildPath(strDestFolder, "WTL");
		if(!FileSys.FolderExists(strDestWTLFolder))
			FileSys.CreateFolder(strDestWTLFolder);
		if(bDebug)
			WScript.Echo("WTL Folder: " + strDestWTLFolder);
	}
	catch(e)
	{
		var strError = "no info";
		if(e.description.length != 0)
			strError = e.description;
		WScript.Echo("ERROR: Cannot create WTL folder (" + strError + ")");
		return;
	}

	// Read and write additional WTLAppWiz.vsdir, add path to the wizard location
	try
	{
		var strSrc = FileSys.BuildPath(strSourceFolder, "WTLAppWiz.vsdir");
		var strDest = FileSys.BuildPath(strDestWTLFolder, "WTLAppWiz.vsdir");

		var ForReading = 1;
		var fileSrc = FileSys.OpenTextFile(strSrc, ForReading);
		if(fileSrc == null)
		{
			WScript.Echo("ERROR: Cannot open source file " + strSrc);
			return;
		}

		var ForWriting = 2;
		var fileDest = FileSys.OpenTextFile(strDest, ForWriting, true);
		if(fileDest == null)
		{
			WScript.Echo("ERROR: Cannot open destination file" + strDest);
			return;
		}

		while(!fileSrc.AtEndOfStream)
		{
			var strLine = fileSrc.ReadLine();
			if(strLine.indexOf("WTLAppWiz.vsz|") != -1)
				strLine = "..\\" + strLine;
			fileDest.WriteLine(strLine);
		}

		fileSrc.Close();
		fileDest.Close();
	}
	catch(e)
	{
		var strError = "no info";
		if(e.description.length != 0)
			strError = e.description;
		WScript.Echo("ERROR: Cannot read and write WTL\\WTLAppWiz.vsdir (" + strError + ")");
		return;
	}

	WScript.Echo("App Wizard successfully installed!");
}
