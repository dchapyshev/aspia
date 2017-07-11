// Windows Template Library - WTL version 9.10
// Copyright (C) Microsoft Corporation, WTL Team. All rights reserved.
//
// This file is a part of the Windows Template Library.
// The use and distribution terms for this software are covered by the
// Microsoft Public License (http://opensource.org/licenses/MS-PL)
// which can be found in the file MS-PL.txt at the root folder.

// Setup program for the Windows Mobile WTL App Wizard for VC++ 8.0 (Whidbey)

// Elevated privilege check
try 
{
	var bElevated = false;
	var Args = WScript.Arguments;
	for(i = 0; i < Args.length ; i++)
		if (bElevated = (Args(i) == "/elevated"))
			break;

	var AppShell = WScript.CreateObject("Shell.Application");

	if (!bElevated && AppShell.IsRestricted("System", "EnableLUA")) 
		throw (WScript.Interactive == true) ? "Restricted" : "Elevation required.";
}
catch(e)
{
	if (e == "Restricted")
		AppShell.ShellExecute("WScript.exe", "\"" + WScript.ScriptFullName + "\"" + " /elevated", null, "RunAs");
	else
		WScript.Echo("Error: " + e);

	WScript.Quit();
}

// WTLMobile AppWizard registration
try
{
	var fso = WScript.CreateObject("Scripting.FileSystemObject");
	var SourceBase = fso.GetParentFolderName(WScript.ScriptFullName) + "\\Files";
	var Source = SourceBase + "\\WTLMobile.";

	var shell = WScript.CreateObject("WScript.Shell");
	var DestBase;
	try { 
	    DestBase = shell.RegRead("HKLM\\Software\\Microsoft\\VisualStudio\\8.0\\Setup\\VC\\ProductDir") + "\\vcprojects";
	    }
    catch (e) {
        try {
            DestBase = shell.RegRead("HKLM\\Software\\Wow6432Node\\Microsoft\\VisualStudio\\8.0\\Setup\\VC\\ProductDir") + "\\vcprojects";
        }
        catch (e) {
            WScript.Echo("ERROR: Cannot find where Visual Studio 8.0 is installed.");
            WScript.Quit();
        }
	}
	        
        
	var Dest =DestBase + "\\WTLMobile.";

	var vsz = Source + "vsz";
	var vsdir = Source + "vsdir";
	var vszText, vsdirText; 

	var ts = fso.OpenTextFile(vsz,1);
	vszText = ts.ReadAll();
	ts.Close();
	vszText = vszText.replace(/(.+PATH\s=).+/,"$1" + SourceBase +"\"\r");
	ts = fso.OpenTextFile(vsdir, 1);
	vsdirText = ts.ReadAll();
	ts.Close();

	fso.CopyFile(Source + "ico", Dest + "ico");

	ts = fso.OpenTextFile(Dest + "vsz", 2, true);
	ts.Write(vszText);
	ts.Close();

	ts = fso.OpenTextFile(Dest + "vsdir", 2, true);
	ts.Write(vsdirText);
	ts.Close();

	vsdirText = "..\\" + vsdirText;

	var DestFolder = DestBase + "\\WTL";
	if(!fso.FolderExists(DestFolder))
		fso.CreateFolder(DestFolder);

	Dest = DestBase + "\\WTL\\WTLMobile.vsdir";
	ts = fso.OpenTextFile(Dest, 2, true);
	ts.Write(vsdirText);
	ts.Close();

	DestFolder = DestBase + "\\smartdevice";
	if(!fso.FolderExists(DestFolder))
		fso.CreateFolder(DestFolder);

	Dest = DestBase + "\\smartdevice\\WTLMobile.vsdir";
	ts = fso.OpenTextFile(Dest, 2, true);
	ts.Write(vsdirText);
	ts.Close();

	WScript.Echo("WTL Mobile App Wizard successfully installed!");
}
catch(e)
{
	WScript.Echo("Error " + e);
}
