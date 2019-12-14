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

#include <AzTest/GemTestEnvironment.h>
#include <AzCore/UnitTest/UnitTest.h>
#include <AzCore/Asset/AssetManagerComponent.h>
#include <AzCore/Jobs/JobManagerComponent.h>
#include <AzCore/Memory/MemoryComponent.h>

namespace AZ
{
    namespace Test
    {
        void GemTestApplication::SetComponentDescriptors(const AZStd::vector<AZ::ComponentDescriptor*>& componentDescriptors)
        {
            m_componentDescriptors = componentDescriptors;
        }

        void GemTestApplication::CreateReflectionManager()
        {
            AZ::ComponentApplication::CreateReflectionManager();
            for (AZ::ComponentDescriptor* descriptor : m_componentDescriptors)
            {
                RegisterComponentDescriptor(descriptor);
            }
        }

        GemTestEnvironment::Parameters::~Parameters()
        {
            m_componentDescriptors.clear();
            m_dynamicModulePaths.clear();
            m_requiredComponents.clear();
        }

        GemTestEnvironment::GemTestEnvironment()
        {
            m_parameters = new Parameters;
        }

        void GemTestEnvironment::AddDynamicModulePaths(const AZStd::vector<AZStd::string>& dynamicModulePaths)
        {
            m_parameters->m_dynamicModulePaths.insert(m_parameters->m_dynamicModulePaths.end(),
                dynamicModulePaths.begin(), dynamicModulePaths.end());
        }

        void GemTestEnvironment::AddComponentDescriptors(const AZStd::vector<AZ::ComponentDescriptor*>& componentDescriptors)
        {
            m_parameters->m_componentDescriptors.insert(m_parameters->m_componentDescriptors.end(),
                componentDescriptors.begin(), componentDescriptors.end());
        }

        void GemTestEnvironment::AddRequiredComponents(const AZStd::vector<AZ::TypeId>& requiredComponents)
        {
            m_parameters->m_requiredComponents.insert(m_parameters->m_requiredComponents.end(),
                requiredComponents.begin(), requiredComponents.end());
        }

        void GemTestEnvironment::SetupEnvironment()
        {
            UnitTest::TraceBusHook::SetupEnvironment();

            AZ::AllocatorInstance<AZ::SystemAllocator>::Create();

            AddGemsAndComponents();

            // Create the application.
            m_application = aznew GemTestApplication;
            AZ::ComponentApplication::Descriptor appDesc;
            appDesc.m_useExistingAllocator = true;
            appDesc.m_enableDrilling = false;
            m_application->SetComponentDescriptors(m_parameters->m_componentDescriptors);

            // Set up gems for loading.
            for (const AZStd::string& dynamicModulePath : m_parameters->m_dynamicModulePaths)
            {
                AZ::DynamicModuleDescriptor dynamicModuleDescriptor;
                dynamicModuleDescriptor.m_dynamicLibraryPath = dynamicModulePath;
                appDesc.m_modules.push_back(dynamicModuleDescriptor);
            }

            // Create a system entity.
            PreCreateApplication();
            m_systemEntity = m_application->Create(appDesc);
            PostCreateApplication();
            m_systemEntity->AddComponent(aznew AZ::MemoryComponent());
            m_systemEntity->AddComponent(aznew AZ::AssetManagerComponent());
            m_systemEntity->AddComponent(aznew AZ::JobManagerComponent());
            m_systemEntity->Init();
            m_systemEntity->Activate();

            // Create a separate entity in order to activate this gem's required components.  Note that this assumes
            // any component dependencies are already satisfied either by the system entity or the entities which were
            // created during the module loading above.  It therefore does not do a dependency sort or use the full
            // entity activation.
            m_gemEntity = aznew GemTestEntity();
            for (AZ::TypeId typeId : m_parameters->m_requiredComponents)
            {
                m_gemEntity->CreateComponent(typeId);
            }
            m_gemEntity->Init();
            for (AZ::Component* component : m_gemEntity->GetComponents())
            {
                m_gemEntity->ActivateComponent(*component);
            }
        }

        void GemTestEnvironment::TeardownEnvironment()
        {
            const AZ::Entity::ComponentArrayType& components = m_gemEntity->GetComponents();
            for (auto itComponent = components.rbegin(); itComponent != components.rend(); ++itComponent)
            {
                m_gemEntity->DeactivateComponent(**itComponent);
            }
            delete m_gemEntity;
            m_gemEntity = nullptr;

            PreDestroyApplication();
            m_application->Destroy();
            delete m_application;
            m_application = nullptr;
            PostDestroyApplication();

            delete m_parameters;
            m_parameters = nullptr;

            AZ::AllocatorInstance<AZ::SystemAllocator>::Destroy();

            UnitTest::TraceBusHook::TeardownEnvironment();
        }
    } // namespace Test
} // namespace AZ
