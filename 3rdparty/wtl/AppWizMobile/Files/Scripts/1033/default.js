// Windows Template Library - WTL version 9.10
// Copyright (C) Microsoft Corporation, WTL Team. All rights reserved.
//
// This file is a part of the Windows Template Library.
// The use and distribution terms for this software are covered by the
// Microsoft Public License (http://opensource.org/licenses/MS-PL)
// which can be found in the file MS-PL.txt at the root folder.


var ProjWiz;
var WizardVersion = wizard.FindSymbol('WIZARD_VERSION');

if (WizardVersion >= 9.0)
    ProjWiz = new ActiveXObject("ProjWiz.SDProjWiz2.3");
else
    ProjWiz = new ActiveXObject("ProjWiz.SDProjWiz2.2");

function OnFinish(selProj, selObj)
{
	try
	{
		var strProjectPath = wizard.FindSymbol('PROJECT_PATH');
		var strProjectName = wizard.FindSymbol('PROJECT_NAME');

		// Create symbols based on the project name
		var strSafeProjectName = CreateSafeName(strProjectName);
		var strProjectRoot = strSafeProjectName.substr(0, 1).toUpperCase() + strSafeProjectName.substr(1);
		wizard.AddSymbol("SAFE_PROJECT_NAME", strSafeProjectName);
		wizard.AddSymbol("NICE_SAFE_PROJECT_NAME", strProjectRoot);
		wizard.AddSymbol("UPPERCASE_SAFE_PROJECT_NAME", strSafeProjectName.toUpperCase());

		// Set current year symbol
		var d = new Date();
		var nYear = d.getFullYear();
		if(nYear >= 2006)
			wizard.AddSymbol("WTL_CURRENT_YEAR", nYear);

		var strBaseName = wizard.FindSymbol("NICE_SAFE_PROJECT_NAME");
		// Set app type symbols
		if(wizard.FindSymbol("WTL_APPTYPE_SDI"))
		{
			var strFrameFile = strBaseName + "Frame";
			var strFrameClass = "C" + strFrameFile;
			wizard.AddSymbol("WTL_APPWND_FILE", strFrameFile);
			wizard.AddSymbol("WTL_APPWND_CLASS", strFrameClass);
			wizard.AddSymbol("WTL_FRAME_CLASS",strFrameClass);
			// Set view symbols
			if(wizard.FindSymbol("WTL_USE_VIEW"))
				SetViewSymbols(strBaseName);
		}
		else if(wizard.FindSymbol("WTL_APPTYPE_DLG"))
		{
			var strDlgFile = strBaseName + "Dialog";
			wizard.AddSymbol("WTL_APPWND_FILE", strDlgFile);
			wizard.AddSymbol("WTL_MAINDLG_CLASS", "C" + strDlgFile);
			wizard.AddSymbol("WTL_APPWND_CLASS", "C" + strDlgFile);
			
			var BaseClassName = "CAppStdDialogImpl";
			if(wizard.FindSymbol("WTL_APP_DLG_ORIENT_AWARE"))
			{
				if(wizard.FindSymbol("WTL_APP_DLG_RESIZE"))
					BaseClassName = "CAppStdDialogResizeImpl";
				else if(wizard.FindSymbol("WTL_APP_DLG_ORIENT"))
					BaseClassName = "CAppStdOrientedDialogImpl";
			}
					
			if(wizard.FindSymbol("WTL_ENABLE_AX"))
				BaseClassName = BaseClassName.replace("Std", "StdAx");

			wizard.AddSymbol("WTL_MAINDLG_BASE_CLASS", BaseClassName);
				
			wizard.AddSymbol("WTL_USE_STATUSBAR", false);
			wizard.AddSymbol("WTL_USE_VIEW", false);
		}


		EnsureDevicePlatforms();

		// this call must occur before we create the project, because otherwise the templates are already generated.
		SetDeviceSymbolsForPlatforms();
		if(wizard.FindSymbol("POCKETPC2003_UI_MODEL") && wizard.FindSymbol("SMARTPHONE2003_UI_MODEL"))
			wizard.AddSymbol("DUAL_UI_MODEL", true);
		
		// Create project and configurations
		selProj = CreateDeviceProject(strProjectName, strProjectPath);
		AddConfigurations(selProj, strProjectName);
		AddFilters(selProj);

		// Create the project's Templates.inf file
		var InfFile = CreateInfFile(); // common.js

		// Add the files in Templates.inf to the project.
		AddFilesToCustomProj(selProj, strProjectName, strProjectPath, InfFile);

		SetCommonPchSettings(selProj);

		SetWtlDeviceResourceConfigurations(selProj.Object);
		selProj.Object.Save();
		InfFile.Delete();

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

function CreateCustomProject(strProjectName, strProjectPath)
{
	try
	{
		var strProjTemplatePath = wizard.FindSymbol('PROJECT_TEMPLATE_PATH');
		var strProjTemplate = '';
		strProjTemplate = strProjTemplatePath + '\\default.vcproj';

		var Solution = dte.Solution;
		var strSolutionName = "";
		if (wizard.FindSymbol("CLOSE_SOLUTION"))
		{
			Solution.Close();
			strSolutionName = wizard.FindSymbol("VS_SOLUTION_NAME");
			if (strSolutionName.length)
			{
				var strSolutionPath = strProjectPath.substr(0, strProjectPath.length - strProjectName.length);
				Solution.Create(strSolutionPath, strSolutionName);
			}
		}

		var strProjectNameWithExt = '';
		strProjectNameWithExt = strProjectName + '.vcproj';

		var oTarget = wizard.FindSymbol("TARGET");
		var prj;
		if (wizard.FindSymbol("WIZARD_TYPE") == vsWizardAddSubProject)  // vsWizardAddSubProject
		{
			var prjItem = oTarget.AddFromTemplate(strProjTemplate, strProjectNameWithExt);
			prj = prjItem.SubProject;
		}
		else
		{
			prj = oTarget.AddFromTemplate(strProjTemplate, strProjectPath, strProjectNameWithExt);
		}
		return prj;
	}
	catch(e)
	{
		throw e;
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

//			var DeployTool = config.DeploymentTool;
//			if(DeployTool)
//			{
//				DeployTool.AdditionalFiles += "atl80.dll|$(VCInstallDir)ce\\dll\\$(INSTRUCTIONSET)\\|%CSIDL_WINDOWS%|0;";
//			}

			// Resource settings
			var RCTool = config.Tools("VCResourceCompilerTool");
			RCTool.Culture = rcEnglishUS;
			RCTool.AdditionalIncludeDirectories = "$(IntDir)";
			if(bDebug)
				strDefines = "_DEBUG";
			else
				strDefines = "NDEBUG";
			strDefines += ";_UNICODE;UNICODE;_WIN32_WCE=$(CEVER);SHELLSDK_MODULES_AYGSHELL"
			RCTool.PreprocessorDefinitions = strDefines;
			RCTool.AdditionalOptions = "-n"; // null-terminated string resources

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
		if (fso.FileExists(strWizTempFile))
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
		var strTempFolder = tfolder.Drive + '\\' + tfolder.Name;

		var strWizTempFile = strTempFolder + "\\" + fso.GetTempName();

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
			if(strName == "root.ico")
			{
				strTarget = strResPath + strProjectName + strName.substr(4, nNameLen - 4);
			}
			else
			{
				strTarget = strProjectName + strName.substr(4, nNameLen - 4);
			}
		}
		else if(strName == 'frame.h' || strName == 'MainDlg.h')
		{
			strTarget = wizard.FindSymbol("WTL_APPWND_FILE") +'.h';
		}
		else if(strName == 'frame.cpp' || strName == 'MainDlg.cpp')
		{
			strTarget = wizard.FindSymbol("WTL_APPWND_FILE") +'.cpp';
		}
		else if(strName == 'view.h')
		{
			strTarget = wizard.FindSymbol("WTL_VIEW_FILE") + '.h';
		}
		else if(strName == 'view.cpp')
		{
			strTarget = wizard.FindSymbol("WTL_VIEW_FILE") + '.cpp';
		}
		else if(strName == 'toolbar.bmp')
		{
			strTarget = strResPath + strName;
		}
		else
		{
			strTarget = strName;
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
			if (strTpl != '')
			{
				strName = strTpl;
				var strTarget = GetTargetName(strName, strProjectName);
				var strTemplate = strTemplatePath + '\\' + strTpl;
				var strFile = strProjectPath + '\\' + strTarget;
				
				var reCopyOnly = /\.(:?bmp|ico|gif|rtf|css)$/i;
				var bCopyOnly = reCopyOnly.test(strFile); //"true" will only copy the file from strTemplate to strTarget without rendering/adding to the project
				
				if(strTpl.search(/(?:root\.rc2|resource\.h)/i) == -1)
				{
					wizard.RenderTemplate(strTemplate, strFile, bCopyOnly);
					
					if(strTarget.search(/\.(?:h|cpp)$/) != -1)
					{
						SetPlatformsCode(strFile);
						if(wizard.FindSymbol('WTL_USE_CPP_FILES') && strTarget.search(/\.cpp$/) != -1)
							SplitCode(strFile);
					}
					proj.Object.AddFile(strFile);
				}
				else
				{
					SplitResourceFile(proj, strTemplate, strFile);
				}
			}
		}
		strTextStream.Close();
	}
	catch(e)
	{
		throw e;
	}
}

function AddPlatformFile(proj, strTemplate, PlatFormFile)
{
	wizard.RenderTemplate(strTemplate, PlatFormFile);
	proj.Object.AddFile(PlatFormFile);
}

/******************************************************************************
	Description: Process and rename a resource file depending on the project
				 selected platforms
******************************************************************************/

function SplitResourceFile(proj, strTemplate, strFile)
{
	var dotPos = strFile.lastIndexOf(".");
	var fileExt = strFile.substring(dotPos, strFile.length);
	var fileName = strFile.substr(0, dotPos);

	var ppcFile = fileName + "ppc" + fileExt;
	var spFile = fileName + "sp" + fileExt;
	
	if(wizard.FindSymbol("POCKETPC2003_UI_MODEL"))
		AddPlatformFile(proj, strTemplate, ppcFile);
		
	if(wizard.FindSymbol("SMARTPHONE2003_UI_MODEL"))
		AddPlatformFile(proj, strTemplate, spFile);
		
	if(wizard.FindSymbol("DUAL_UI_MODEL"))
	{
		wizard.AddSymbol("DUAL_UI_MODEL", false);
		SetPlatformsCode(ppcFile);
		wizard.AddSymbol("POCKETPC2003_UI_MODEL", false);
		SetPlatformsCode(spFile);
		wizard.AddSymbol("POCKETPC2003_UI_MODEL", true);
		wizard.AddSymbol("DUAL_UI_MODEL", true);
	}
	else
	{
		SetPlatformsCode(wizard.FindSymbol("POCKETPC2003_UI_MODEL") ? ppcFile : spFile);
	}
}

/******************************************************************************
	Description: Add the function members body to a .cpp file and remove them
				 from the matching header. Do nothing if no matching .h file.
		cppPath: Path name to a .cpp file
******************************************************************************/

function SplitCode(cppPath)
{
	var fso = new ActiveXObject('Scripting.FileSystemObject');
	var hPath = cppPath.replace(/.cpp$/, ".h");
	if (!fso.FileExists(hPath))
		return;
		
	var cppFile = fso.GetFile(cppPath);
	var hFile = fso.GetFile(hPath);
	var tsh = hFile.OpenAsTextStream(1);
	var hText = tsh.ReadAll();
	tsh.Close();
	var tscpp = cppFile.OpenAsTextStream(8);
	
	var ClassPattern = /^[ \t]*class[ \t]+(\w+)[\s\S]+?(?:^[ \t]*\};$)/mg
	var ClassInfo = hText.match(ClassPattern);
	var numClass = ClassInfo.length;
	for (nc = 0 ; nc < numClass; nc++)
	{
		var ClassText = ClassInfo[nc];
		ClassText.match(ClassPattern);
		var ClassName = RegExp.$1;
		
		var FnPattern = /(^(?:[ \t]+\w+)*[ \t]+(\w+)\([^\)]*\))([\s]*\{([^\}]*\{[^\}]*\})*[^\}]*\}+?)/mg
		var FnPatternIf = /(^(?:#if.+\s+)*(?:[ \t]+\w+)*[ \t]+(\w+)\([^\)]*\))([\s]*\{([^\}]*\{[^\}]*\})*[^\}]*\}+?(?:\s+#e(?:ndif|lif|lse)[^\r\n]*)*)/mg
		var FnInfo = ClassText.match(FnPatternIf);
		var numFn = FnInfo.length;
		for (n = 0 ; n < numFn; n++)
		{
			var FnTextIf = FnInfo[n];
			var FnTextIf = FnTextIf.match(FnPatternIf);
			var FnDef = RegExp.$1;
			var FnName = RegExp.$2;
			var FnBody = RegExp.$3;
			
			var FnFullName = ClassName + "::" + FnName;
			FnDef = FnDef.replace(FnName, FnFullName);
			FnDef = FnDef.replace(/^\t(?:\s*virtual\s+)*/, "");
			FnBody = FnBody.replace(/^\t/mg, "");
			
			tscpp.Write(FnDef);
			tscpp.Write(FnBody);
			tscpp.WriteBlankLines(2);
			
			var FnText = FnTextIf.input.match(FnPattern);
			var FnDecl = RegExp.$1 + ";";
			ClassText = ClassText.replace(FnText, FnDecl);
		}
		hText = hText.replace(ClassInfo[nc], ClassText);
	}
	tscpp.Close();
	tsh = hFile.OpenAsTextStream(2);
	tsh.Write(hText);
	tsh.Close();
}


/******************************************************************************
Description: Process the platform dependant code depending on the project
             selected platforms:
                            //?ppc or //?sp
                            Platform dependant code
                            //?sp or //?ppc Optional alternative
                            Optional alternate platform code
                            //?end
	FileName: Path name to a file
******************************************************************************/

function SetPlatformsCode(FileName)
{
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	var f = fso.GetFile(FileName);
	var ts = f.OpenAsTextStream();
	var text = ts.ReadAll();
	ts.Close();
	
	var PlatformPattern = /(?:\r\n)?(^\/{2}\?(?:ppc|sp)\r\n)(?:^.*\r\n)+?(\/{2}\?(?:ppc|sp)\r\n)?(?:^.*\r\n)*?(^\/{2}\?end\r\n)/m
	var ppcPattern = /^\/{2}\?ppc\r\n/m   
	var spPattern  = /^\/{2}\?sp\r\n/m  
	var endPattern =/^\/{2}\?end\r\n/m 

	var PlatformInfo = text.match(PlatformPattern);
	while (PlatformInfo != null)
	{
		var PlatformText = PlatformInfo[0];
		var type;
		if(RegExp.$2.length)
		{
			if(RegExp.$1 == "//?ppc\r\n")
				type = "ppcDual";
			else
				type = "spDual";
		}
		else
		{
			if(RegExp.$1 == "//?ppc\r\n")
				type = "ppcSingle";
			else
				type = "spSingle";
		}
				
		if(wizard.FindSymbol("DUAL_UI_MODEL"))
		{
			switch (type)
			{
			case "ppcDual":
				PlatformText = PlatformText.replace("//?sp", "#else");
			case "ppcSingle":
				PlatformText = PlatformText.replace("//?ppc", "#ifdef WIN32_PLATFORM_PSPC");
				break;
			case "spDual":
				PlatformText = PlatformText.replace("//?ppc", "#else");
			case "spSingle":
				PlatformText = PlatformText.replace("//?sp", "#ifdef WIN32_PLATFORM_WFSP");
				break;
			}
			PlatformText = PlatformText.replace("//?end", "#endif");
		}
		else if(wizard.FindSymbol("POCKETPC2003_UI_MODEL"))
		{
			switch (type)
			{
			case "ppcDual":
				PlatformText = PlatformText.replace(/\/\/\?sp[\s\S]*\r\n/, "");
			case "spDual":
				PlatformText = PlatformText.replace(/(?:\r\n)?\/\/\?sp[\s\S]*\/\/\?ppc/, "");
			case "ppcSingle":
				PlatformText = PlatformText.replace(ppcPattern, "");
				PlatformText = PlatformText.replace(endPattern, "");
				break;
			case "spSingle":
				PlatformText = "";
			}
		}
		else // if(wizard.FindSymbol("SMARTPHONE2003_UI_MODEL"))
		{
			switch (type)
			{
			case "spDual":
				PlatformText = PlatformText.replace(/\/\/\?ppc[\s\S]*\r\n/, "");
			case "ppcDual":
				PlatformText = PlatformText.replace(/(?:\r\n)?\/\/\?ppc[\s\S]*\/\/\?sp/, "");
			case "spSingle":
				PlatformText = PlatformText.replace(spPattern, "");
				PlatformText = PlatformText.replace(endPattern, "");
				break;
			case "ppcSingle":
				PlatformText = "";
			}
		}
		text = text.replace(PlatformInfo[0], PlatformText);
		PlatformInfo = text.match(PlatformPattern);
	}
	ts = fso.CreateTextFile(FileName);
	ts.Write(text);
	ts.Close();
}

function SetViewSymbols(strBaseName)
{
	var strViewClass;

	if(wizard.FindSymbol("WTL_USE_VIEW"))
	{
		var strViewFile = strBaseName + "View";
		wizard.AddSymbol("WTL_VIEW_FILE", strViewFile);

		strViewClass = "C" + strViewFile;
	}

	wizard.AddSymbol("WTL_VIEWTYPE_GENERIC", false);
	wizard.AddSymbol("WTL_VIEWTYPE_AX", false);
	wizard.AddSymbol("WTL_VIEWTYPE_FORM", false);
	wizard.AddSymbol("WTL_VIEWTYPE_PROPSHEET", false);
	var strView = wizard.FindSymbol("WTL_COMBO_VIEW_TYPE");
	switch(strView)
	{
	case "WTL_VIEWTYPE_FORM":
		wizard.AddSymbol("WTL_USE_VIEW_CLASS", true);
		wizard.AddSymbol("WTL_VIEWTYPE_FORM", true);
		if(wizard.FindSymbol("WTL_ENABLE_AX") && wizard.FindSymbol("WTL_HOST_AX"))
			wizard.AddSymbol("WTL_VIEW_BASE_CLASS", "CAxDialogImpl");
		else
			wizard.AddSymbol("WTL_VIEW_BASE_CLASS", "CDialogImpl");
		break;
	case "WTL_VIEWTYPE_PROPSHEET":
		wizard.AddSymbol("WTL_USE_VIEW_CLASS", true);
		wizard.AddSymbol("WTL_VIEWTYPE_PROPSHEET", true);
		wizard.AddSymbol("WTL_VIEW_BASE_CLASS", "CPropertySheetImpl");
		if(wizard.FindSymbol("WTL_ENABLE_AX") && wizard.FindSymbol("WTL_HOST_AX"))
			wizard.AddSymbol("WTL_PROPPAGE_BASE_CLASS", "CAxPropertyPageImpl");
		else
			wizard.AddSymbol("WTL_PROPPAGE_BASE_CLASS", "CPropertyPageImpl");
		break;
	case "WTL_VIEWTYPE_AX":
		wizard.AddSymbol("WTL_HOST_AX", true);
		wizard.AddSymbol("WTL_VIEWTYPE_AX", true);
		if(wizard.FindSymbol("WTL_USE_VIEW_CLASS"))
			wizard.AddSymbol("WTL_VIEW_BASE", "CAxWindow");
		else
			strViewClass = "CAxWindow";
		break;
	case "WTL_VIEWTYPE_EDIT":
		wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL | WS_VSCROLL | ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL");
		if(wizard.FindSymbol("WTL_USE_VIEW_CLASS"))
		{
			wizard.AddSymbol("WTL_VIEWTYPE_EDIT", true);
			wizard.AddSymbol("WTL_VIEW_BASE", "CEdit");
		}
		else
		{
			strViewClass = "CEdit";
		}
		break;
	case "WTL_VIEWTYPE_LISTVIEW":
		wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | LVS_REPORT | LVS_SHOWSELALWAYS");
		if(wizard.FindSymbol("WTL_USE_VIEW_CLASS"))
		{
			wizard.AddSymbol("WTL_VIEWTYPE_LISTVIEW", true);
			wizard.AddSymbol("WTL_VIEW_BASE", "CListViewCtrl");
		}
		else
		{
			strViewClass = "CListViewCtrl";
		}
		break;
	case "WTL_VIEWTYPE_TREEVIEW":
		wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS");
		if(wizard.FindSymbol("WTL_USE_VIEW_CLASS"))
		{
			wizard.AddSymbol("WTL_VIEWTYPE_TREEVIEW", true);
			wizard.AddSymbol("WTL_VIEW_BASE", "CTreeViewCtrl");
		}
		else
		{
			strViewClass = "CTreeViewCtrl";
		}
		break;
	case "WTL_VIEWTYPE_HTML":
		wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN  | HS_CONTEXTMENU");
		if(wizard.FindSymbol("WTL_USE_VIEW_CLASS"))
		{
			wizard.AddSymbol("WTL_VIEWTYPE_HTML", true);
			wizard.AddSymbol("WTL_VIEW_BASE", "CHtmlCtrl");
		}
		else
		{
			strViewClass = "CHtmlCtrl";
		}
		break;
	case "WTL_VIEWTYPE_INKX":
		wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN  | IS_BOTTOMVOICEBAR");
		if(wizard.FindSymbol("WTL_USE_VIEW_CLASS"))
		{
			wizard.AddSymbol("WTL_VIEWTYPE_INKX", true);
			wizard.AddSymbol("WTL_VIEW_BASE", "CInkXCtrl");
		}
		else
		{
			strViewClass = "CInkXCtrl";
		}
		break;
	case "WTL_VIEWTYPE_RICHINK":
		wizard.AddSymbol("WTL_VIEW_STYLES", "WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN  | IS_BOTTOMVOICEBAR");
		if(wizard.FindSymbol("WTL_USE_VIEW_CLASS"))
		{
			wizard.AddSymbol("WTL_VIEWTYPE_RICHINK", true);
			wizard.AddSymbol("WTL_VIEW_BASE", "CRichInkCtrl");
		}
		else
		{
			strViewClass = "CRichInkCtrl";
		}
		break;
	default:
		wizard.AddSymbol("WTL_USE_VIEW_CLASS", true);
		wizard.AddSymbol("WTL_VIEWTYPE_GENERIC", true);

		if(wizard.FindSymbol("WTL_VIEW_SCROLL"))
		{
			if(wizard.FindSymbol("WTL_VIEW_ZOOM"))
				wizard.AddSymbol("WTL_SCROLL_CLASS", "CZoomScrollImpl");
			else
				wizard.AddSymbol("WTL_SCROLL_CLASS", "CScrollImpl");
		}

		break;
	}

	wizard.AddSymbol("WTL_VIEW_CLASS", strViewClass);
}

function SetWtlDeviceResourceConfigurations(selProj)
{
	var projectName = wizard.FindSymbol("PROJECT_NAME");
	projectName = projectName.toLowerCase();
	for (var fileIdx = 1; fileIdx <= selProj.Files.Count; fileIdx++)
	{
		var file = selProj.Files.Item(fileIdx);
		if (file.Extension.toLowerCase() == ".rc")
		{
			for (var fileCfgIdx = 1; fileCfgIdx <= file.FileConfigurations.Count; fileCfgIdx++)
			{
				var fileCfg = file.FileConfigurations.Item(fileCfgIdx);
				var cfgName = fileCfg.Name;
				var fileName = file.Name.toLowerCase();
				var cfg;

				for (var i = 1; i <= selProj.Configurations.Count; i++)
				{
					if (selProj.Configurations.Item(i).Name == cfgName)
					{
						// this condition will ALWAYS be hit eventually, and
						// cfg will always be assigned by the end of the loop
						cfg = selProj.Configurations.Item(i);
						break;
					}
				}

				var platformName = cfg.Platform.Name;
				var symbol = ProjWiz.GetBaseNativePlatformProperty(platformName, "UISymbol");
				var CLTool = cfg.Tools("VCCLCompilerTool");
				CLTool.PreprocessorDefinitions += ";" + symbol;
				if (symbol == "POCKETPC2003_UI_MODEL")
				{
					if (fileName == projectName + "ppc.rc")
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = false;
					}
					else
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = true;
					}
				}
				else if (symbol == "SMARTPHONE2003_UI_MODEL")
				{
					if (fileName == projectName + "sp.rc" || fileName == "resourcesp.h")
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = false;
					}
					else
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = true;
					}
				}
				else if (symbol == "AYGSHELL_UI_MODEL")
				{
					if (fileName == projectName + "ayg.rc")
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = false;
					}
					else
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = true;
					}
				}
				else if (symbol == "STANDARDSHELL_UI_MODEL")
				{
					if (fileName == projectName + ".rc")
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = false;
					}
					else
					{
						file.FileConfigurations.Item(fileCfgIdx).ExcludedFromBuild = true;
					}
				}
				else
				{
					//wizard.ReportError("Error: unknown UI model [" + symbol + "]");
				}
			}
		}
	}
	return true;
}
