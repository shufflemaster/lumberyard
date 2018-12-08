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

#include <AzCore/Module/DynamicModuleHandle.h>
#include <AzCore/Serialization/EditContext.h>
#include <SceneAPI/SceneCore/DataTypes/GraphData/IAnimationData.h>
#include <Config/SettingsObjects/NodeSoftNameSetting.h>
#include <Config/SettingsObjects/FileSoftNameSetting.h>
#include <Config/Components/SceneProcessingConfigSystemComponent.h>

namespace AZ
{
    namespace SceneProcessing
    {
        extern AZStd::unique_ptr<AZ::DynamicModuleHandle> s_sceneCoreModule;
        extern AZStd::unique_ptr<AZ::DynamicModuleHandle> s_sceneDataModule;
        extern AZStd::unique_ptr<AZ::DynamicModuleHandle> s_fbxSceneBuilderModule;
    }

    namespace SceneProcessingConfig
    {
        void SceneProcessingConfigSystemComponentSerializationEvents::OnWriteBegin(void* classPtr)
        {
            SceneProcessingConfigSystemComponent* component = reinterpret_cast<SceneProcessingConfigSystemComponent*>(classPtr);
            component->Clear();
        }

        SceneProcessingConfigSystemComponent::SceneProcessingConfigSystemComponent()
        {
            using namespace AZ::SceneAPI::SceneCore;

            ActivateSceneModule(SceneProcessing::s_sceneCoreModule);
            ActivateSceneModule(SceneProcessing::s_sceneDataModule);
            ActivateSceneModule(SceneProcessing::s_fbxSceneBuilderModule);
            
            // Defaults in case there's no config setup in the Project Configurator.
            m_softNames.push_back(aznew NodeSoftNameSetting("_lod1", PatternMatcher::MatchApproach::PostFix, "LODMesh1", true));
            m_softNames.push_back(aznew NodeSoftNameSetting("_lod2", PatternMatcher::MatchApproach::PostFix, "LODMesh2", true));
            m_softNames.push_back(aznew NodeSoftNameSetting("_lod3", PatternMatcher::MatchApproach::PostFix, "LODMesh3", true));
            m_softNames.push_back(aznew NodeSoftNameSetting("_lod4", PatternMatcher::MatchApproach::PostFix, "LODMesh4", true));
            m_softNames.push_back(aznew NodeSoftNameSetting("_lod5", PatternMatcher::MatchApproach::PostFix, "LODMesh5", true));
            m_softNames.push_back(aznew NodeSoftNameSetting("_phys", PatternMatcher::MatchApproach::PostFix, "PhysicsMesh", true));
            m_softNames.push_back(aznew NodeSoftNameSetting("_ignore", PatternMatcher::MatchApproach::PostFix, "Ignore", false));
            // If the filename ends with "_anim" this will mark all nodes as "Ignore" unless they're derived from IAnimationData. This will
            // cause only animations to be exported from the .fbx file even if there's other data available.
            m_softNames.push_back(aznew FileSoftNameSetting("_anim", PatternMatcher::MatchApproach::PostFix, "Ignore", false,
                { FileSoftNameSetting::GraphType(SceneAPI::DataTypes::IAnimationData::TYPEINFO_Name()) }));

            m_UseCustomNormals = true;
        }

        void SceneProcessingConfigSystemComponent::Activate()
        {
            SceneProcessingConfigRequestBus::Handler::BusConnect();
            AZ::SceneAPI::Events::AssetImportRequestBus::Handler::BusConnect();
        }

        void SceneProcessingConfigSystemComponent::Deactivate()
        {
            AZ::SceneAPI::Events::AssetImportRequestBus::Handler::BusDisconnect();
            SceneProcessingConfigRequestBus::Handler::BusDisconnect();
        }

        SceneProcessingConfigSystemComponent::~SceneProcessingConfigSystemComponent()
        {
            DeactivateSceneModule(SceneProcessing::s_fbxSceneBuilderModule);
            DeactivateSceneModule(SceneProcessing::s_sceneDataModule);
            DeactivateSceneModule(SceneProcessing::s_sceneCoreModule);
        }

        void SceneProcessingConfigSystemComponent::Clear()
        {
            m_softNames.clear();
            m_softNames.shrink_to_fit();
            m_UseCustomNormals = true;
        }

        const AZStd::vector<SoftNameSetting*>* SceneProcessingConfigSystemComponent::GetSoftNames()
        {
            return &m_softNames;
        }

        void SceneProcessingConfigSystemComponent::AreCustomNormalsUsed(bool &value)
        {
            value = m_UseCustomNormals;
        }

        void SceneProcessingConfigSystemComponent::Reflect(AZ::ReflectContext* context)
        {
            ReflectSceneModule(context, SceneProcessing::s_sceneCoreModule);
            ReflectSceneModule(context, SceneProcessing::s_sceneDataModule);
            ReflectSceneModule(context, SceneProcessing::s_fbxSceneBuilderModule);

            SoftNameSetting::Reflect(context);
            NodeSoftNameSetting::Reflect(context);
            FileSoftNameSetting::Reflect(context);

            AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
            if (serialize)
            {
                serialize->Class<SceneProcessingConfigSystemComponent, AZ::Component>()
                    ->Version(2)
                    ->EventHandler<SceneProcessingConfigSystemComponentSerializationEvents>() 
                    ->Field("softNames", &SceneProcessingConfigSystemComponent::m_softNames)
                    ->Field("useCustomNormals", &SceneProcessingConfigSystemComponent::m_UseCustomNormals);

                if (AZ::EditContext* ec = serialize->GetEditContext())
                {
                    ec->Class<SceneProcessingConfigSystemComponent>("Scene Processing Config", "Use this component to fine tune the defaults for processing of scene files like Fbx.")
                        ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                            ->Attribute(AZ::Edit::Attributes::Category, "Assets")
                            ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("System", 0xc94d118b))
                            ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &SceneProcessingConfigSystemComponent::m_softNames,
                            "Soft naming conventions", "Update the naming conventions to suit your project.")
                            ->Attribute(AZ::Edit::Attributes::AutoExpand, false)
                        ->DataElement(AZ::Edit::UIHandlers::Default, &SceneProcessingConfigSystemComponent::m_UseCustomNormals,
                            "Use Custom Normals", "When enabled, Lumberyard will use the DCC assets custom or tangent space normals. When disabled, the normals will be averaged. This setting can be overridden on individual FBX asset settings.")
                            ->Attribute(AZ::Edit::Attributes::AutoExpand, false);
                }
            }
        }

        void SceneProcessingConfigSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("SceneProcessingConfigService", 0x7b333b47));
        }

        void SceneProcessingConfigSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            incompatible.push_back(AZ_CRC("SceneProcessingConfigService", 0x7b333b47));
        }

        void SceneProcessingConfigSystemComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            AZ_UNUSED(required);
        }

        void SceneProcessingConfigSystemComponent::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
        {
            AZ_UNUSED(dependent);
        }

        void SceneProcessingConfigSystemComponent::ReflectSceneModule(AZ::ReflectContext* context, 
            const AZStd::unique_ptr<AZ::DynamicModuleHandle>& module)
        {
            using ReflectFunc = void(*)(AZ::SerializeContext*);

            AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context);
            if (serialize)
            {
                if (module)
                {
                    ReflectFunc reflect = module->GetFunction<ReflectFunc>("Reflect");
                    if (reflect)
                    {
                        (*reflect)(serialize);
                    }
                }
            }
        }

        void SceneProcessingConfigSystemComponent::ActivateSceneModule(const AZStd::unique_ptr<AZ::DynamicModuleHandle>& module)
        {
            using ActivateFunc = void(*)();

            if (module)
            {
                ActivateFunc activate = module->GetFunction<ActivateFunc>("Activate");
                if (activate)
                {
                    (*activate)();
                }
            }
        }

        void SceneProcessingConfigSystemComponent::DeactivateSceneModule(const AZStd::unique_ptr<AZ::DynamicModuleHandle>& module)
        {
            using DeactivateFunc = void(*)();

            if (module)
            {
                DeactivateFunc deactivate = module->GetFunction<DeactivateFunc>("Deactivate");
                if (deactivate)
                {
                    (*deactivate)();
                }
            }
        }
    } // namespace SceneProcessingConfig
} // namespace AZ