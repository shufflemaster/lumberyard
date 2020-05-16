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

#include <AzCore/Component/ComponentApplication.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzFramework/Application/Application.h>
#include <AzFramework/CommandLine/CommandRegistrationBus.h>

#include <AzQtComponents/Utilities/QtPluginPaths.h>
#include <QCoreApplication>

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzToolsFramework/API/EditorPythonConsoleBus.h>

#include <Source/PythonSystemComponent.h>
#include <Source/PythonReflectionComponent.h>
#include <Source/PythonMarshalComponent.h>

namespace UnitTest
{
    struct CommandRegistrationBusSupression
        : public AzFramework::CommandRegistrationBus::Handler
    {
        CommandRegistrationBusSupression()
        {
            BusConnect();
        }

        ~CommandRegistrationBusSupression()
        {
            BusDisconnect();
        }

        bool RegisterCommand(AZStd::string_view, AZStd::string_view, AZ::u32, AzFramework::CommandFunction) override
        {
            return true;
        }

        bool UnregisterCommand(AZStd::string_view) override
        {
            return true;
        }
    };

    struct PythonTestingFixture
        : public ::testing::Test
        , protected AzFramework::ApplicationRequests::Bus::Handler
    {
        class FileIOHelper
        {
        public:
            AZ::IO::LocalFileIO m_fileIO;
            AZ::IO::FileIOBase* m_prevFileIO;

            FileIOHelper()
            {
                m_prevFileIO = AZ::IO::FileIOBase::GetInstance();
                AZ::IO::FileIOBase::SetInstance(&m_fileIO);
            }

            ~FileIOHelper()
            {
                AZ::IO::FileIOBase::SetInstance(m_prevFileIO);
            }
        };

        void SetUp() override
        {
            // fetch the Engine Root folder
            {
                int argc = 0;
                char** argv = nullptr;
                QCoreApplication qtApp(argc, argv);
                azsnprintf(m_engineRoot, sizeof(m_engineRoot), AzQtComponents::FindEngineRootDir(nullptr).toLocal8Bit().data());
            }

            m_fileIOHelper = AZStd::make_unique<FileIOHelper>();
            m_fileIOHelper->m_fileIO.SetAlias("@devroot@", m_engineRoot);
            m_fileIOHelper->m_fileIO.SetAlias("@engroot@", m_engineRoot);

            AzFramework::Application::Descriptor appDesc;
            appDesc.m_enableDrilling = false;
            m_app.Create(appDesc);

            AzFramework::ApplicationRequests::Bus::Handler::BusConnect();
        }

        void TearDown() override
        {
            AzFramework::ApplicationRequests::Bus::Handler::BusDisconnect();

            m_commandRegistrationBusSupression.reset();
            m_fileIOHelper.reset();
            m_app.Destroy();
        }

        void SimulateEditorBecomingInitialized(bool useCommandRegistrationBusSupression = true)
        {
            if (useCommandRegistrationBusSupression)
            {
                m_commandRegistrationBusSupression = AZStd::make_unique<CommandRegistrationBusSupression>();
            }

            auto editorPythonEventsInterface = AZ::Interface<AzToolsFramework::EditorPythonEventsInterface>::Get();
            if (editorPythonEventsInterface)
            {
                editorPythonEventsInterface->StartPython();
            }
        }

        void RegisterComponentDescriptors()
        {
            m_app.RegisterComponentDescriptor(EditorPythonBindings::PythonSystemComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(EditorPythonBindings::PythonReflectionComponent::CreateDescriptor());
            m_app.RegisterComponentDescriptor(EditorPythonBindings::PythonMarshalComponent::CreateDescriptor());
        }

        void Activate(AZ::Entity& e)
        {
            e.CreateComponent<EditorPythonBindings::PythonSystemComponent>();
            e.CreateComponent<EditorPythonBindings::PythonReflectionComponent>();
            e.CreateComponent<EditorPythonBindings::PythonMarshalComponent>();
            e.Init();
            e.Activate();
        }

        //////////////////////////////////////////////////////////////////////////
        // AzFramework::ApplicationRequests::Bus::Handler
        // required pure virtual overrides
        void NormalizePath(AZStd::string& ) override {}
        void NormalizePathKeepCase(AZStd::string& ) override {}
        void QueryApplicationType(AzFramework::ApplicationTypeQuery& ) const override {}
        void CalculateBranchTokenForAppRoot(AZStd::string& ) const override {}
        // Gets the engine root path for testing
        const char* GetEngineRoot() const override { return m_engineRoot; }
        // Retrieves the app root path for testing
        const char* GetAppRoot() const override { return m_engineRoot; }

        AZ::ComponentApplication m_app;
        AZStd::unique_ptr<FileIOHelper> m_fileIOHelper;
        AZStd::unique_ptr<CommandRegistrationBusSupression> m_commandRegistrationBusSupression;
        char m_engineRoot[1024];
    };
}
