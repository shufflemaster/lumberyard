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

#include "precompiled.h"

#include <ScriptCanvas/Assets/Functions/ScriptCanvasFunctionAssetHandler.h>

#include <ScriptCanvas/Asset/RuntimeAsset.h>
#include <ScriptCanvas/Bus/ScriptCanvasBus.h>
#include <Core/ScriptCanvasBus.h>
#include <ScriptCanvas/Components/EditorGraph.h>
#include <ScriptCanvas/Components/EditorScriptCanvasComponent.h>
#include <ScriptCanvas/Components/EditorGraphVariableManagerComponent.h>

#include <GraphCanvas/GraphCanvasBus.h>

#include <AzCore/IO/GenericStreams.h>
#include <AzCore/IO/FileIO.h>
#include <AzCore/Serialization/Utils.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/std/string/string_view.h>

#include <AzFramework/StringFunc/StringFunc.h>
#include <AzToolsFramework/API/EditorAssetSystemAPI.h>
#include <ScriptCanvas/Asset/Functions/ScriptCanvasFunctionAsset.h>

namespace ScriptCanvasEditor
{
    ScriptCanvasFunctionAssetHandler::ScriptCanvasFunctionAssetHandler(AZ::SerializeContext* context)
        : ScriptCanvasAssetHandler(context)
    {
        AZ::AssetTypeInfoBus::MultiHandler::BusConnect(azrtti_typeid<ScriptCanvas::ScriptCanvasFunctionAsset>());
    }

    ScriptCanvasFunctionAssetHandler::~ScriptCanvasFunctionAssetHandler()
    {
        AZ::AssetTypeInfoBus::MultiHandler::BusDisconnect();
    }

    AZ::Data::AssetPtr ScriptCanvasFunctionAssetHandler::CreateAsset(const AZ::Data::AssetId& id, const AZ::Data::AssetType& type)
    {
        AZ_UNUSED(type);

        ScriptCanvas::ScriptCanvasFunctionAsset* assetData = aznew ScriptCanvas::ScriptCanvasFunctionAsset(id);

        AZ::Entity* scriptCanvasEntity = aznew AZ::Entity(ScriptCanvas::AssetDescription::GetEntityName<ScriptCanvas::ScriptCanvasFunctionAsset>());
        SystemRequestBus::Broadcast(&SystemRequests::CreateEditorComponentsOnEntity, scriptCanvasEntity, azrtti_typeid<ScriptCanvas::RuntimeFunctionAsset>());
        assetData->m_cachedComponent = scriptCanvasEntity->CreateComponent<ScriptCanvas::ScriptCanvasFunctionDataComponent>();

        assetData->SetScriptCanvasEntity(scriptCanvasEntity);

        return assetData;
    }

    bool ScriptCanvasFunctionAssetHandler::LoadAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, AZ::IO::GenericStream* stream, const AZ::Data::AssetFilterCB& assetLoadFilterCB)
    {
        ScriptCanvas::ScriptCanvasFunctionAsset* scriptCanvasAsset = asset.GetAs<ScriptCanvas::ScriptCanvasFunctionAsset>();
        AZ_Assert(m_serializeContext, "ScriptCanvasFunctionAssetHandler needs to be initialized with a SerializeContext");

        if (scriptCanvasAsset)
        {
            stream->Seek(0U, AZ::IO::GenericStream::ST_SEEK_BEGIN);

            bool loadSuccess = AZ::Utils::LoadObjectFromStreamInPlace(*stream, scriptCanvasAsset->GetScriptCanvasData(), m_serializeContext, AZ::ObjectStream::FilterDescriptor(assetLoadFilterCB, AZ::ObjectStream::FILTERFLAG_IGNORE_UNKNOWN_CLASSES));
            if (loadSuccess)
            {
                scriptCanvasAsset->m_cachedComponent = scriptCanvasAsset->GetScriptCanvasData().GetScriptCanvasEntity()->FindComponent<ScriptCanvas::ScriptCanvasFunctionDataComponent>();
            }

            return loadSuccess;
        }
        return false;
    }

    bool ScriptCanvasFunctionAssetHandler::SaveAssetData(const AZ::Data::Asset<AZ::Data::AssetData>& asset, AZ::IO::GenericStream* stream)
    {
        return SaveAssetData(asset.GetAs<ScriptCanvas::ScriptCanvasFunctionAsset>(), stream);
    }

    bool ScriptCanvasFunctionAssetHandler::SaveAssetData(const ScriptCanvas::ScriptCanvasFunctionAsset* assetData, AZ::IO::GenericStream* stream)
    {
        return SaveAssetData(assetData, stream, AZ::DataStream::ST_XML);
    }

    bool ScriptCanvasFunctionAssetHandler::SaveAssetData(const ScriptCanvas::ScriptCanvasFunctionAsset* assetData, AZ::IO::GenericStream* stream, AZ::DataStream::StreamType streamType)
    {
        if (assetData && m_serializeContext)
        {
            AZStd::vector<char> byteBuffer;
            AZ::IO::ByteContainerStream<decltype(byteBuffer)> byteStream(&byteBuffer);
            AZ::ObjectStream* objStream = AZ::ObjectStream::Create(&byteStream, *m_serializeContext, streamType);

            const ScriptCanvas::ScriptCanvasData& functionData = assetData->GetScriptCanvasData();
            bool assetSaved = objStream->WriteClass(&functionData);
            objStream->Finalize();
            assetSaved = stream->Write(byteBuffer.size(), byteBuffer.data()) == byteBuffer.size() && assetSaved;
            return assetSaved;
        }

        return false;
    }

    AZ::Data::AssetType ScriptCanvasFunctionAssetHandler::GetAssetType() const
    {
        return ScriptCanvasFunctionAssetHandler::GetAssetTypeStatic();
    }

    const char* ScriptCanvasFunctionAssetHandler::GetAssetTypeDisplayName() const
    {
        return ScriptCanvas::AssetDescription::GetAssetTypeDisplayName<ScriptCanvas::ScriptCanvasFunctionAsset>();
    }

    bool ScriptCanvasFunctionAssetHandler::CanCreateComponent(const AZ::Data::AssetId& assetId) const
    {
        return false;
    }

    void ScriptCanvasFunctionAssetHandler::GetAssetTypeExtensions(AZStd::vector<AZStd::string>& extensions)
    {
        const AZ::Uuid& assetType = *AZ::AssetTypeInfoBus::GetCurrentBusId();
        if (assetType == AZ::AzTypeInfo<ScriptCanvas::ScriptCanvasFunctionAsset>::Uuid())
        {
            extensions.push_back(ScriptCanvas::AssetDescription::GetExtension<ScriptCanvas::ScriptCanvasFunctionAsset>());
        }
    }

    void ScriptCanvasFunctionAssetHandler::GetHandledAssetTypes(AZStd::vector<AZ::Data::AssetType>& assetTypes)
    {
        assetTypes.push_back(AZ::AzTypeInfo<ScriptCanvas::ScriptCanvasFunctionAsset>::Uuid());
    }


    AZ::Data::AssetType ScriptCanvasFunctionAssetHandler::GetAssetTypeStatic()
    {
        return azrtti_typeid<ScriptCanvas::ScriptCanvasFunctionAsset>();
    }

    const char* ScriptCanvasFunctionAssetHandler::GetGroup() const
    {
        return ScriptCanvas::AssetDescription::GetGroup<ScriptCanvas::ScriptCanvasFunctionAsset>();
    }

    const char* ScriptCanvasFunctionAssetHandler::GetBrowserIcon() const
    {
        return ScriptCanvas::AssetDescription::GetIconPath<ScriptCanvas::ScriptCanvasFunctionAsset>();
    }

} // namespace ScriptCanvasEditor
