/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
*or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "ImageProcessing_precompiled.h"
#include <ImageBuilderComponent.h>
#include <AzCore/std/string/conversions.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzFramework/StringFunc/StringFunc.h>
#include <AzFramework/IO/LocalFileIO.h>
#include <AzCore/Debug/Trace.h>
#include <BuilderSettings/BuilderSettingManager.h>
#include <BuilderSettings/CubemapSettings.h>
#include <Processing/ImageConvert.h>
#include <Processing/PixelFormatInfo.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzCore/Serialization/EditContextConstants.inl>
#include <QFile>

#include <AzQtComponents/Utilities/QtPluginPaths.h>

namespace ImageProcessing
{
    BuilderPluginComponent::BuilderPluginComponent()
    {
        // AZ Components should only initialize their members to null and empty in constructor
        // after construction, they may be deserialized from file.
    }

    BuilderPluginComponent::~BuilderPluginComponent()
    {
    }

    void BuilderPluginComponent::Init()
    {
    }

    void BuilderPluginComponent::Activate()
    {
        //load qt plugins for some image file formats support
        AzQtComponents::PrepareQtPaths();

        // create and initialize BuilderSettingManager once since it's will be used for image conversion
        BuilderSettingManager::CreateInstance();

        auto outcome = ImageProcessing::BuilderSettingManager::Instance()->LoadBuilderSettings();
        AZ_Error("Image Processing", outcome.IsSuccess(), "Failed to load default preset settings!");

        // Activate is where you'd perform registration with other objects and systems.
        // Since we want to register our builder, we do that here:
        AssetBuilderSDK::AssetBuilderDesc builderDescriptor;
        builderDescriptor.m_name = "Image Worker Builder";
        builderDescriptor.m_version = ImageProcessing::BuilderSettingManager::Instance()->BuilderSettingsVersion();

        for (int i = 0; i < s_TotalSupportedImageExtensions; i++)
        {
            builderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern(s_SupportedImageExtensions[i], AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));
        }

        //add ".dds" here separately since we only apply copy operation for this type of file. and there won't be export option for dds files. 
        builderDescriptor.m_patterns.push_back(AssetBuilderSDK::AssetBuilderPattern("*.dds", AssetBuilderSDK::AssetBuilderPattern::PatternType::Wildcard));

        builderDescriptor.m_busId = azrtti_typeid<ImageBuilderWorker>();
        builderDescriptor.m_createJobFunction = AZStd::bind(&ImageBuilderWorker::CreateJobs, &m_imageBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        builderDescriptor.m_processJobFunction = AZStd::bind(&ImageBuilderWorker::ProcessJob, &m_imageBuilder, AZStd::placeholders::_1, AZStd::placeholders::_2);
        m_imageBuilder.BusConnect(builderDescriptor.m_busId);
        AssetBuilderSDK::AssetBuilderBus::Broadcast(&AssetBuilderSDK::AssetBuilderBusTraits::RegisterBuilderInformation, builderDescriptor);
    }

    void BuilderPluginComponent::Deactivate()
    {
        m_imageBuilder.BusDisconnect();
        BuilderSettingManager::DestroyInstance();
        CPixelFormats::DestroyInstance();
    }

    void BuilderPluginComponent::Reflect(AZ::ReflectContext* context)
    {
        // components also get Reflect called automatically
        // this is your opportunity to perform static reflection or type registration of any types you want the serializer to know about
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<BuilderPluginComponent, AZ::Component>()
                ->Version(0)
                ->Attribute(AZ::Edit::Attributes::SystemComponentTags, AZStd::vector<AZ::Crc32>({ AssetBuilderSDK::ComponentTags::AssetBuilder }))
                ;
        }

        BuilderSettingManager::Reflect(context);
        BuilderSettings::Reflect(context);
        PresetSettings::Reflect(context);
        CubemapSettings::Reflect(context);
        MipmapSettings::Reflect(context);
        TextureSettings::Reflect(context);
    }

    void BuilderPluginComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("ImagerBuilderPluginService", 0x6dc0db6e));
    }

    void BuilderPluginComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("ImagerBuilderPluginService", 0x6dc0db6e));
    }

    void ImageBuilderWorker::ShutDown()
    {
        // it is important to note that this will be called on a different thread than your process job thread
        m_isShuttingDown = true;
    }

    // this happens early on in the file scanning pass
    // this function should consistently always create the same jobs, and should do no checking whether the job is up to date or not - just be consistent.
    void ImageBuilderWorker::CreateJobs(const AssetBuilderSDK::CreateJobsRequest& request, AssetBuilderSDK::CreateJobsResponse& response)
    {
        if (m_isShuttingDown)
        {
            response.m_result = AssetBuilderSDK::CreateJobsResultCode::ShuttingDown;
            return;
        }

        // Get the extension of the file
        AZStd::string ext;
        AzFramework::StringFunc::Path::GetExtension(request.m_sourceFile.c_str(), ext, false);
        AZStd::to_upper(ext.begin(), ext.end());
        
        // We process the same file for all platforms
        for (const AssetBuilderSDK::PlatformInfo& platformInfo : request.m_enabledPlatforms)
        {
            if (ImageProcessing::BuilderSettingManager::Instance()->DoesSupportPlatform(platformInfo.m_identifier))
            {
                AssetBuilderSDK::JobDescriptor descriptor;
                descriptor.m_jobKey = ext + " Compile";
                descriptor.SetPlatformIdentifier(platformInfo.m_identifier.c_str());
                descriptor.m_critical = false;
                response.m_createJobOutputs.push_back(descriptor);
            }
        }
        
        response.m_result = AssetBuilderSDK::CreateJobsResultCode::Success;
        return; 
    }

    // later on, this function will be called for jobs that actually need doing.
    // the request will contain the CreateJobResponse you constructed earlier, including any keys and values you placed into the hash table
    void ImageBuilderWorker::ProcessJob(const AssetBuilderSDK::ProcessJobRequest& request, AssetBuilderSDK::ProcessJobResponse& response)
    {
        // Before we begin, let's make sure we are not meant to abort.
        AssetBuilderSDK::JobCancelListener jobCancelListener(request.m_jobId);

        AZStd::vector<AZStd::string> productFilepaths;
        bool imageProcessingSuccessful = false;
        bool needConversion = true;

        //if the original file is a dds file then skip conversion
        if (AzFramework::StringFunc::Path::IsExtension(request.m_fullPath.c_str(), "dds", false))
        {
            productFilepaths.push_back(request.m_fullPath);
            imageProcessingSuccessful = true;
            needConversion = false;
        }

        // Do conversion and get exported file's path
        if (needConversion)
        {
            AZ_TracePrintf(AssetBuilderSDK::InfoWindow, "Performing image conversion: %s\n", request.m_fullPath.c_str());
            ImageConvertProcess* process = CreateImageConvertProcess(request.m_fullPath, request.m_tempDirPath, 
                request.m_jobDescription.GetPlatformIdentifier());

            if (process != nullptr)
            {
                //the process can be stopped if the job is cancelled or the worker is shutting down
                while (!process->IsFinished() && !m_isShuttingDown && !jobCancelListener.IsCancelled())
                {
                    process->UpdateProcess();
                }

                //get process result
                imageProcessingSuccessful = process->IsSucceed();
                process->GetAppendOutputFilePaths(productFilepaths);

                delete process;
            }
            else
            {
                imageProcessingSuccessful = false;
            }
        }

        if (imageProcessingSuccessful)
        {
            // Report the image-import result (filepath to one or many '.dds') 
            for (const auto& product : productFilepaths)
            {
                AssetBuilderSDK::JobProduct jobProduct(product);
                response.m_outputProducts.push_back(jobProduct);
            }
            response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Success;
        }
        else
        {
            if (m_isShuttingDown)
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Cancelled job %s because shutdown was requested.\n", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            }
            else if (jobCancelListener.IsCancelled())
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Cancelled was requested for job %s.\n", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Cancelled;
            }
            else
            {
                AZ_TracePrintf(AssetBuilderSDK::ErrorWindow, "Unexpected error during processing job %s.\n", request.m_fullPath.c_str());
                response.m_resultCode = AssetBuilderSDK::ProcessJobResult_Failed;
            }
        }
    }
} // namespace ImageProcessing
