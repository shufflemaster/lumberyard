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
#pragma once

#include <AzCore/EBus/EBus.h>
#include <AzCore/Interface/Interface.h>

namespace AzToolsFramework
{
    //! Interface into the Python virtual machine's data
    class EditorPythonConsoleInterface
    {
    protected:
        EditorPythonConsoleInterface() = default;
        virtual ~EditorPythonConsoleInterface() = default;
        EditorPythonConsoleInterface(EditorPythonConsoleInterface&&) = delete;
        EditorPythonConsoleInterface& operator=(EditorPythonConsoleInterface&&) = delete;

    public:
        AZ_RTTI(EditorPythonConsoleInterface, "{CAE877C8-DAA8-4535-BCBF-F392EAA7CEA9}");

        //! Returns the known list of modules exported to Python
        virtual void GetModuleList(AZStd::vector<AZStd::string_view>& moduleList) const = 0;

        //! Returns the known list of global functions inside a Python module
        struct GlobalFunction
        {
            AZStd::string_view m_moduleName;
            AZStd::string_view m_functionName;
            AZStd::string_view m_description;
        };
        using GlobalFunctionCollection = AZStd::vector<GlobalFunction>;
        virtual void GetGlobalFunctionList(GlobalFunctionCollection& globalFunctionCollection) const = 0;
    };

    //! Interface to signal the phases for the Python virtual machine
    class EditorPythonEventsInterface
    {
    protected:
        EditorPythonEventsInterface() = default;
        virtual ~EditorPythonEventsInterface() = default;
        EditorPythonEventsInterface(EditorPythonEventsInterface&&) = delete;
        EditorPythonEventsInterface& operator=(EditorPythonEventsInterface&&) = delete;

    public:
        AZ_RTTI(EditorPythonEventsInterface, "{F50AE641-2C80-4E07-B4B3-7CB34FFAB393}");

        //! Signal the Python handler to start
        virtual bool StartPython() = 0;

        //! Signal the Python handler to stop
        virtual bool StopPython() = 0;

        //! Determines if the caller needs to wait for the Python VM to initialize (non-main thread only)
        virtual void WaitForInitialization() {}
    };

    //! A bus to handle post notifications to the console views of Python output
    class EditorPythonConsoleNotifications
        : public AZ::EBusTraits
    {
    public:
        //////////////////////////////////////////////////////////////////////////
        // EBusTraits overrides
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
        static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
        //////////////////////////////////////////////////////////////////////////

        //! post a normal message to the console
        virtual void OnTraceMessage(AZStd::string_view message) = 0;

        //! post an error message to the console
        virtual void OnErrorMessage(AZStd::string_view message) = 0;
    };
    using EditorPythonConsoleNotificationBus = AZ::EBus<EditorPythonConsoleNotifications>;
}

