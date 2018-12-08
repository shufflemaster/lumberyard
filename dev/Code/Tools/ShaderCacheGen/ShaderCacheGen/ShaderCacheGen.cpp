/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "stdafx.h"

// Insert your headers here
#include <platform.h>
#include <ISystem.h>

#include <ShellAPI.h>
#include <StringUtils.h>
#include <platform_impl.h>

#include <ParseEngineConfig.h>

#include <AzFramework/Application/Application.h>

// We need shell api for Current Root Extraction.
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

#include <CryLibrary.h>
#include <IConsole.h>

struct COutputPrintSink
    : public IOutputPrintSink
{
    virtual void Print(const char* inszText)
    {
        printf("%s\n", inszText);
    }
};

void ClearPlatformCVars(ISystem* pISystem)
{
    pISystem->GetIConsole()->ExecuteString("r_ShadersDX11 = 0");
    pISystem->GetIConsole()->ExecuteString("r_ShadersOrbis = 0"); // ACCEPTED_USE
    pISystem->GetIConsole()->ExecuteString("r_ShadersDurango = 0"); // ACCEPTED_USE
    pISystem->GetIConsole()->ExecuteString("r_ShadersMETAL = 0");
    pISystem->GetIConsole()->ExecuteString("r_ShadersGL4 = 0");
    pISystem->GetIConsole()->ExecuteString("r_ShadersGLES3 = 0");
}

static AZ::EnvironmentVariable<IMemoryManager*> s_cryMemoryManager = nullptr;
HMODULE s_crySystemDll = nullptr;

void AcquireCryMemoryManager()
{
    using GetMemoryManagerInterfaceFunction = decltype(CryGetIMemoryManagerInterface)*;

    IMemoryManager* cryMemoryManager = nullptr;
    GetMemoryManagerInterfaceFunction getMemoryManager = nullptr;

    s_crySystemDll = CryLoadLibraryDefName("CrySystem");
    AZ_Assert(s_crySystemDll, "Unable to load CrySystem to resolve memory manager");
    getMemoryManager = reinterpret_cast<GetMemoryManagerInterfaceFunction>(CryGetProcAddress(s_crySystemDll, "CryGetIMemoryManagerInterface"));
    AZ_Assert(getMemoryManager, "Unable to resolve CryGetIMemoryInterface via CrySystem");
    getMemoryManager((void**)&cryMemoryManager);
    AZ_Assert(cryMemoryManager, "Unable to resolve CryMemoryManager");
    s_cryMemoryManager = AZ::Environment::CreateVariable<IMemoryManager*>("CryIMemoryManagerInterface", cryMemoryManager);
}

void ReleaseCryMemoryManager()
{
    s_cryMemoryManager.Reset();
    if (s_crySystemDll)
    {
        while (CryFreeLibrary(s_crySystemDll))
        {
            // keep freeing until free.
            // this is unfortunate, but CryMemoryManager currently in its internal impl grabs an extra handle to this and does not free it nor
            // does it have an interface to do so.
            // until we can rewrite the memory manager init and shutdown code, we need to do this here because we need crysystem GONE
            // before we attempt to destroy the memory managers which it uses.
            ;
        }
        s_crySystemDll = nullptr;
    }
}

// wrapped main so that it can be inside the lifetime of an AZCore Application in the real main()
// and all memory created on the stack here in main_wrapped can go out of scope before we stop the application
int main_wrapped(int argc, char* argv[])
{
    const char* commandLine = GetCommandLineA();

    HANDLE mutex = CreateMutex(NULL, TRUE, "CrytekApplication");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (CryStringUtils::stristr(commandLine, "-devmode") == 0)
        {
            MessageBox(0, "There is already a Crytek application running. Cannot start another one!", "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);
            return 1;
        }

        if (CryStringUtils::stristr(commandLine, "-noprompt") == 0)
        {
            if (MessageBox(GetDesktopWindow(), "There is already a Crytek application running\nDo you want to start another one?", "Too many apps", MB_YESNO) != IDYES)
            {
                return 1;
            }
        }
    }

    InitRootDir();

    PFNCREATESYSTEMINTERFACE CreateSystemInterface =
        (PFNCREATESYSTEMINTERFACE)::GetProcAddress(s_crySystemDll, "CreateSystemInterface");

    COutputPrintSink printSink;
    SSystemInitParams sip;
    CEngineConfig cfg;
    cfg.CopyToStartupParams(sip);

    sip.bShaderCacheGen = true;
    sip.bDedicatedServer = false;
    sip.bPreview = false;
    sip.bTestMode = false;
    sip.bMinimal = true;
    sip.bToolMode = true;
    sip.bSkipScriptSystem = true;
    sip.pSharedEnvironment = AZ::Environment::GetInstance();

    sip.pPrintSync = &printSink;
    cry_strcpy(sip.szSystemCmdLine, commandLine);

    // this actually detects our root as part of construction.
    // we will load an alternate bootstrap file so that this tool never turns on VFS.

    sip.sLogFileName = "@log@/ShaderCacheGen.log";
    sip.bSkipFont = true;

    ISystem* pISystem = CreateSystemInterface(sip);

    if (!pISystem)
    {
        string errorStr;
        errorStr.Format("CreateSystemInterface Failed");

        MessageBox(0, errorStr.c_str(), "Error", MB_OK | MB_DEFAULT_DESKTOP_ONLY);

        return false;
    }

    ////////////////////////////////////
    // Current command line options

    pISystem->ExecuteCommandLine();

    if (CryStringUtils::stristr(commandLine, "ShadersPlatform="))
    {
        ClearPlatformCVars(pISystem);

        if (CryStringUtils::stristr(commandLine, "ShadersPlatform=PC") != 0)
        {
            pISystem->GetIConsole()->ExecuteString("r_ShadersDX11 = 1");   // DX9 is dead, long live DX11
        }
        else if (CryStringUtils::stristr(commandLine, "ShadersPlatform=Durango") != 0) // ACCEPTED_USE
        {
            pISystem->GetIConsole()->ExecuteString("r_ShadersDurango = 1"); // ACCEPTED_USE
        }
        else if (CryStringUtils::stristr(commandLine, "ShadersPlatform=Orbis") != 0) // ACCEPTED_USE
        {
            pISystem->GetIConsole()->ExecuteString("r_ShadersOrbis = 1"); // ACCEPTED_USE
        }
        else if (CryStringUtils::stristr(commandLine, "ShadersPlatform=GL4") != 0)
        {
            pISystem->GetIConsole()->ExecuteString("r_ShadersGL4 = 1");
        }
        else if (CryStringUtils::stristr(commandLine, "ShadersPlatform=GLES3") != 0)
        {
            pISystem->GetIConsole()->ExecuteString("r_ShadersGLES3 = 1");
        }
        else if (CryStringUtils::stristr(commandLine, "ShadersPlatform=METAL") != 0)
        {
            pISystem->GetIConsole()->ExecuteString("r_ShadersMETAL = 1");
        }
    }

    if (CryStringUtils::stristr(commandLine, "BuildGlobalCache") != 0)
    {
        // to only compile shader explicitly listed in global list, call PrecacheShaderList
        if (CryStringUtils::stristr(commandLine, "NoCompile") != 0)
        {
            pISystem->GetIConsole()->ExecuteString("r_StatsShaderList");
        }
        else
        {
            pISystem->GetIConsole()->ExecuteString("r_PrecacheShaderList");
        }
    }
    else if (CryStringUtils::stristr(commandLine, "BuildLevelCache") != 0)
    {
        pISystem->GetIConsole()->ExecuteString("r_PrecacheShadersLevels");
    }

    ////////////////////////////////////
    // Deprecated command line options
    if (strstr(commandLine, "PrecacheShaderList") != 0)
    {
        pISystem->GetIConsole()->ExecuteString("r_PrecacheShaderList");
    }
    else if (strstr(commandLine, "StatsShaderList") != 0)
    {
        pISystem->GetIConsole()->ExecuteString("r_StatsShaderList");
    }
    else if (strstr(commandLine, "StatsShaders") != 0)
    {
        pISystem->GetIConsole()->ExecuteString("r_StatsShaders");
    }
    else if (strstr(commandLine, "PrecacheShadersLevels") != 0)
    {
        pISystem->GetIConsole()->ExecuteString("r_PrecacheShadersLevels");
    }
    else if (strstr(commandLine, "PrecacheShaders") != 0)
    {
        pISystem->GetIConsole()->ExecuteString("r_PrecacheShaders");
    }
    else if (strstr(commandLine, "MergeShaders") != 0)
    {
        pISystem->GetIConsole()->ExecuteString("r_MergeShaders");
    }

    if (pISystem)
    {
        pISystem->Release();
    }

    pISystem = NULL;

    CloseHandle(mutex);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    AZ::AllocatorInstance<AZ::SystemAllocator>::Create();
    AZ::AllocatorInstance<AZ::LegacyAllocator>::Create();
    AZ::AllocatorInstance<CryStringAllocator>::Create();

    AzFramework::Application app;
    AzFramework::Application::StartupParameters startupParams;
    AzFramework::Application::Descriptor descriptor;
    app.Start(descriptor, startupParams);
    AcquireCryMemoryManager();

    int returncode = main_wrapped(argc, argv);

    app.Stop();
    
    ReleaseCryMemoryManager();
    AZ::AllocatorInstance<CryStringAllocator>::Destroy();
    AZ::AllocatorInstance<AZ::LegacyAllocator>::Destroy();
    AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();
    return returncode;
}
