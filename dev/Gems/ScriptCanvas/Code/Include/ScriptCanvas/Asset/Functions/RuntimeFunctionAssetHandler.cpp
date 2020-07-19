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

#include <ScriptCanvas/Core/Graph.h>
#include <ScriptCanvas/Asset/RuntimeAsset.h>
#include <ScriptCanvas/Asset/Functions/RuntimeFunctionAssetHandler.h>
#include <ScriptCanvas/Execution/RuntimeComponent.h>

#include <ScriptCanvas/Core/ScriptCanvasBus.h>

#include <AzCore/IO/GenericStreams.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>


namespace ScriptCanvas
{
    //=========================================================================
    // RuntimeFunctionAssetHandler
    //=========================================================================
    RuntimeFunctionAssetHandler::RuntimeFunctionAssetHandler(AZ::SerializeContext* context)
    {
        SetSerializeContext(context);

        AZ::AssetTypeInfoBus::MultiHandler::BusConnect(AZ::AzTypeInfo<ScriptCanvas::RuntimeFunctionAsset>::Uuid());
    }

    RuntimeFunctionAssetHandler::~RuntimeFunctionAssetHandler()
    {
        AZ::AssetTypeInfoBus::MultiHandler::BusDisconnect();
    }

    AZ::Data::AssetType RuntimeFunctionAssetHandler::GetAssetType() const
    {
        return AZ::AzTypeInfo<RuntimeFunctionAsset>::Uuid();
    }

    const char* RuntimeFunctionAssetHandler::GetAssetTypeDisplayName() const
    {
        return "Script Canvas Runtime Function Graph";
    }

    const char* RuntimeFunctionAssetHandler::GetGroup() const
    {
        return "Script";
    }

    const char* RuntimeFunctionAssetHandler::GetBrowserIcon() const
    {
        return "Editor/Icons/ScriptCanvas/Viewport/ScriptCanvas_Function.png";
    }

    AZ::Uuid RuntimeFunctionAssetHandler::GetComponentTypeId() const
    {
        return azrtti_typeid<RuntimeComponent>();
    }

    void RuntimeFunctionAssetHandler::GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions)
    {
        const AZ::Uuid& assetType = *AZ::AssetTypeInfoBus::GetCurrentBusId();
        if (assetType == AZ::AzTypeInfo<ScriptCanvas::RuntimeFunctionAsset>::Uuid())
        {
            extensions.push_back(ScriptCanvas::RuntimeFunctionAsset::GetFileExtension());
        }
    }

    bool RuntimeFunctionAssetHandler::CanCreateComponent(const AZ::Data::AssetId& assetId) const
    {
        // This is a runtime component so we shouldn't be making components at edit time for this
        return false;
    }

    AZ::Data::AssetPtr RuntimeFunctionAssetHandler::CreateAsset(const AZ::Data::AssetId& id, const AZ::Data::AssetType& type)
    {
        (void)type;
        AZ_Assert(type == AZ::AzTypeInfo<ScriptCanvas::RuntimeFunctionAsset>::Uuid(), "This handler deals only with the Script Canvas Runtime Asset type!");

        return aznew ScriptCanvas::RuntimeFunctionAsset(id);
    }

    bool RuntimeFunctionAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, AZ::IO::GenericStream* stream, const AZ::Data::AssetFilterCB& assetLoadFilterCB)
    {
        ScriptCanvas::RuntimeFunctionAsset* runtimeFunctionAsset = asset.GetAs<ScriptCanvas::RuntimeFunctionAsset>();
        AZ_Assert(runtimeFunctionAsset, "This should be a Script Canvas runtime asset, as this is the only type we process!");
        if (runtimeFunctionAsset && m_serializeContext)
        {
            stream->Seek(0U, AZ::IO::GenericStream::ST_SEEK_BEGIN);
            bool loadSuccess = AZ::Utils::LoadObjectFromStreamInPlace(*stream, runtimeFunctionAsset->m_runtimeData, m_serializeContext, AZ::ObjectStream::FilterDescriptor(assetLoadFilterCB));
            return loadSuccess;
        }
        return false;
    }

    bool RuntimeFunctionAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, const char* assetPath, const AZ::Data::AssetFilterCB& assetLoadFilterCB)
    {
        // Load through IFileIO
        AZ::IO::FileIOBase* fileIO = AZ::IO::FileIOBase::GetInstance();
        if (fileIO)
        {
            AZ::IO::FileIOStream stream(assetPath, AZ::IO::OpenMode::ModeRead);
            if (stream.IsOpen())
            {
                return LoadAssetData(asset, &stream, assetLoadFilterCB);
            }
        }

        return false;
    }

    bool RuntimeFunctionAssetHandler::SaveAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, AZ::IO::GenericStream* stream)
    {
        ScriptCanvas::RuntimeFunctionAsset* runtimeFunctionAsset = asset.GetAs<ScriptCanvas::RuntimeFunctionAsset>();
        AZ_Assert(runtimeFunctionAsset, "This should be a Script Canvas runtime asset, as this is the only type we process!");
        if (runtimeFunctionAsset && m_serializeContext)
        {
            AZ::ObjectStream* binaryObjStream = AZ::ObjectStream::Create(stream, *m_serializeContext, AZ::ObjectStream::ST_XML);
            bool graphSaved = binaryObjStream->WriteClass(&runtimeFunctionAsset->m_runtimeData);
            binaryObjStream->Finalize();
            return graphSaved;
        }

        return false;
    }

    void RuntimeFunctionAssetHandler::DestroyAsset(AZ::Data::AssetPtr ptr)
    {
        delete ptr;
    }

    void RuntimeFunctionAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(AZ::AzTypeInfo<ScriptCanvas::RuntimeFunctionAsset>::Uuid());
    }

    AZ::SerializeContext* RuntimeFunctionAssetHandler::GetSerializeContext() const
    {
        return m_serializeContext;
    }
    
    void RuntimeFunctionAssetHandler::SetSerializeContext(AZ::SerializeContext* context)
    {
        m_serializeContext = context;

        if (m_serializeContext == nullptr)
        {
            // use the default app serialize context
            AZ::ComponentApplicationBus::BroadcastResult(m_serializeContext, &AZ::ComponentApplicationBus::Events::GetSerializeContext);
            if (!m_serializeContext)
            {
                AZ_Error("Script Canvas", false, "RuntimeFunctionAssetHandler: No serialize context provided! We will not be able to process the Script Canvas Runtime Asset type");
            }
        }
    }
} // namespace AZ
