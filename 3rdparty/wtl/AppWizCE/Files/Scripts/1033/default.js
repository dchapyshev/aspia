// Windows Template Library - WTL version 9.10
// Copyright (C) Microsoft Corporation, WTL Team. All rights reserved.
//
// This file is a part of the Windows Template Library.
// The use and distribution terms for this software are covered by the
// Microsoft Public License (http://opensource.org/licenses/MS-PL)
// which can be found in the file MS-PL.txt at the root folder.


var ProjWiz;
var WizardVersion = wizard.FindSymbol('WIZARD_VERSION');

if(WizardVersion >= 9.0)
	ProjWiz = new ActiveXObject("ProjWiz.SDProjWiz2.3");
else
	ProjWiz = new ActiveXObject("ProjWiz.SDProjWiz2.2");

function SetDCOMSymbols()
{
	var checkedPlatforms = wizard.FindSymbol("CHECKED_PLATFORMS");
	wizard.AddSymbol("SUPPORT_DCOM", false);
	wizard.AddSymbol("SUPPORT_NON_DCOM", false);
	for(var i = 0; i < checkedPlatforms.length; i++)
	{
		if(ProjWiz.GetBaseNativePlatformSupportsDCOM(checkedPlatforms[i]))
		{
			wizard.AddSymbol("SUPPORT_DCOM", true);
		}
		else
		{
			wizard.AddSymbol("SUPPORT_NON_DCOM", true);
		}
	}
}

function OnFinish(selProj, selObj)
{
	try
	{
		var strProjectPath = wizard.FindSymbol('PROJECT_PATH');
		var strProjectName = wizard.FindSymbol('PROJECT_NAME');

		// Create symbols based on the project name
		var strSafeProjectName = CreateSafeName(strProjectName);
		wizard.AddSymbol("SAFE_PROJECT_NAME", strSafeProjectName);
		wizard.AddSymbol("NICE_SAFE_PROJECT_NAME", strSafeProjectName.substr(0, 1).toUpperCase() + strSafeProjectName.substr(1))
		wizard.AddSymbol("UPPERCASE_SAFE_PROJECT_NAME", strSafeProjectName.toUpperCase());

		// Set current year symbol
		var d = new Date();
		var nYear = 0;
		nYear = d.getFullYear();
		if(nYear >= 2003)
			wizard.AddSymbol("WTL_CURRENT_YEAR", nYear);

		// Set APPID and LIBID symbols for COM servers
		if(wizard.FindSymbol("WTL_COM_SERVER"))
		{
			var strGuid = wizard.CreateGuid();
			var strVal = wizard.FormatGuid(strGuid, 0);
			wizard.AddSymbol("WTL_APPID", strVal);

			strGuid = wizard.CreateGuid();
			strVal = wizard.FormatGuid(strGuid, 0);
			wizard.AddSymbol("WTL_LIBID", strVal);
		}

		// Set app type symbols
		if(wizard.FindSymbol("WTL_APPTYPE_SDI"))
		{
			wizard.AddSymbol("WTL_FRAME_BASE_CLASS","CFrameWindowImpl");
		}
		else if(wizard.FindSymbol("WTL_APPTYPE_MTSDI"))
		{
			wizard.AddSymbol("WTL_FRAME_BASE_CLASS","CFrameWindowImpl");
		}
		else if(wizard.FindSymbol("WTL_APPTYPE_DLG"))
		{
			wizard.AddSymbol("WTL_MAINDLG_CLASS","CMainDlg");
			if(wizard.FindSymbol("WTL_ENABLE_AX"))
				wizard.AddSymbol("WTL_MAINDLG_BASE_CLASS", "CAxDialogImpl");
			else
				wizard.AddSymbol("WTL_MAINDLG_BASE_CLASS", "CDialogImpl");

			wizard.AddSymbol("WTL_USE_TOOLBAR", false);
			wizard.AddSymbol("WTL_USE_STATUSBAR", false);
			wizard.AddSymbol("WTL_USE_VIEW", false);
		}

		// Set view symbols
		if(wizard.FindSymbol("WTL_USE_VIEW"))
		{
			var strViewFile = strProjectName + "View";
			wizard.AddSymbol("WTL_VIEW_FILE", strViewFile);

			var strViewClass = "C" + wizard.FindSymbol("NICE_SAFE_PROJECT_NAME") + "View";
			wizard.AddSymbol("WTL_VIEW_CLASS", strViewClass);

			wizard.AddSymbol("WTL_VIEWTYPE_GENERIC", false);
			var strView = wizard.FindSymbol("WTL_COMBO_VIEW_TYPE");
			switch(strView)
			{
			case "WTL_VIEWTYPE_FORM":
				wizard.AddSymbol("WTL_VIEWTYPE_FORM", true);
				if(wizard.FindSymbol("WTL_ENABLE_AX") && wizard.FindSymbol("WTL_HOST_AX"))
					wizard.AddSymbol("WTL_VIEW_BASE_CLASS", "CAxDialogImpl");
				else
					wizard.AddSymbol("WTL_VIEW_BASE_CLASS", "CDialogImpl");
				break;
			case "WTL_VIEWTYPE_LISTBOX":
				wizard.AddSymbol("WTL_VIEWTYPE_LISTBOX", true);
				wizard.AddSymbol("WTL_VIEW_BASE", "CListBox");
				wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | LBS_WANTKEYBOARDINPUT");
				break;
			case "WTL_VIEWTYPE_EDIT":
				wizard.AddSymbol("WTL_VIEWTYPE_EDIT", true);
				wizard.AddSymbol("WTL_VIEW_BASE", "CEdit");
				wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL");
				break;
			case "WTL_VIEWTYPE_LISTVIEW":
				wizard.AddSymbol("WTL_VIEWTYPE_LISTVIEW", true);
				wizard.AddSymbol("WTL_VIEW_BASE", "CListViewCtrl");
				wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_REPORT | LVS_SHOWSELALWAYS");
				break;
			case "WTL_VIEWTYPE_TREEVIEW":
				wizard.AddSymbol("WTL_VIEWTYPE_TREEVIEW", true);
				wizard.AddSymbol("WTL_VIEW_BASE", "CTreeViewCtrl");
				wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS");
				break;
			case "WTL_VIEWTYPE_HTML":
				wizard.AddSymbol("WTL_VIEWTYPE_HTML", true);
				wizard.AddSymbol("WTL_VIEW_BASE", "CAxWindow");
				wizard.AddSymbol("WTL_ENABLE_AX", true);
				wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL");
				break;
			default:
				wizard.AddSymbol("WTL_VIEWTYPE_GENERIC", true);
				break;
			}
		}

		EnsureDevicePlatforms();

		// these calls must occur before project creation, because they affect how the templates are generated.
		SetDCOMSymbols();
		SetDeviceSymbolsForPlatforms();

		// Create project and configurations
		selProj = CreateDeviceProject(strProjectName, strProjectPath);
		AddConfigurations(selProj, strProjectName);
		AddFilters(selProj);

		var InfFile = CreateCustomInfFile();
		AddFilesToCustomProj(selProj, strProjectName, strProjectPath, InfFile);
		SetCommonPchSettings(selProj);
		InfFile.Delete();

		selProj.Object.Save();

		if(wizard.FindSymbol("WTL_APPTYPE_DLG"))
		{
			var ResHelper = wizard.ResourceHelper;
			ResHelper.OpenResourceFile(strProjectPath + "\\" + strProjectName + ".rc");
			ResHelper.OpenResourceInEditor("DIALOG", "IDD_MAINDLG");
		}
		else if(wizard.FindSymbol("WTL_USE_VIEW") && wizard.FindSymbol("WTL_VIEWTYPE_FORM"))
		{
			var strDialogID = "IDD_" + wizard.FindSymbol("UPPERCASE_SAFE_PROJECT_NAME") + "_FORM";
			var ResHelper = wizard.ResourceHelper;
			ResHelper.OpenResourceFile(strProjectPath + "\\" + strProjectName + ".rc");
			ResHelper.OpenResourceInEditor("DIALOG", strDialogID);
		}
	}
	catch(e)
	{
		if(e.description.length != 0)
			SetErrorInfo(e);
		return e.number
	}
}

function AddFilters(proj)
{
	try
	{
		// Add the folders to your project
		var strSrcFilter = wizard.FindSymbol('SOURCE_FILTER');
		var group = proj.Object.AddFilter('Source Files');
		group.Filter = strSrcFilter;

		strSrcFilter = wizard.FindSymbol('INCLUDE_FILTER');
		group = proj.Object.AddFilter('Header Files');
		group.Filter = strSrcFilter;

		strSrcFilter = wizard.FindSymbol('RESOURCE_FILTER');
		group = proj.Object.AddFilter('Resource Files');
		group.Filter = strSrcFilter;
	}
	catch(e)
	{
		throw e;
	}
}

function AddConfigurations(proj, strProjectName)
{
	try
	{
		var oConfigs = proj.Object.Configurations;
		for(var nCntr = 1; nCntr <= oConfigs.Count; nCntr++)
		{
			var config = oConfigs(nCntr);

			// Check if it's Debug configuration
			var bDebug = false;
			if(config.Name.indexOf("Debug") != -1)
				bDebug = true;

			// General settings
			config.CharacterSet = charSetUnicode;
			config.ATLMinimizesCRunTimeLibraryUsage = false;
			if(bDebug)
			{
				config.IntermediateDirectory = "$(PlatformName)\\Debug";
				config.OutputDirectory = "$(PlatformName)\\Debug";
			}
			else
			{
				config.IntermediateDirectory = "$(PlatformName)\\Release";
				config.OutputDirectory = "$(PlatformName)\\Release";
			}

			if(wizard.FindSymbol("WTL_USE_VIEW") && wizard.FindSymbol("WTL_COMBO_VIEW_TYPE") == "WTL_VIEWTYPE_HTML")
				config.UseOfATL = useATLDynamic;

			// Instruction set
			var instructionSet = "";
			var sIndex = config.Name.indexOf("(");
			var eIndex = config.Name.indexOf(")");
			if((sIndex != -1) && (eIndex != -1))
			{
				instructionSet = config.Name.substr(sIndex + 1, eIndex - sIndex - 1);
			}

			// Compiler settings
			var CLTool = config.Tools('VCCLCompilerTool');
			CLTool.UsePrecompiledHeader = pchUseUsingSpecific;
			CLTool.WarningLevel = warningLevel_3;
			if(bDebug)
			{
				CLTool.RuntimeLibrary = rtMultiThreadedDebug;
				CLTool.MinimalRebuild = true;
				CLTool.DebugInformationFormat = debugEnabled;
				CLTool.Optimization = optimizeDisabled;
			}
			else
			{
				CLTool.RuntimeLibrary = rtMultiThreaded;
				CLTool.ExceptionHandling = false;
				CLTool.DebugInformationFormat = debugDisabled;
			}

			var strDefines = GetPlatformDefine(config);
			strDefines += "_WIN32_WCE=$(CEVER);UNDER_CE=$(CEVER);WINCE;";
			if(bDebug)
				strDefines += "_DEBUG";
			else
				strDefines += "NDEBUG";

			// PLATFORMDEFINES isn't defined for all platforms
			var PlatformDefines = config.Platform.GetMacroValue("PLATFORMDEFINES");
			if(PlatformDefines != "")
				strDefines += ";$(PLATFORMDEFINES)";

			strDefines += ";$(ARCHFAM);$(_ARCHFAM_);_UNICODE"
			CLTool.PreprocessorDefinitions = strDefines;

			// Linker settings
			var LinkTool = config.Tools('VCLinkerTool');
			if(bDebug)
			{
				LinkTool.LinkIncremental = linkIncrementalYes;
				LinkTool.GenerateDebugInformation = true;
			}
			else
			{
				LinkTool.LinkIncremental = linkIncrementalNo;
			}

			LinkTool.SubSystem = subSystemNotSet;
			LinkTool.IgnoreImportLibrary = true;

			if(bDebug)
				LinkTool.AdditionalDependencies = "atlsd.lib libcmtd.lib"
			else
				LinkTool.AdditionalDependencies = "atls.lib libcmt.lib"
			LinkTool.AdditionalDependencies += " corelibc.lib coredll.lib commctrl.lib ole32.lib oleaut32.lib uuid.lib atl.lib atlosapis.lib $(NOINHERIT)"

			if(config.Platform.Name != "Win32")
			{
				LinkTool.AdditionalOptions = " /subsystem:windowsce," + ProjWiz.GetNativePlatformMajorVersion(config.Platform.Name) + "." + ProjWiz.GetNativePlatformMinorVersion(config.Platform.Name);
				LinkTool.AdditionalOptions = LinkTool.AdditionalOptions + " " + ProjWiz.GetLinkerMachineType(instructionSet);
			}

			if(config.Platform.Name == "Pocket PC 2003 (ARMV4)" ||
			   config.Platform.Name == "SmartPhone 2003 (ARMV4)" ||
			   config.Platform.Name == "Smartphone 2003 (ARMV4)" )
			{
				LinkTool.AdditionalOptions = LinkTool.AdditionalOptions + " /ARMPADCODE";
			}

			if(config.Platform.Name == "Pocket PC 2003 (Emulator)" ||
			   config.Platform.Name == "SmartPhone 2003 (Emulator)" ||
			   config.Platform.Name == "Smartphone 2003 (Emulator)" )
			{
				if(bDebug)
				{
					LinkTool.AdditionalDependencies += " libcmtx86d.lib";
				}
				else
				{
					LinkTool.AdditionalDependencies += " libcmtx86.lib";
				}
			}

			if(config.Platform.Name.indexOf("Pocket PC 2003") != -1)
			{
				LinkTool.AdditionalDependencies += " ccrtrtti.lib";
			}
			
			if(config.Platform.Name.indexOf("SmartPhone 2003") != -1 ||
			   config.Platform.Name.indexOf("Smartphone 2003") != -1 )
			{
				LinkTool.AdditionalDependencies += " ccrtrtti.lib";
			}

			LinkTool.AdditionalDependencies += " $(NOINHERIT)";
			LinkTool.DelayLoadDLLs = "$(NOINHERIT)";

			var DeployTool = config.DeploymentTool;
			if(DeployTool)
			{
				var strAddDll = ".dll|$(VCInstallDir)ce\\dll\\$(INSTRUCTIONSET)\\|%CSIDL_WINDOWS%|0;"
				if(WizardVersion >= 9.0)
					DeployTool.AdditionalFiles += "atl90" + strAddDll;
				else
					DeployTool.AdditionalFiles += "atl80" + strAddDll;
			}

			// Resource settings
			var RCTool = config.Tools("VCResourceCompilerTool");
			RCTool.Culture = rcEnglishUS;
			RCTool.AdditionalIncludeDirectories = "$(IntDir)";
			if(bDebug)
				strDefines = "_DEBUG";
			else
				strDefines = "NDEBUG";
			strDefines += ";_UNICODE;UNICODE;_WIN32_WCE"
			RCTool.PreprocessorDefinitions = strDefines;

			// MIDL settings
			var MidlTool = config.Tools("VCMidlTool");
			MidlTool.MkTypLibCompatible = false;
			if(IsPlatformWin32(config))
				MidlTool.TargetEnvironment = midlTargetWin32;
			if(bDebug)
				MidlTool.PreprocessorDefinitions = "_DEBUG";
			else
				MidlTool.PreprocessorDefinitions = "NDEBUG";
			MidlTool.HeaderFileName = strProjectName + ".h";
			MidlTool.InterfaceIdentifierFileName = strProjectName + "_i.c";
			MidlTool.ProxyFileName = strProjectName + "_p.c";
			MidlTool.GenerateStublessProxies = true;
			MidlTool.TypeLibraryName = "$(IntDir)/" + strProjectName + ".tlb";
			MidlTool.DLLDataFileName = "";

			// Post-build settings
			if(wizard.FindSymbol('WTL_COM_SERVER'))
			{
				var PostBuildTool = config.Tools("VCPostBuildEventTool");
				PostBuildTool.Description = "Performing registration...";
				PostBuildTool.CommandLine = "\"$(TargetPath)\" /RegServer";
			}
		}
	}
	catch(e)
	{
		throw e;
	}
}

function DelFile(fso, strWizTempFile)
{
	try
	{
		if(fso.FileExists(strWizTempFile))
		{
			var tmpFile = fso.GetFile(strWizTempFile);
			tmpFile.Delete();
		}
	}
	catch(e)
	{
		throw e;
	}
}

function CreateCustomInfFile()
{
	try
	{
		var fso, TemplatesFolder, TemplateFiles, strTemplate;
		fso = new ActiveXObject('Scripting.FileSystemObject');

		var TemporaryFolder = 2;
		var tfolder = fso.GetSpecialFolder(TemporaryFolder);

		var strWizTempFile = tfolder.Path + "\\" + fso.GetTempName();

		var strTemplatePath = wizard.FindSymbol('TEMPLATES_PATH');
		var strInfFile = strTemplatePath + '\\Templates.inf';
		wizard.RenderTemplate(strInfFile, strWizTempFile);

		var WizTempFile = fso.GetFile(strWizTempFile);
		return WizTempFile;
	}
	catch(e)
	{
		throw e;
	}
}

function GetTargetName(strName, strProjectName)
{
	try
	{
		var strTarget = strName;
		var strResPath = "res\\";

		if(strName.substr(0, 4) == "root")
		{
			var nNameLen = strName.length;
			if(strName == "root.ico" || strName == "rootDoc.ico" || strName == "root.exe.manifest")
			{
				strTarget = strResPath + strProjectName + strName.substr(4, nNameLen - 4);
			}
			else
			{
				strTarget = strProjectName + strName.substr(4, nNameLen - 4);
			}
		}
		else if(strName == 'frame.h')
		{
			strTarget = 'MainFrm.h';
		}
		else if(strName == 'frame.cpp')
		{
			strTarget = 'MainFrm.cpp';
		}
		else if(strName == 'view.h')
		{
			strTarget = strProjectName + 'View.h';
		}
		else if(strName == 'view.cpp')
		{
			strTarget = strProjectName + 'View.cpp';
		}
		else if(strName == 'toolbar.bmp')
		{
			strTarget = strResPath + strName;
		}

		return strTarget; 
	}
	catch(e)
	{
		throw e;
	}
}

function AddFilesToCustomProj(proj, strProjectName, strProjectPath, InfFile)
{
	try
	{
		var projItems = proj.ProjectItems

		var strTemplatePath = wizard.FindSymbol('TEMPLATES_PATH');

		var strTpl = '';
		var strName = '';

		var strTextStream = InfFile.OpenAsTextStream(1, -2);
		while (!strTextStream.AtEndOfStream)
		{
			strTpl = strTextStream.ReadLine();
			if(strTpl != '')
			{
				strName = strTpl;
				var strTarget = GetTargetName(strName, strProjectName);
				var strTemplate = strTemplatePath + '\\' + strTpl;
				var strFile = strProjectPath + '\\' + strTarget;

				var bCopyOnly = false;  //"true" will only copy the file from strTemplate to strTarget without rendering/adding to the project
				var strExt = strName.substr(strName.lastIndexOf("."));
				if(strExt==".bmp" || strExt==".ico" || strExt==".gif" || strExt==".rtf" || strExt==".css")
					bCopyOnly = true;
				wizard.RenderTemplate(strTemplate, strFile, bCopyOnly);

				// don't add these files to the project
				if(strTarget == strProjectName + ".h" ||
				   strTarget == strProjectName + "ps.mk" ||
				   strTarget == strProjectName + "ps.def")
					continue;

				proj.Object.AddFile(strFile);
			}
		}
		strTextStream.Close();
	}
	catch(e)
	{
		throw e;
	}
}
