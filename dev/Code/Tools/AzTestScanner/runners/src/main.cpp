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
#include <iostream>
#include <string>
#include <sstream>
#include <memory>

#define VERBOSE 1

#if AZ_TESTS_ENABLED
#   include <AzTest/AzTest.h>
#   include <AzTest/Platform.h>
#   include <AzCore/IO/SystemFile.h>
#else
#   define AZ_MAX_PATH_LEN 1024
#endif // AZ_TESTS_ENABLED

#if defined(AZ_PLATFORM_WINDOWS)
#    define getcwd _getcwd // stupid MSFT "deprecation" warning
#endif

namespace
{
    const int INCORRECT_USAGE = 101;
    const int LIB_NOT_FOUND = 102;
    const int SYMBOL_NOT_FOUND = 103;

    // note that MODULE_SKIPPED is not an error condition, but not 0 to indicate its not the
    // same as successfully running tests and finding them.
    const int MODULE_SKIPPED = 104;
    const char* INTEG_BOOTSTRAP = "AzTestIntegBootstrap";
}

#if AZ_TESTS_ENABLED
DECLARE_AZ_UNIT_TEST_MAIN()
#endif // AZ_TESTS_ENABLED

//! display proper usage of the application
void usage(AZ::Test::Platform& platform)
{
    std::stringstream ss;
    ss <<
        "AzTestRunner\n"
        "Runs AZ unit and integration tests. Exit code is the result from GoogleTest.\n"
        "\n"
        "Usage:\n"
        "   AzTestRunner.exe <lib> (AzRunUnitTests|AzRunIntegTests) [--integ] [--wait-for-debugger] [--pause-on-completion] [google-test-args]\n"
        "\n"
        "Options:\n"
        "   <lib>: the module to test\n"
        "   <hook>: the name of the aztest hook function to run in the <lib>\n"
        "           'AzRunUnitTests' will hook into unit tests\n"
        "           'AzRunIntegTests' will hook into integration tests\n"
        "   --integ: tells runner to bootstrap the engine, needed for integration tests\n"
        "            Note: you can run unit tests with a bootstrapped engine (AzRunUnitTests --integ),\n"
        "            but running integration tests without a bootstrapped engine (AzRunIntegTests w/ no --integ) might not work.\n"
        "   --wait-for-debugger: tells runner to wait for debugger to attach to process (on supported platforms)\n"
        "   --pause-on-completion: tells the runner to pause after running the tests\n"
        "   --suite: runs only tests marked with the given <suiteName>\n"
        "\n"
        "Example:\n"
        "   AzTestRunner.exe CrySystem.dll AzRunUnitTests --pause-on-completion\n"
        "   AzTestRunner.exe CrySystem.dll AzRunIntegTests --integ\n"
        "\n"
        "Exit Codes:\n"
        "   0 - all tests pass\n"
        "   1 - test failure\n"
        << "   " << INCORRECT_USAGE << " - incorrect usage (see above)\n"
        << "   " << LIB_NOT_FOUND << " - library/dll could not be loaded\n"
        << "   " << SYMBOL_NOT_FOUND << " - export symbol not found\n"
        << "   " << MODULE_SKIPPED << " - non-integ module was skipped (not an error)\n";

    std::cerr << ss.str() << std::endl;
}

//! attempt to run the int X() method exported by the specified library
int wrapped_main(int argc, char** argv)
{
#if AZ_TESTS_ENABLED
    INVOKE_AZ_UNIT_TEST_MAIN();
#endif

    AZ::Test::Platform& platform = AZ::Test::GetPlatform();

    if (argc < 3)
    {
        usage(platform);
        return INCORRECT_USAGE;
    }

#if VERBOSE
    char cwd[AZ_MAX_PATH_LEN] = { '\0' };
    getcwd(cwd, sizeof(cwd));
    std::cout << "cwd = " << cwd << std::endl;

    for (int i = 0; i < argc; i++)
    {
        std::cout << "arg[" << i << "] " << argv[i] << std::endl;
    }
#endif

    // capture positional arguments
    // [0] is the program name
    std::string lib = argv[1];
    std::string symbol = argv[2];

    // shift argv parameters down as we don't need lib or symbol anymore
    AZ::Test::RemoveParameters(argc, argv, 1, 2);

    // capture optional arguments
    bool waitForDebugger = false;
    bool isInteg = false;
    bool pauseOnCompletion = false;
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--wait-for-debugger") == 0)
        {
            waitForDebugger = true;
            AZ::Test::RemoveParameters(argc, argv, i, i);
            i--;
        }
        else if (strcmp(argv[i], "--integ") == 0)
        {
            isInteg = true;
            AZ::Test::RemoveParameters(argc, argv, i, i);
            i--;
        }
        else if (strcmp(argv[i], "--pause-on-completion") == 0)
        {
            pauseOnCompletion = true;
            AZ::Test::RemoveParameters(argc, argv, i, i);
            i--;
        }
    }

    std::cout << "LIB: " << lib << std::endl;

    // Wait for debugger
    if (waitForDebugger)
    {
        if (platform.SupportsWaitForDebugger())
        {
            std::cout << "Waiting for debugger..." << std::endl;
            platform.WaitForDebugger();
        }
        else
        {
            std::cerr << "Warning - platform does not support --wait-for-debugger feature" << std::endl;
        }
    }

    // make sure the module actually has the expected entry point before proceeding.
    // it is very expensive to start the bootstrapper.

    std::shared_ptr<AZ::Test::IFunctionHandle> testMainFunction;

    std::cout << "Loading: " << lib << std::endl;
    std::shared_ptr<AZ::Test::IModuleHandle> module = platform.GetModule(lib);

    int result = 0;
    if (module->IsValid())
    {
        std::cout << "OKAY Library loaded: " << lib << std::endl;

        testMainFunction = module->GetFunction(symbol);
        if (!testMainFunction->IsValid())
        {
            std::cerr << "FAILED to find symbol: " << symbol << std::endl;
            result = SYMBOL_NOT_FOUND;
        }
        else
        {
            std::cout << "OKAY Symbol found: " << symbol << std::endl;
        }
    }
    else
    {
        std::cerr << "FAILED to load library: " << lib << std::endl;
        result = LIB_NOT_FOUND;
    }

    // bail out early if the module does not contain the expected entry point or the module could not be loaded.
    if (result != 0)
    {
        module.reset();

        if ((isInteg) && (result == SYMBOL_NOT_FOUND))
        {
            // special case:  It is not required to put an INTEG test inside every DLL - so if
            // we failed to find the INTEG entry point in this DLL, its not an error.
            // its only an error if we find it and there are no tests, or we find it and tests actually
            // fail.
            std::cerr << "INTEG module has no entry point and will be skipped: " << lib << std::endl;
            return MODULE_SKIPPED;
        }

        return result;
    }

    platform.SuppressPopupWindows();

    // Grab a bootstrapper library if requested
    std::shared_ptr<AZ::Test::IModuleHandle> bootstrap;
    if (isInteg)
    {
        bootstrap = platform.GetModule(INTEG_BOOTSTRAP);
        if (!bootstrap->IsValid())
        {
            std::cerr << "FAILED to load bootstrapper" << std::endl;
            return LIB_NOT_FOUND;
        }

        // Initialize the bootstrapper
        auto init = bootstrap->GetFunction("Initialize");
        if (init->IsValid())
        {
            int initResult = (*init)();
            if (initResult != 0)
            {
                std::cerr << "Bootstrapper Initialize failed with code " << initResult << ", exiting" << std::endl;
                return initResult;
            }
        }
    }


    // run the test main function.
    if (testMainFunction->IsValid())
    {
        result = (*testMainFunction)(argc, argv);
        std::cout << "OKAY " << symbol << "() returned " << result << std::endl;
        testMainFunction.reset();
    }

    // unload and reset the module here, because it needs to release resources that were used / activated in
    // system allocator / etc.
    module.reset();

    // Shutdown the bootstrapper
    if (bootstrap)
    {
        auto shutdown = bootstrap->GetFunction("Shutdown");
        if (shutdown->IsValid())
        {
            int shutdownResult = (*shutdown)();
            if (shutdownResult != 0)
            {
                std::cerr << "Bootstrapper shutdown failed with code " << shutdownResult << ", exiting" << std::endl;
                return shutdownResult;
            }
        }
        bootstrap.reset();
    }

    if (pauseOnCompletion)
    {
        system("pause");
    }

    return result;
}

// wrapping the main allows you to guarantee that things are out of scope
// and also that any cleanup code will function.
int main(int argc, char** argv)
{
    int result = wrapped_main(argc, argv);
    std::cout << "Returning code: " << result << std::endl;
    return result;
}
