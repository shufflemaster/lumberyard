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
#include "Vegetation_precompiled.h"

#include <AzTest/AzTest.h>
#include <AzCore/UnitTest/TestTypes.h>
#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Asset/AssetManagerComponent.h>
#include <AzCore/Asset/AssetManager.h>
#include <AzCore/Memory/PoolAllocator.h>
#include <AzCore/Memory/SystemAllocator.h>

//////////////////////////////////////////////////////////////////////////

#include <Vegetation/Ebuses/AreaSystemRequestBus.h>
#include <VegetationModule.h>
#include <AreaSystemComponent.h>

namespace UnitTest
{
    // This component meets all the dependencies required to get the Vegetation system activated:
    // - Provides SurfaceData services
    // - Starts / stops the Asset Manager
    // Note that this will always start before the vegetation components and end after them due
    // to the dependency-enforced ordering.
    class MockVegetationDependenciesComponent
        : public AZ::Component
    {
    public:
        AZ_COMPONENT(MockVegetationDependenciesComponent, "{C93FAEE8-E0C3-41E6-BBD1-89023C5ACB28}");

        static void Reflect(AZ::ReflectContext* context)
        {
            if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
            {
                serialize->Class<MockVegetationDependenciesComponent, AZ::Component>()->Version(0);
            }
        }

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("SurfaceDataSystemService", 0x1d44d25f));
            provided.push_back(AZ_CRC("SurfaceDataProviderService", 0xfe9fb95e));
        }
        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible) {}
        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required) {}
        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent) {}

    protected:
        ////////////////////////////////////////////////////////////////////////
        // AZ::Component interface implementation
        void Init() override {}
        void Activate() override
        {
            AZ::AllocatorInstance<AZ::PoolAllocator>::Create();
            AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Create();

            AZ::Data::AssetManager::Descriptor descriptor;
            descriptor.m_maxWorkerThreads = 1;
            AZ::Data::AssetManager::Create(descriptor);
        }

        void Deactivate() override
        {
            AZ::Data::AssetManager::Destroy();

            AZ::AllocatorInstance<AZ::ThreadPoolAllocator>::Destroy();
            AZ::AllocatorInstance<AZ::PoolAllocator>::Destroy();
        }
    };

    // Create a mock module to load our mock component that meets all the vegetation system dependencies.
    class MockVegetationDependenciesModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(MockVegetationDependenciesModule, "{3F7470AD-4FF9-48E6-8FFB-A5314F874F2B}", AZ::Module);
        AZ_CLASS_ALLOCATOR(MockVegetationDependenciesModule, AZ::SystemAllocator, 0);

        MockVegetationDependenciesModule()
        {
            m_descriptors.insert(m_descriptors.end(), {
                MockVegetationDependenciesComponent::CreateDescriptor()
                });
        }

        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<MockVegetationDependenciesComponent>()
            };
        }
    };

    // Test harness for the vegetation system that starts up / shuts down all the vegetation system components.
    class VegetationTestApp
        : public ::testing::Test
    {
    public:
        VegetationTestApp()
            : m_application()
            , m_systemEntity(nullptr)
        {
        }

        void SetUp() override
        {
            AZ::ComponentApplication::Descriptor appDesc;
            appDesc.m_memoryBlocksByteSize = 50 * 1024 * 1024;
            appDesc.m_recordingMode = AZ::Debug::AllocationRecords::RECORD_FULL;
            appDesc.m_stackRecordLevels = 20;

            AZ::ComponentApplication::StartupParameters appStartup;
            appStartup.m_createStaticModulesCallback =
                [](AZStd::vector<AZ::Module*>& modules)
            {
                modules.emplace_back(new MockVegetationDependenciesModule);
                modules.emplace_back(new Vegetation::VegetationModule);
            };

            m_systemEntity = m_application.Create(appDesc, appStartup);
            m_systemEntity->Init();
            m_systemEntity->Activate();
        }

        void TearDown() override
        {
            m_systemEntity->Deactivate();
            m_application.Destroy();
        }

        AZ::ComponentApplication m_application;
        AZ::Entity* m_systemEntity;

    };

    TEST_F(VegetationTestApp, Vegetation_AreaComponentTest_SuccessfulActivation)
    {
        // This test simply creates an environment that activates and deactivates the vegetation system components.
        // If it runs without asserting / crashing, then it is successful.
    }
}

