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

#include <SceneAPI/SceneCore/Utilities/FileUtilities.h>
#include <SceneAPI/SceneCore/Utilities/Reporting.h>
#include <SceneAPI/SceneCore/Events/ExportProductList.h>

#include <SceneAPIExt/Rules/MetaDataRule.h>
#include <SceneAPIExt/Groups/IMotionGroup.h>
#include <RCExt/Motion/MotionGroupExporter.h>
#include <RCExt/ExportContexts.h>

#include <EMotionFX/Exporters/ExporterLib/Exporter/Exporter.h>
#include <EMotionFX/Source/SkeletalMotion.h>
#include <EMotionFX/Exporters/ExporterLib/Exporter/Exporter.h>
#include <EMotionFX/CommandSystem/Source/MetaData.h>

namespace EMotionFX
{
    namespace Pipeline
    {
        namespace SceneEvents = AZ::SceneAPI::Events;
        namespace SceneUtil = AZ::SceneAPI::Utilities;
        namespace SceneContainer = AZ::SceneAPI::Containers;
        namespace SceneDataTypes = AZ::SceneAPI::DataTypes;

        const AZStd::string MotionGroupExporter::s_fileExtension = "motion";

        MotionGroupExporter::MotionGroupExporter()
        {
            BindToCall(&MotionGroupExporter::ProcessContext);
        }

        void MotionGroupExporter::Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
            if (serializeContext)
            {
                serializeContext->Class<MotionGroupExporter, AZ::SceneAPI::SceneCore::ExportingComponent>()->Version(1);
            }
        }

        SceneEvents::ProcessingResult MotionGroupExporter::ProcessContext(MotionGroupExportContext& context) const
        {
            if (context.m_phase != AZ::RC::Phase::Filling)
            {
                return SceneEvents::ProcessingResult::Ignored;
            }

            const AZStd::string& groupName = context.m_group.GetName();
            AZStd::string filename = SceneUtil::FileUtilities::CreateOutputFileName(groupName, context.m_outputDirectory, s_fileExtension);
            if (filename.empty() || !SceneUtil::FileUtilities::EnsureTargetFolderExists(filename))
            {
                return SceneEvents::ProcessingResult::Failure;
            }

            EMotionFX::SkeletalMotion* motion = EMotionFX::SkeletalMotion::Create(groupName.c_str());
            if (!motion)
            {
                return SceneEvents::ProcessingResult::Failure;
            }
            motion->SetUnitType(MCore::Distance::UNITTYPE_METERS);

            SceneEvents::ProcessingResultCombiner result;

            const Group::IMotionGroup& motionGroup = context.m_group;
            MotionDataBuilderContext dataBuilderContext(context.m_scene, motionGroup, *motion, AZ::RC::Phase::Construction);
            result += SceneEvents::Process(dataBuilderContext);
            result += SceneEvents::Process<MotionDataBuilderContext>(dataBuilderContext, AZ::RC::Phase::Filling);
            result += SceneEvents::Process<MotionDataBuilderContext>(dataBuilderContext, AZ::RC::Phase::Finalizing);

            // Check if there is meta data and apply it to the motion.
            AZStd::vector<MCore::Command*> metaDataCommands;
            if (Rule::MetaDataRule::LoadMetaData(motionGroup, metaDataCommands))
            {
                if (!CommandSystem::MetaData::ApplyMetaDataOnMotion(motion, metaDataCommands))
                {
                    AZ_Error("EMotionFX", false, "Applying meta data to '%s' failed.", filename.c_str());
                }
            }

            ExporterLib::SaveSkeletalMotion(filename, motion, MCore::Endian::ENDIAN_LITTLE, false);
            static AZ::Data::AssetType emotionFXMotionAssetType("{00494B8E-7578-4BA2-8B28-272E90680787}"); // from MotionAsset.h in EMotionFX Gem
            context.m_products.AddProduct(AZStd::move(filename), context.m_group.GetId(), emotionFXMotionAssetType,
                AZStd::nullopt, AZStd::nullopt);

            // The motion object served the purpose of exporting motion and is no longer needed
            MCore::Destroy(motion);
            motion = nullptr;

            return result.GetResult();
        }
    } // namespace Pipeline
} // namespace EMotionFX
