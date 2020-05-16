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
#include "StdAfx.h"
#include "PushToSliceCommand.h"

#include <AzCore/Component/TransformBus.h>

#include <AzToolsFramework/Slice/SliceUtilities.h>
namespace AzToolsFramework
{
    PushToSliceCommand::PushToSliceCommand(const AZStd::string& friendlyName)
        : BaseSliceCommand(friendlyName)
    {
    }

    void PushToSliceCommand::Capture(const AZ::Data::Asset<AZ::SliceAsset>& sliceAsset, const AZStd::unordered_map<AZ::EntityId, AZ::EntityId>& addedEntityIdRemaps)
    {
        if (!sliceAsset.Get())
        {
            AZ_Error("PushToSliceCommand::Capture",
                false,
                "Invalid SliceAsset passed in. Unable to capture undo/redo state");

            return;
        }

        // Nothing to do if no remaps
        if (addedEntityIdRemaps.empty())
        {
            AZ_Warning("PushToSliceCommand::Capture",
                false,
                "Emtpty addedEntityIdRemaps passed in. Nothing to undo/redo");

            return;
        }

        AZ::SliceComponent* editorRootSlice = nullptr;
        EditorEntityContextRequestBus::BroadcastResult(editorRootSlice, &EditorEntityContextRequestBus::Events::GetEditorRootSlice);

        // Find the parent EntityID highest up in the slice hierarchy
        AZ::EntityId parentId;
        AZ::TransformBus::EventResult(parentId, addedEntityIdRemaps.begin()->first, &AZ::TransformBus::Events::GetParentId);

        while (parentId.IsValid() && !SliceUtilities::IsSliceOrSubsliceRootEntity(parentId))
        {
            AZ::EntityId currentParentId = parentId;
            AZ::TransformBus::EventResult(parentId, currentParentId, &AZ::TransformBus::Events::GetParentId);
        }

        if (!parentId.IsValid())
        {
            AZ_Error("SliceUtilities::SlicePostPushCallback", false, "Unable to find Slice Parent Entity cannot proceed with callback");
            return;
        }

        // Acquire the highest level owning slice of this hierarchy
        AZ::SliceComponent::SliceInstanceAddress targetSliceAddress = editorRootSlice->FindSlice(parentId);

        // Capture undo state for each entity
        for (auto addedIdRemapPair : addedEntityIdRemaps)
        {
            const AZ::EntityId& liveEntityId = addedIdRemapPair.first;
            const AZ::EntityId& assetEntityId = addedIdRemapPair.second;

            // Manufacture restoreInfo for the redo to add the entity back into targetSliceAddress
            AZ::SliceComponent::EntityRestoreInfo redoRestoreInfo(sliceAsset, targetSliceAddress.GetInstance()->GetId(), assetEntityId, AZ::DataPatch::FlagsMap());
            m_entityRedoRestoreInfoArray.emplace_back(liveEntityId, redoRestoreInfo);

            bool restoreCaptured = BaseSliceCommand::CaptureRestoreInfoForUndo(liveEntityId);

            if (!restoreCaptured)
            {
                m_entityRedoRestoreInfoArray.clear();
                m_entityUndoRestoreInfoArray.clear();

                return;
            }
        }
    }

    void PushToSliceCommand::Undo()
    {
        BaseSliceCommand::RestoreEntities(m_entityUndoRestoreInfoArray);
    }

    void PushToSliceCommand::Redo()
    {
        BaseSliceCommand::RestoreEntities(m_entityRedoRestoreInfoArray, false, AzToolsFramework::SliceEntityRestoreType::Added);
    }
}
