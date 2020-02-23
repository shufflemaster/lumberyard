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

#include <platform_impl.h>
#include <IGem.h>
#include <AzCore/Module/DynamicModuleHandle.h>
#include <Behaviors/LoggingGroupBehavior.h>
#include <Processors/LoadingTrackingProcessor.h>
#include <Processors/ExportTrackingProcessor.h>

namespace SceneLoggingExample
{
    // The SceneLoggingExampleModule is the entry point for gems. To extend the SceneAPI, the  
    // logging, loading, and export components must be registered here.
    //
    // NOTE: The gem system currently does not support registering file extensions through the  
    // AssetImportRequest EBus.
    class SceneLoggingExampleModule
        : public CryHooksModule
    {
    public:
        AZ_RTTI(SceneLoggingExampleModule, "{36AA9C0F-7976-40C7-AF54-C492AC5B16F6}", CryHooksModule);

        SceneLoggingExampleModule()
            : CryHooksModule()
        {
            // The SceneAPI libraries require specialized initialization. As early as possible, be 
            // sure to repeat the following two lines for any SceneAPI you want to use. Omitting these 
            // calls or making them too late can cause problems such as missing EBus events.
#if !defined(SCENE_CORE_STATIC)
            m_sceneCoreModule = AZ::DynamicModuleHandle::Create("SceneCore");
            m_sceneCoreModule->Load(true);
#endif // !defined(SCENE_CORE_STATIC)
            m_descriptors.insert(m_descriptors.end(), 
            {
                LoggingGroupBehavior::CreateDescriptor(),
                LoadingTrackingProcessor::CreateDescriptor(),
                ExportTrackingProcessor::CreateDescriptor()
            });
        }

        // In this example, no system components are added. You can use system components 
        // to set global settings for this gem from the Project Configurator.
        // For functionality that should always be available to the SceneAPI, we recommend 
        // that you use a BehaviorComponent instead.
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList {};
        }

    private:
#if !defined(SCENE_CORE_STATIC)
        AZStd::unique_ptr<AZ::DynamicModuleHandle> m_sceneCoreModule;
#endif // !defined(SCENE_CORE_STATIC)
    };
} // namespace SceneLoggingExample

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM.
// The first parameter should be GemName_GemIdLower.
// The second should be the fully qualified name of the class above.
AZ_DECLARE_MODULE_CLASS(SceneLoggingExample_35d8f6e49ae04c9382c61a42d4355c2f, SceneLoggingExample::SceneLoggingExampleModule)
