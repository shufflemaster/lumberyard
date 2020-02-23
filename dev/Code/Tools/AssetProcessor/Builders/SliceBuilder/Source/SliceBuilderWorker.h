/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#pragma once

#include <AssetBuilderSDK/AssetBuilderBusses.h>
#include <AssetBuilderSDK/AssetBuilderSDK.h>
#include <AzCore/Slice/SliceAsset.h>
#include <AzCore/Component/ComponentExport.h>
#include <AzToolsFramework/Fingerprinting/TypeFingerprinter.h>
#include <AzCore/Serialization/DataPatchBus.h>

namespace SliceBuilder
{
    class SliceBuilderSettings
    {
    public:
        AZ_TYPE_INFO(SliceBuilderSettings, "{FB9075DA-10CA-452C-93FA-168A2EDA1EBD}");

        SliceBuilderSettings()
        {}

        bool m_enableSliceConversion = false;

        // Reflect our wrapped key and value types to serializeContext so that may later be used
        static void Reflect(AZ::ReflectContext* context)
        {
            AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);

            if (serializeContext)
            {
                serializeContext->Class<SliceBuilderSettings>()
                    ->Version(1)
                    ->Field("EnableSliceConversion", &SliceBuilderSettings::m_enableSliceConversion)
                    ;
            }
        }
    };

    class SliceBuilderWorker
        : public AssetBuilderSDK::AssetBuilderCommandBus::Handler
        , public AZ::DataPatchNotificationBus::Handler
    {
    public:
        AZ_CLASS_ALLOCATOR(SliceBuilderWorker, AZ::SystemAllocator, 0);

        SliceBuilderWorker();
        ~SliceBuilderWorker() override;

        //! Asset Builder Callback Functions
        void CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response);
        void ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response) const;

        bool GetDynamicSliceAssetAndDependencies(AZ::IO::GenericStream* stream, const char* fullPath, const AZ::PlatformTagSet& platformTags, AZ::Data::Asset<AZ::SliceAsset>& outSliceAsset, AZStd::vector<AssetBuilderSDK::ProductDependency>& outProductDependencies, AssetBuilderSDK::ProductPathDependencySet& productPathDependencySet) const;
        //////////////////////////////////////////////////////////////////////////
        //!AssetBuilderSDK::AssetBuilderCommandBus interface
        void ShutDown() override;
        //////////////////////////////////////////////////////////////////////////

        static AZ::Uuid GetUUID();

        //////////////////////////////////////////////////////////////////////////
        //!DataPatchNotificationBus interface
        //////////////////////////////////////////////////////////////////////////

        void OnLegacyDataPatchLoadFailed() override;
        void OnLegacyDataPatchLoaded() override;

        bool SliceUpgradesAllowed() const;

    private:
        bool m_isShuttingDown = false;
        AZStd::unique_ptr<AzToolsFramework::Fingerprinting::TypeFingerprinter> m_typeFingerprinter;

        bool m_sliceHasLegacyDataPatches = false;
        bool m_sliceDataPatchError = false;

        AZStd::string m_settingsWarning;

        SliceBuilderSettings m_settings;
    };
}
