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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#include "StdAfx.h"
#include <MathConversion.h>

#include <Maestro/Bus/SequenceComponentBus.h>
#include <Maestro/Bus/EditorSequenceComponentBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>
#include <AzToolsFramework/ToolsComponents/TransformComponent.h>
#include <AzToolsFramework/ToolsComponents/GenericComponentWrapper.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzFramework/API/ApplicationAPI.h>
#include <AzToolsFramework/API/EntityCompositionRequestBus.h>
#include <AzToolsFramework/ToolsComponents/EditorDisabledCompositionBus.h>
#include <AzToolsFramework/ToolsComponents/EditorPendingCompositionComponent.h>
#include <AzFramework/API/ApplicationAPI.h>

#include "TrackViewDialog.h"
#include "TrackViewAnimNode.h"
#include "TrackViewTrack.h"
#include "TrackViewSequence.h"
#include "TrackViewUndo.h"
#include "TrackViewNodeFactories.h"
#include "AnimationContext.h"
#include "CommentNodeAnimator.h"
#include "LayerNodeAnimator.h"
#include "DirectorNodeAnimator.h"
#include <Plugins/ComponentEntityEditorPlugin/Objects/ComponentEntityObject.h>
#include "Objects/EntityObject.h"
#include "Objects/CameraObject.h"
#include "Objects/SequenceObject.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/MiscEntities.h"
#include "Objects/GizmoManager.h"
#include "Objects/ObjectManager.h"
#include "ViewManager.h"
#include "RenderViewport.h"
#include "Clipboard.h"
#include <Maestro/Types/AnimNodeType.h>
#include <Maestro/Types/AnimValueType.h>
#include <Maestro/Types/AnimParamType.h>
#include <Maestro/Types/SequenceType.h>

// static class data
const AZ::Uuid CTrackViewAnimNode::s_nullUuid = AZ::Uuid::CreateNull();

//////////////////////////////////////////////////////////////////////////
static void CreateDefaultTracksForEntityNode(CTrackViewAnimNode* node, const AZStd::vector<AnimParamType>& tracks)
{
    AZ_Assert(node->GetType() == AnimNodeType::AzEntity, "Expected AzEntity node for creating default tracks");

    // add a Transform Component anim node if needed, then go through and look for Position,
    // Rotation and Scale default tracks and adds them by hard-coded Virtual Property name. This is not a scalable way to do this,
    // but fits into the legacy Track View entity property system. This will all be re-written in a new TrackView for components in the future.
    AZ::Entity* entity = nullptr;
    AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, node->GetAzEntityId());
    if (entity)
    {
        AZ::Component* transformComponent = entity->FindComponent(AzToolsFramework::Components::TransformComponent::TYPEINFO_Uuid());
        if (transformComponent)
        {
            // find a transform Component Node if it exists, otherwise create one.
            CTrackViewAnimNode* transformComponentNode = nullptr;

            for (int i = node->GetChildCount(); --i >= 0;)
            {
                if (node->GetChild(i)->GetNodeType() == eTVNT_AnimNode)
                {
                    CTrackViewAnimNode* childAnimNode = static_cast<CTrackViewAnimNode*>(node->GetChild(i));
                    AZ::ComponentId componentId = childAnimNode->GetComponentId();
                    AZ::Uuid componentTypeId;
                    AzFramework::ApplicationRequests::Bus::BroadcastResult(componentTypeId, &AzFramework::ApplicationRequests::Bus::Events::GetComponentTypeId, entity->GetId(), componentId);
                    if (componentTypeId == AzToolsFramework::Components::TransformComponent::TYPEINFO_Uuid())
                    {
                        transformComponentNode = childAnimNode;
                        break;
                    }
                }
            }

            if (!transformComponentNode)
            {
                // no existing Transform Component node found - create one.
                transformComponentNode = node->AddComponent(transformComponent, false);
            }

            if (transformComponentNode)
            {
                for (size_t i = 0; i < tracks.size(); ++i)
                {
                    // This is not ideal - we hard-code the VirtualProperty names for "Position", "Rotation", 
                    // and "Scale" here, which creates an implicitly name dependency, but these are unlikely to change.
                    CAnimParamType paramType = tracks[i];
                    CAnimParamType transformPropertyParamType;
                    bool createTransformTrack = false;

                    if (paramType.GetType() == AnimParamType::Position)
                    {
                        transformPropertyParamType = AZStd::string("Position");
                        createTransformTrack = true;
                    }
                    else if (paramType.GetType() == AnimParamType::Rotation)
                    {
                        transformPropertyParamType = AZStd::string("Rotation");
                        createTransformTrack = true;
                    }
                    else if (paramType.GetType() == AnimParamType::Scale)
                    {
                        transformPropertyParamType = AZStd::string("Scale");
                        createTransformTrack = true;
                    }

                    if (createTransformTrack)
                    {
                        // this sets the type to one of AnimParamType::Position, AnimParamType::Rotation or AnimParamType::Scale
                        // but maintains the name
                        transformPropertyParamType = paramType.GetType();
                        transformComponentNode->CreateTrack(transformPropertyParamType);
                    }
                }
            } 
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNodeBundle::AppendAnimNode(CTrackViewAnimNode* node)
{
    stl::push_back_unique(m_animNodes, node);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNodeBundle::AppendAnimNodeBundle(const CTrackViewAnimNodeBundle& bundle)
{
    for (auto iter = bundle.m_animNodes.begin(); iter != bundle.m_animNodes.end(); ++iter)
    {
        AppendAnimNode(*iter);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNodeBundle::ExpandAll(bool bAlsoExpandParentNodes)
{
    std::set<CTrackViewNode*> nodesToExpand;
    std::copy(m_animNodes.begin(), m_animNodes.end(), std::inserter(nodesToExpand, nodesToExpand.end()));

    if (bAlsoExpandParentNodes)
    {
        for (auto iter = nodesToExpand.begin(); iter != nodesToExpand.end(); ++iter)
        {
            CTrackViewNode* node = *iter;

            for (CTrackViewNode* pParent = node->GetParentNode(); pParent; pParent = pParent->GetParentNode())
            {
                nodesToExpand.insert(pParent);
            }
        }
    }

    for (auto iter = nodesToExpand.begin(); iter != nodesToExpand.end(); ++iter)
    {
        (*iter)->SetExpanded(true);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNodeBundle::CollapseAll()
{
    for (auto iter = m_animNodes.begin(); iter != m_animNodes.end(); ++iter)
    {
        (*iter)->SetExpanded(false);
    }
}

//////////////////////////////////////////////////////////////////////////
const bool CTrackViewAnimNodeBundle::DoesContain(const CTrackViewNode* pTargetNode)
{
    return stl::find(m_animNodes, pTargetNode);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNodeBundle::Clear()
{
    m_animNodes.clear();
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNode::CTrackViewAnimNode(IAnimSequence* pSequence, IAnimNode* animNode, CTrackViewNode* pParentNode)
    : CTrackViewNode(pParentNode)
    , m_animSequence(pSequence)
    , m_animNode(animNode)
    , m_pNodeEntity(nullptr)
    , m_pNodeAnimator(nullptr)
    , m_trackGizmo(nullptr)
{
    if (animNode)
    {
        // Search for child nodes
        const int nodeCount = pSequence->GetNodeCount();
        for (int i = 0; i < nodeCount; ++i)
        {
            IAnimNode* node = pSequence->GetNode(i);
            IAnimNode* pParentNode = node->GetParent();

            // If our node is the parent, then the current node is a child of it
            if (animNode == pParentNode)
            {
                CTrackViewAnimNodeFactory animNodeFactory;
                CTrackViewAnimNode* pNewTVAnimNode = animNodeFactory.BuildAnimNode(pSequence, node, this);
                m_childNodes.push_back(std::unique_ptr<CTrackViewNode>(pNewTVAnimNode));
            }
        }

        // Copy tracks from animNode
        const int trackCount = animNode->GetTrackCount();
        for (int i = 0; i < trackCount; ++i)
        {
            IAnimTrack* pTrack = animNode->GetTrackByIndex(i);

            CTrackViewTrackFactory trackFactory;
            CTrackViewTrack* pNewTVTrack = trackFactory.BuildTrack(pTrack, this, this);
            m_childNodes.push_back(std::unique_ptr<CTrackViewNode>(pNewTVTrack));
        }

        // Set owner to update entity CryMovie entity IDs and remove it again
        SetNodeEntity(GetNodeEntity());
    }

    SortNodes();

    switch (GetType())
    {
    case AnimNodeType::Comment:
        m_pNodeAnimator.reset(new CCommentNodeAnimator(this));
        break;
    case AnimNodeType::Layer:
        m_pNodeAnimator.reset(new CLayerNodeAnimator());
        break;
    case AnimNodeType::Director:
        m_pNodeAnimator.reset(new CDirectorNodeAnimator(this));
        break;
    }

    AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusConnect();

    if (IsBoundToAzEntity())
    {
        AZ::TransformNotificationBus::Handler::BusConnect(GetAzEntityId());
    }
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNode::~CTrackViewAnimNode()
{
    AzToolsFramework::EditorEntityContextNotificationBus::Handler::BusDisconnect();

    if (IsBoundToAzEntity())
    {
        AZ::EntityId entityId = GetAzEntityId();
        AZ::TransformNotificationBus::Handler::BusDisconnect(entityId);
        AZ::EntityBus::Handler::BusDisconnect(entityId);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::BindToEditorObjects()
{
    if (!IsActive())
    {
        return;
    }

    CTrackViewSequenceNotificationContext context(GetSequence());

    CTrackViewAnimNode* director = GetDirector();
    const bool bBelongsToActiveDirector = director ? director->IsActiveDirector() : true;

    if (bBelongsToActiveDirector)
    {
        IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
        CEntityObject* pEntity = (CEntityObject*)pObjectManager->FindAnimNodeOwner(this);
        bool ownerChanged = false;
        if (m_pNodeAnimator)
        {
            m_pNodeAnimator->Bind(this);
        }

        if (m_animNode)
        {
            m_animNode->SetNodeOwner(this);
            ownerChanged = true;
        }

        if (pEntity)
        {
            pEntity->SetTransformDelegate(this);
            pEntity->RegisterListener(this);
            SetNodeEntity(pEntity);
        }

        if (ownerChanged)
        {
            GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_NodeOwnerChanged);
        }

        for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
        {
            CTrackViewNode* pChildNode = (*iter).get();
            if (pChildNode->GetNodeType() == eTVNT_AnimNode)
            {
                CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
                pChildAnimNode->BindToEditorObjects();
            }
        }
    }

    UpdateTrackGizmo();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::UnBindFromEditorObjects()
{
    CTrackViewSequenceNotificationContext context(GetSequence());

    IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
    CEntityObject* pEntity = (CEntityObject*)pObjectManager->FindAnimNodeOwner(this);

    if (pEntity)
    {
        pEntity->SetTransformDelegate(nullptr);
        pEntity->UnregisterListener(this);
    }

    if (m_animNode)
    {
        // 'Owner' is the TrackViewNode, as opposed to the EditorEntityNode (as 'owner' is used in animSequence, or the pEntity 
        // returned from FindAnimNodeOwner() - confusing, isn't it?
        m_animNode->SetNodeOwner(nullptr);
    }

    if (m_pNodeAnimator)
    {
        m_pNodeAnimator->UnBind(this);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
            pChildAnimNode->UnBindFromEditorObjects();
        }
    }

    GetIEditor()->GetObjectManager()->GetGizmoManager()->RemoveGizmo(m_trackGizmo);
    m_trackGizmo = nullptr;
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsBoundToEditorObjects() const
{
    if (m_animNode)
    {
        if (m_animNode->GetType() == AnimNodeType::AzEntity)
        {
            // check if bound to comoponent entity
            return m_animNode->GetAzEntityId().IsValid();
        }
        else
        {
            // check if bound to legacy entity
            return (m_animNode->GetNodeOwner() != NULL);
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SyncToConsole(SAnimContext& animContext)
{
    switch (GetType())
    {
    case AnimNodeType::Camera:
    {
        IEntity* pEntity = GetEntity();
        if (pEntity)
        {
            CBaseObject* pCameraObject = GetIEditor()->GetObjectManager()->FindObject(GetIEditor()->GetViewManager()->GetCameraObjectId());
            IEntity* pCameraEntity = pCameraObject ? ((CEntityObject*)pCameraObject)->GetIEntity() : NULL;
            if (pCameraEntity && pEntity->GetId() == pCameraEntity->GetId())
            // If this camera is currently active,
            {
                Matrix34 viewTM = pEntity->GetWorldTM();
                Vec3 oPosition(viewTM.GetTranslation());
                Vec3 oDirection(viewTM.TransformVector(FORWARD_DIRECTION));
            }
        }
    }
    break;
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = (CTrackViewAnimNode*)pChildNode;
            pChildAnimNode->SyncToConsole(animContext);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNode* CTrackViewAnimNode::CreateSubNode(const QString& originalName, const AnimNodeType animNodeType, CEntityObject* owner , AZ::Uuid componentTypeId, AZ::ComponentId componentId)
{
    const bool isGroupNode = IsGroupNode();
    AZ_Assert(isGroupNode, "Expected CreateSubNode to be called on a group capible node.");
    if (!isGroupNode)
    {
        return nullptr;
    }

    const auto originalNameStr = originalName.toUtf8();

    // Find the director or sequence.
    CTrackViewAnimNode* director = (GetType() == AnimNodeType::Director) ? this : GetDirector();
    director = director ? director : GetSequence();
    AZ_Assert(director, "Expected a valid director or sequence to be found.");
    if (!director)
    {
        return nullptr;
    }

    // If this is an AzEntity, make sure there is an associated entity id
    AZ::EntityId entityId(AZ::EntityId::InvalidEntityId);
    if (owner != nullptr && animNodeType == AnimNodeType::AzEntity)
    {
        AzToolsFramework::ComponentEntityObjectRequestBus::EventResult(entityId, owner, &AzToolsFramework::ComponentEntityObjectRequestBus::Events::GetAssociatedEntityId);
        if (!entityId.IsValid())
        {
            GetIEditor()->GetMovieSystem()->LogUserNotificationMsg(AZStd::string::format("Failed to add '%s' to sequence '%s', could not find associated entity. Please try adding the entity associated with '%s'.", originalNameStr.constData(), director->GetName(), originalNameStr.constData()));            return nullptr;
            return nullptr;
        }
    }

    QString name = originalName;

    // Check if the node's director or sequence already contains a node with this name, unless it's a component, for which we allow duplicate names since
    // Components are children of unique AZEntities in Track View. If a different Entity component with the same name exists, create a new unique name;
    if (animNodeType != AnimNodeType::Component)
    {
        CTrackViewAnimNode* director = (GetType() == AnimNodeType::Director) ? this : GetDirector();
        director = director ? director : GetSequence();

        AZ_Assert(director, "Expected a valid director or sequence to be found.");
        if (!director)
        {
            return nullptr;
        }

        bool alreadyExists = false;

        // If this is a valid AzEntity, we may generate a unique name if a dupe name is found
        // from a different entity.
        if (entityId.IsValid())
        {
            // Check for a duplicates
            CTrackViewAnimNodeBundle azEntityNodesFound = director->GetAnimNodesByType(AnimNodeType::AzEntity);
            for (int x = 0; x < azEntityNodesFound.GetCount(); x++)
            {
                if (azEntityNodesFound.GetNode(x)->GetAzEntityId() == entityId)
                {
                    alreadyExists = true;
                    break;
                }
            }
        }
        else
        {
            // Search by name for other non AzEntity 
            alreadyExists = director->GetAnimNodesByName(name.toUtf8().data()).GetCount() > 0;
        }

        // Show an error if this node is a duplicate
        if (alreadyExists)
        {
            GetIEditor()->GetMovieSystem()->LogUserNotificationMsg(AZStd::string::format("'%s' already exists in sequence '%s', skipping...", originalNameStr.constData(), director->GetName()));
            return nullptr;
        }

        // Ensure a unique name, disallowed duplicates are already resolved by here.
        name = GetAvailableNodeNameStartingWith(name);
    }

    const auto nameStr = name.toUtf8();

    // Create CryMovie and TrackView node
    IAnimNode* newAnimNode = m_animSequence->CreateNode(animNodeType);
    if (!newAnimNode)
    {
        GetIEditor()->GetMovieSystem()->LogUserNotificationMsg(AZStd::string::format("Failed to add '%s' to sequence '%s'.", nameStr.constData(), director->GetName()));
        return nullptr;
    }

    newAnimNode->SetName(nameStr.constData());
    newAnimNode->CreateDefaultTracks();
    newAnimNode->SetParent(m_animNode.get());
    newAnimNode->SetComponent(componentId, componentTypeId);

    CTrackViewAnimNodeFactory animNodeFactory;
    CTrackViewAnimNode* newNode = animNodeFactory.BuildAnimNode(m_animSequence, newAnimNode, this);

    // Make sure that camera and entity nodes get created with an owner
    AZ_Assert((animNodeType != AnimNodeType::Camera && animNodeType != AnimNodeType::Entity) || owner, "Entity node should have valid owner.");

    newNode->SetNodeEntity(owner);
    newAnimNode->SetNodeOwner(newNode);

    newNode->BindToEditorObjects();

    AddNode(newNode);

    // Add node to sequence, let AZ Undo take care of undo/redo
    m_animSequence->AddNode(newNode->m_animNode.get());

    return newNode;
}

// Helper function to remove a child node
void CTrackViewAnimNode::RemoveChildNode(CTrackViewAnimNode* child)
{
    assert(child);
    auto parent = static_cast<CTrackViewAnimNode*>(child->m_pParentNode);
    assert(parent);

    child->UnBindFromEditorObjects();

    for (auto iter = parent->m_childNodes.begin(); iter != parent->m_childNodes.end(); ++iter)
    {
        std::unique_ptr<CTrackViewNode>& currentNode = *iter;

        if (currentNode.get() == child)
        {
            parent->m_childNodes.erase(iter);
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::RemoveSubNode(CTrackViewAnimNode* pSubNode)
{
    assert(CUndo::IsRecording());

    const bool bIsGroupNode = IsGroupNode();
    assert(bIsGroupNode);
    if (!bIsGroupNode)
    {
        return;
    }

    // remove anim node children
    for (int i = pSubNode->GetChildCount(); --i >= 0;)
    {
        CTrackViewAnimNode* childAnimNode = static_cast<CTrackViewAnimNode*>(pSubNode->GetChild(i));
        if (childAnimNode->GetNodeType() == eTVNT_AnimNode)
        {
            RemoveSubNode(childAnimNode);
        }
    }

    // Remove node from sequence entity, let AZ Undo take care of undo/redo
    m_animSequence->RemoveNode(pSubNode->m_animNode.get(), /*removeChildRelationships=*/ false);
    pSubNode->GetSequence()->OnNodeChanged(pSubNode, ITrackViewSequenceListener::eNodeChangeType_Removed);
    RemoveChildNode(pSubNode);
}

//////////////////////////////////////////////////////////////////////////
CTrackViewTrack* CTrackViewAnimNode::CreateTrack(const CAnimParamType& paramType)
{
    assert(CUndo::IsRecording());

    if (GetTrackForParameter(paramType) && !(GetParamFlags(paramType) & IAnimNode::eSupportedParamFlags_MultipleTracks))
    {
        return nullptr;
    }

    // Create CryMovie track
    IAnimTrack* pNewAnimTrack = m_animNode->CreateTrack(paramType);
    if (!pNewAnimTrack)
    {
        return nullptr;
    }

    // Create Track View Track
    CTrackViewTrackFactory trackFactory;
    CTrackViewTrack* pNewTrack = trackFactory.BuildTrack(pNewAnimTrack, this, this);

    AddNode(pNewTrack);

    MarkAsModified();

    const AnimParamType animParamType = paramType.GetType();
    SetPosRotScaleTracksDefaultValues(
        animParamType == AnimParamType::Position,
        animParamType == AnimParamType::Rotation,
        animParamType == AnimParamType::Scale
    );

    return pNewTrack;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::RemoveTrack(CTrackViewTrack* track)
{
    assert(CUndo::IsRecording());
    assert(!track->IsSubTrack());

    if (!track->IsSubTrack())
    {
        CTrackViewSequence* sequence = track->GetSequence();
        if (nullptr != sequence)
        {
            AzToolsFramework::ScopedUndoBatch undoBatch("Remove Track");
            CTrackViewAnimNode* parentNode = track->GetAnimNode();
            std::unique_ptr<CTrackViewNode> foundTrack;

            if (nullptr != parentNode)
            {
                for (auto iter = parentNode->m_childNodes.begin(); iter != parentNode->m_childNodes.end(); ++iter)
                {
                    std::unique_ptr<CTrackViewNode>& currentNode = *iter;
                    if (currentNode.get() == track)
                    {
                        // Hang onto a reference until after OnNodeChanged is called.
                        currentNode.swap(foundTrack);

                        // Remove from parent node and sequence
                        parentNode->m_childNodes.erase(iter);
                        parentNode->m_animNode->RemoveTrack(track->GetAnimTrack());
                        break;
                    }
                }

                m_pParentNode->GetSequence()->OnNodeChanged(track, ITrackViewSequenceListener::eNodeChangeType_Removed);

                // Release the track now that OnNodeChanged is complete.
                if (nullptr != foundTrack.get())
                {
                    foundTrack.release();
                }

            }
            undoBatch.MarkEntityDirty(sequence->GetSequenceComponentEntityId());            
        }
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::SnapTimeToPrevKey(float& time) const
{
    const float startTime = time;
    float closestTrackTime = std::numeric_limits<float>::min();
    bool bFoundPrevKey = false;

    for (size_t i = 0; i < m_childNodes.size(); ++i)
    {
        const CTrackViewNode* node = m_childNodes[i].get();

        float closestNodeTime = startTime;
        if (node->SnapTimeToPrevKey(closestNodeTime))
        {
            closestTrackTime = std::max(closestNodeTime, closestTrackTime);
            bFoundPrevKey = true;
        }
    }

    if (bFoundPrevKey)
    {
        time = closestTrackTime;
    }

    return bFoundPrevKey;
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::SnapTimeToNextKey(float& time) const
{
    const float startTime = time;
    float closestTrackTime = std::numeric_limits<float>::max();
    bool bFoundNextKey = false;

    for (size_t i = 0; i < m_childNodes.size(); ++i)
    {
        const CTrackViewNode* node = m_childNodes[i].get();

        float closestNodeTime = startTime;
        if (node->SnapTimeToNextKey(closestNodeTime))
        {
            closestTrackTime = std::min(closestNodeTime, closestTrackTime);
            bFoundNextKey = true;
        }
    }

    if (bFoundNextKey)
    {
        time = closestTrackTime;
    }

    return bFoundNextKey;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetExpanded(bool expanded)
{
    if (GetExpanded() != expanded)
    {
        CTrackViewSequence* sequence = GetSequence();
        AZ_Assert(nullptr != sequence, "Every node should have a sequence.");
        if (nullptr != sequence)
        {
            AZ_Assert(m_animNode, "Expected m_animNode to be valid.");
            if (m_animNode)
            {
                m_animNode->SetExpanded(expanded);
            }

            if (expanded)
            {
                sequence->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Expanded);
            }
            else
            {
                sequence->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Collapsed);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::GetExpanded() const
{
    bool result = true;

    AZ_Assert(m_animNode, "Expected m_animNode to be valid.");
    if (m_animNode)
    {
        result = m_animNode->GetExpanded();
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewKeyBundle CTrackViewAnimNode::GetSelectedKeys()
{
    CTrackViewKeyBundle bundle;

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        bundle.AppendKeyBundle((*iter)->GetSelectedKeys());
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewKeyBundle CTrackViewAnimNode::GetAllKeys()
{
    CTrackViewKeyBundle bundle;

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        bundle.AppendKeyBundle((*iter)->GetAllKeys());
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewKeyBundle CTrackViewAnimNode::GetKeysInTimeRange(const float t0, const float t1)
{
    CTrackViewKeyBundle bundle;

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        bundle.AppendKeyBundle((*iter)->GetKeysInTimeRange(t0, t1));
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewTrackBundle CTrackViewAnimNode::GetAllTracks()
{
    return GetTracks(false, CAnimParamType());
}

//////////////////////////////////////////////////////////////////////////
CTrackViewTrackBundle CTrackViewAnimNode::GetSelectedTracks()
{
    return GetTracks(true, CAnimParamType());
}

//////////////////////////////////////////////////////////////////////////
CTrackViewTrackBundle CTrackViewAnimNode::GetTracksByParam(const CAnimParamType& paramType) const
{
    return GetTracks(false, paramType);
}

//////////////////////////////////////////////////////////////////////////
CTrackViewTrackBundle CTrackViewAnimNode::GetTracks(const bool bOnlySelected, const CAnimParamType& paramType) const
{
    CTrackViewTrackBundle bundle;

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* node = (*iter).get();

        if (node->GetNodeType() == eTVNT_Track)
        {
            CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(node);

            if (paramType != AnimParamType::Invalid && pTrack->GetParameterType() != paramType)
            {
                continue;
            }

            if (!bOnlySelected || pTrack->IsSelected())
            {
                bundle.AppendTrack(pTrack);
            }

            const unsigned int subTrackCount = pTrack->GetChildCount();
            for (unsigned int subTrackIndex = 0; subTrackIndex < subTrackCount; ++subTrackIndex)
            {
                CTrackViewTrack* pSubTrack = static_cast<CTrackViewTrack*>(pTrack->GetChild(subTrackIndex));
                if (!bOnlySelected || pSubTrack->IsSelected())
                {
                    bundle.AppendTrack(pSubTrack);
                }
            }
        }
        else if (node->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* animNode = static_cast<CTrackViewAnimNode*>(node);
            bundle.AppendTrackBundle(animNode->GetTracks(bOnlySelected, paramType));
        }
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
AnimNodeType CTrackViewAnimNode::GetType() const
{
    return m_animNode ? m_animNode->GetType() : AnimNodeType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
EAnimNodeFlags CTrackViewAnimNode::GetFlags() const
{
    return m_animNode ? (EAnimNodeFlags)m_animNode->GetFlags() : (EAnimNodeFlags)0;
}

bool CTrackViewAnimNode::AreFlagsSetOnNodeOrAnyParent(EAnimNodeFlags flagsToCheck) const
{
    return m_animNode ? m_animNode->AreFlagsSetOnNodeOrAnyParent(flagsToCheck) : false;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetAsActiveDirector()
{
    if (GetType() == AnimNodeType::Director)
    {
        m_animSequence->SetActiveDirector(m_animNode.get());

        GetSequence()->UnBindFromEditorObjects();
        GetSequence()->BindToEditorObjects();

        GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_SetAsActiveDirector);
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsActiveDirector() const
{
    return m_animNode == m_animSequence->GetActiveDirector();
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsParamValid(const CAnimParamType& param) const
{
    return m_animNode ? m_animNode->IsParamValid(param) : false;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewTrack* CTrackViewAnimNode::GetTrackForParameter(const CAnimParamType& paramType, uint32 index) const
{
    uint32 currentIndex = 0;

    if (GetType() == AnimNodeType::AzEntity)
    {
        // For AzEntity, search for track on all child components - returns first track match found (note components searched in reverse)
        for (int i = GetChildCount(); --i >= 0;)
        {
            if (GetChild(i)->GetNodeType() == eTVNT_AnimNode)
            {
                CTrackViewAnimNode* componentNode = static_cast<CTrackViewAnimNode*>(GetChild(i));
                if (componentNode->GetType() == AnimNodeType::Component)
                {
                    if (CTrackViewTrack* track = componentNode->GetTrackForParameter(paramType, index))
                    {
                        return track;
                    }
                }
            }
        }
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* node = (*iter).get();

        if (node->GetNodeType() == eTVNT_Track)
        {
            CTrackViewTrack* pTrack = static_cast<CTrackViewTrack*>(node);

            if (pTrack->GetParameterType() == paramType)
            {
                if (currentIndex == index)
                {
                    return pTrack;
                }
                else
                {
                    ++currentIndex;
                }
            }

            if (pTrack->IsCompoundTrack())
            {
                unsigned int numChildTracks = pTrack->GetChildCount();
                for (unsigned int i = 0; i < numChildTracks; ++i)
                {
                    CTrackViewTrack* pChildTrack = static_cast<CTrackViewTrack*>(pTrack->GetChild(i));
                    if (pChildTrack->GetParameterType() == paramType)
                    {
                        if (currentIndex == index)
                        {
                            return pChildTrack;
                        }
                        else
                        {
                            ++currentIndex;
                        }
                    }
                }
            }
        }
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::Render(const SAnimContext& ac)
{
    if (m_pNodeAnimator && IsActive())
    {
        m_pNodeAnimator->Render(this, ac);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            pChildAnimNode->Render(ac);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::Animate(const SAnimContext& animContext)
{
    if (m_pNodeAnimator && IsActive())
    {
        m_pNodeAnimator->Animate(this, animContext);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            pChildAnimNode->Animate(animContext);
        }
    }

    UpdateTrackGizmo();
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::SetName(const char* pName)
{
    // Check if the node's director already contains a node with this name
    CTrackViewAnimNode* director = GetDirector();
    director = director ? director : GetSequence();

    CTrackViewAnimNodeBundle nodes = director->GetAnimNodesByName(pName);
    const uint numNodes = nodes.GetCount();
    for (uint i = 0; i < numNodes; ++i)
    {
        if (nodes.GetNode(i) != this)
        {
            return false;
        }
    }

    string oldName = GetName();
    m_animNode->SetName(pName);

    CTrackViewSequence* sequence = GetSequence();
    AZ_Assert(sequence, "Nodes should never have a null sequence.");

    sequence->OnNodeRenamed(this, oldName);

    return true;
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::CanBeRenamed() const
{
    return (GetFlags() & eAnimNodeFlags_CanChangeName) != 0;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetNodeEntity(CEntityObject* pEntity)
{
    bool entityPointerChanged = (pEntity != m_pNodeEntity);

    m_pNodeEntity = pEntity;

    if (pEntity)
    {
        const EntityGUID guid = ToEntityGuid(pEntity->GetId());
        SetEntityGuid(guid);

        if (m_animNode->GetType() == AnimNodeType::AzEntity)
        {
            // We're connecting to a new AZ::Entity
            AZ::EntityId    sequenceComponentEntityId(m_animSequence->GetSequenceEntityId());

            AZ::EntityId id(AZ::EntityId::InvalidEntityId);
            AzToolsFramework::ComponentEntityObjectRequestBus::EventResult(id, pEntity, &AzToolsFramework::ComponentEntityObjectRequestBus::Events::GetAssociatedEntityId);

            if (!id.IsValid() && m_animNode->GetAzEntityId().IsValid())
            {
                // When undoing, pEntity may not have an associated entity Id. Fall back to our stored entityId if we have one.
                id = m_animNode->GetAzEntityId();
            }

            // Notify the SequenceComponent that we're binding an entity to the sequence
            Maestro::EditorSequenceComponentRequestBus::Event(sequenceComponentEntityId, &Maestro::EditorSequenceComponentRequestBus::Events::AddEntityToAnimate, id);

            if (id != m_animNode->GetAzEntityId())
            {
                if (m_animNode->GetAzEntityId().IsValid())
                {
                    // disconnect from bus with previous entity ID before we reset it
                    AZ::EntityBus::Handler::BusDisconnect(m_animNode->GetAzEntityId());
                    AZ::TransformNotificationBus::Handler::BusDisconnect(m_animNode->GetAzEntityId());
                }

                m_animNode->SetAzEntityId(id);
            }

            // connect to EntityBus for OnEntityActivated() notifications to sync components on the entity
            if (!AZ::EntityBus::Handler::BusIsConnectedId(m_animNode->GetAzEntityId()))
            {
                AZ::EntityBus::Handler::BusConnect(m_animNode->GetAzEntityId());
            }

            if (!AZ::TransformNotificationBus::Handler::BusIsConnectedId(m_animNode->GetAzEntityId()))
            {
                AZ::TransformNotificationBus::Handler::BusConnect(m_animNode->GetAzEntityId());
            }
        }

        if (qobject_cast<CEntityObject*>(pEntity->GetLookAt()))
        {
            CEntityObject* target = static_cast<CEntityObject*>(pEntity->GetLookAt());
            SetEntityGuidTarget(ToEntityGuid(target->GetId()));
        }
        if (qobject_cast<CEntityObject*>(pEntity->GetLookAtSource()))
        {
            CEntityObject* source = static_cast<CEntityObject*>(pEntity->GetLookAtSource());
            SetEntityGuidSource(ToEntityGuid(source->GetId()));
        }

        if (entityPointerChanged)
        {
            SetPosRotScaleTracksDefaultValues();
        }

        OnSelectionChanged(pEntity->IsSelected());
    }
}

//////////////////////////////////////////////////////////////////////////
CEntityObject* CTrackViewAnimNode::GetNodeEntity(const bool bSearch)
{
    CEntityObject* entityObject = nullptr;

    if (m_animNode)
    {
        if (m_pNodeEntity)
        {
            entityObject = m_pNodeEntity;
        }
        else if (bSearch)
        {
            // First Search with object manager
            EntityGUID* pGuid = GetEntityGuid();
            entityObject = static_cast<CEntityObject*>(GetIEditor()->GetObjectManager()->FindAnimNodeOwner(this));

            // if not found, search with AZ::EntityId
            if (!entityObject && IsBoundToAzEntity())
            {
                // Search AZ::Entities
                AzToolsFramework::ComponentEntityEditorRequestBus::EventResult(entityObject, GetAzEntityId(), &AzToolsFramework::ComponentEntityEditorRequestBus::Events::GetSandboxObject);
            }
        }
    }

    return entityObject;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAllAnimNodes()
{
    CTrackViewAnimNodeBundle bundle;

    if (GetNodeType() == eTVNT_AnimNode)
    {
        bundle.AppendAnimNode(this);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            bundle.AppendAnimNodeBundle(pChildAnimNode->GetAllAnimNodes());
        }
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNodeBundle CTrackViewAnimNode::GetSelectedAnimNodes()
{
    CTrackViewAnimNodeBundle bundle;

    if ((GetNodeType() == eTVNT_AnimNode || GetNodeType() == eTVNT_Sequence) && IsSelected())
    {
        bundle.AppendAnimNode(this);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            bundle.AppendAnimNodeBundle(pChildAnimNode->GetSelectedAnimNodes());
        }
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAllOwnedNodes(const CEntityObject* owner)
{
    CTrackViewAnimNodeBundle bundle;

    if (GetNodeType() == eTVNT_AnimNode && GetNodeEntity() == owner)
    {
        bundle.AppendAnimNode(this);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            bundle.AppendAnimNodeBundle(pChildAnimNode->GetAllOwnedNodes(owner));
        }
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAnimNodesByType(AnimNodeType animNodeType)
{
    CTrackViewAnimNodeBundle bundle;

    if (GetNodeType() == eTVNT_AnimNode && GetType() == animNodeType)
    {
        bundle.AppendAnimNode(this);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            bundle.AppendAnimNodeBundle(pChildAnimNode->GetAnimNodesByType(animNodeType));
        }
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNodeBundle CTrackViewAnimNode::GetAnimNodesByName(const char* pName)
{
    CTrackViewAnimNodeBundle bundle;

    QString nodeName = GetName();
    if (GetNodeType() == eTVNT_AnimNode && QString::compare(pName, nodeName, Qt::CaseInsensitive) == 0)
    {
        bundle.AppendAnimNode(this);
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            bundle.AppendAnimNodeBundle(pChildAnimNode->GetAnimNodesByName(pName));
        }
    }

    return bundle;
}

//////////////////////////////////////////////////////////////////////////
const char* CTrackViewAnimNode::GetParamName(const CAnimParamType& paramType) const
{
    const char* pName = m_animNode->GetParamName(paramType);
    return pName ? pName : "";
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsGroupNode() const
{
    AnimNodeType nodeType = GetType();

    // AZEntities are really just containers for components, so considered a 'Group' node
    return nodeType == AnimNodeType::Director || nodeType == AnimNodeType::Group || nodeType == AnimNodeType::AzEntity;
}

//////////////////////////////////////////////////////////////////////////
QString CTrackViewAnimNode::GetAvailableNodeNameStartingWith(const QString& name) const
{
    QString newName = name;
    unsigned int index = 2;

    while (const_cast<CTrackViewAnimNode*>(this)->GetAnimNodesByName(newName.toUtf8().data()).GetCount() > 0)
    {
        newName = QStringLiteral("%1%2").arg(name).arg(index);
        ++index;
    }

    return newName;
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNodeBundle CTrackViewAnimNode::AddSelectedEntities(const AZStd::vector<AnimParamType>& tracks)
{
    AZ_Assert(IsGroupNode(), "Expected to added selected entities to a group node.");

    CTrackViewAnimNodeBundle addedNodes;

    // Add selected nodes.
    CSelectionGroup* selection = GetIEditor()->GetSelection();
    for (int i = 0; i < selection->GetCount(); i++)
    {
        CBaseObject* object = selection->GetObject(i);
        if (!object)
        {
            continue;
        }

        // Check if object already assigned to some AnimNode.
        CTrackViewAnimNode* existingNode = GetIEditor()->GetSequenceManager()->GetActiveAnimNode(static_cast<const CEntityObject*>(object));
        if (existingNode)
        {
            // If it has the same director than the current node, reject it
            if (existingNode->GetDirector() == GetDirector())
            {
                GetIEditor()->GetMovieSystem()->LogUserNotificationMsg(AZStd::string::format("'%s' was already added to '%s', skipping...", object->GetName().toUtf8().data(), GetDirector()->GetName()));
                continue;
            }
        }

        CTrackViewAnimNode* animNode = CreateSubNode(object->GetName(), AnimNodeType::AzEntity, static_cast<CEntityObject*>(object));

        if (animNode)
        {
            CUndo undo("Add Default Tracks");

            CreateDefaultTracksForEntityNode(animNode, tracks);

            addedNodes.AppendAnimNode(animNode);
        }
    }

    return addedNodes;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::AddCurrentLayer()
{
    assert(IsGroupNode());

    CObjectLayerManager* pLayerManager = GetIEditor()->GetObjectManager()->GetLayersManager();
    CObjectLayer* pLayer = pLayerManager->GetCurrentLayer();
    const QString name = pLayer->GetName();

    CreateSubNode(name, AnimNodeType::Entity);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetEntityGuidTarget(const EntityGUID& guid)
{
    if (m_animNode)
    {
        m_animNode->SetEntityGuidTarget(guid);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetEntityGuid(const EntityGUID& guid)
{
    if (m_animNode)
    {
        m_animNode->SetEntityGuid(guid);
    }
}

//////////////////////////////////////////////////////////////////////////
EntityGUID* CTrackViewAnimNode::GetEntityGuid() const
{
    return m_animNode ? m_animNode->GetEntityGuid() : nullptr;
}

//////////////////////////////////////////////////////////////////////////
IEntity* CTrackViewAnimNode::GetEntity() const
{
    return m_animNode ? m_animNode->GetEntity() : nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetEntityGuidSource(const EntityGUID& guid)
{
    if (m_animNode)
    {
        m_animNode->SetEntityGuidSource(guid);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetAsViewCamera()
{
    assert (GetType() == AnimNodeType::Camera);

    if (GetType() == AnimNodeType::Camera)
    {
        CEntityObject* pCameraEntity = GetNodeEntity();
        CRenderViewport* pRenderViewport = static_cast<CRenderViewport*>(GetIEditor()->GetViewManager()->GetGameViewport());
        pRenderViewport->SetCameraObject(pCameraEntity);
    }
}

//////////////////////////////////////////////////////////////////////////
unsigned int CTrackViewAnimNode::GetParamCount() const
{
    return m_animNode ? m_animNode->GetParamCount() : 0;
}

//////////////////////////////////////////////////////////////////////////
CAnimParamType CTrackViewAnimNode::GetParamType(unsigned int index) const
{
    unsigned int paramCount = GetParamCount();
    if (!m_animNode || index >= paramCount)
    {
        return AnimParamType::Invalid;
    }

    return m_animNode->GetParamType(index);
}

//////////////////////////////////////////////////////////////////////////
IAnimNode::ESupportedParamFlags CTrackViewAnimNode::GetParamFlags(const CAnimParamType& paramType) const
{
    if (m_animNode)
    {
        return m_animNode->GetParamFlags(paramType);
    }

    return IAnimNode::ESupportedParamFlags(0);
}

//////////////////////////////////////////////////////////////////////////
AnimValueType CTrackViewAnimNode::GetParamValueType(const CAnimParamType& paramType) const
{
    if (m_animNode)
    {
        return m_animNode->GetParamValueType(paramType);
    }

    return AnimValueType::Unknown;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::UpdateDynamicParams()
{
    if (m_animNode)
    {
        m_animNode->UpdateDynamicParams();
    }

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);
            pChildAnimNode->UpdateDynamicParams();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::CopyKeysToClipboard(XmlNodeRef& xmlNode, const bool bOnlySelectedKeys, const bool bOnlyFromSelectedTracks)
{
    XmlNodeRef childNode = xmlNode->createNode("Node");
    childNode->setAttr("name", GetName());
    childNode->setAttr("type", static_cast<int>(GetType()));

    for (auto iter = m_childNodes.begin(); iter != m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        pChildNode->CopyKeysToClipboard(childNode, bOnlySelectedKeys, bOnlyFromSelectedTracks);
    }

    if (childNode->getChildCount() > 0)
    {
        xmlNode->addChild(childNode);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::CopyNodesToClipboard(const bool bOnlySelected, QWidget* context)
{
    XmlNodeRef animNodesRoot = XmlHelpers::CreateXmlNode("CopyAnimNodesRoot");

    CopyNodesToClipboardRec(this, animNodesRoot, bOnlySelected);

    CClipboard clipboard(context);
    clipboard.Put(animNodesRoot, "Track view entity nodes");
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::CopyNodesToClipboardRec(CTrackViewAnimNode* pCurrentAnimNode, XmlNodeRef& xmlNode, const bool bOnlySelected)
{
    if (pCurrentAnimNode->m_animNode && (!bOnlySelected || pCurrentAnimNode->IsSelected()))
    {
        XmlNodeRef childXmlNode = xmlNode->newChild("Node");
        pCurrentAnimNode->m_animNode->Serialize(childXmlNode, false, true);
    }

    for (auto iter = pCurrentAnimNode->m_childNodes.begin(); iter != pCurrentAnimNode->m_childNodes.end(); ++iter)
    {
        CTrackViewNode* pChildNode = (*iter).get();
        if (pChildNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* pChildAnimNode = static_cast<CTrackViewAnimNode*>(pChildNode);

            // If selected and group node, force copying of children
            const bool bSelectedAndGroupNode = pCurrentAnimNode->IsSelected() && pCurrentAnimNode->IsGroupNode();
            CopyNodesToClipboardRec(pChildAnimNode, xmlNode, !bSelectedAndGroupNode && bOnlySelected);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::PasteTracksFrom(XmlNodeRef& xmlNodeWithTracks)
{
    assert(CUndo::IsRecording());

    // we clear our own tracks first because calling SerializeAnims() will clear out m_animNode's tracks below
    CTrackViewTrackBundle allTracksBundle = GetAllTracks();
    for (int i = allTracksBundle.GetCount(); --i >= 0;)
    {
        RemoveTrack(allTracksBundle.GetTrack(i));
    }

    // serialize all the tracks from xmlNode - note this will first delete all existing tracks on m_animNode
    m_animNode->SerializeAnims(xmlNodeWithTracks, true, true);

    // create TrackView tracks
    const int trackCount = m_animNode->GetTrackCount();
    for (int i = 0; i < trackCount; ++i)
    {
        IAnimTrack* pTrack = m_animNode->GetTrackByIndex(i);

        CTrackViewTrackFactory trackFactory;
        CTrackViewTrack* newTrackNode = trackFactory.BuildTrack(pTrack, this, this);

        AddNode(newTrackNode);

        MarkAsModified();
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::PasteNodesFromClipboard(QWidget* context)
{
    assert(CUndo::IsRecording());

    CClipboard clipboard(context);
    if (clipboard.IsEmpty())
    {
        return false;
    }

    XmlNodeRef animNodesRoot = clipboard.Get();
    if (animNodesRoot == NULL || strcmp(animNodesRoot->getTag(), "CopyAnimNodesRoot") != 0)
    {
        return false;
    }

    const bool bLightAnimationSetActive = GetSequence()->GetFlags() & IAnimSequence::eSeqFlags_LightAnimationSet;

    AZStd::map<int, IAnimNode*> copiedIdToNodeMap;
    const unsigned int numNodes = animNodesRoot->getChildCount();
    for (int i = 0; i < numNodes; ++i)
    {
        XmlNodeRef xmlNode = animNodesRoot->getChild(i);

        // skip non-light nodes in light animation sets
        int type;
        if (!xmlNode->getAttr("Type", type) || (bLightAnimationSetActive && (AnimNodeType)type != AnimNodeType::Light))
        {
            continue;
        }

        PasteNodeFromClipboard(copiedIdToNodeMap, xmlNode);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::PasteNodeFromClipboard(AZStd::map<int, IAnimNode*>& copiedIdToNodeMap, XmlNodeRef xmlNode)
{
    QString name;

    if (!xmlNode->getAttr("Name", name))
    {
        return;
    }

    // can only paste nodes into a groupNode (i.e. accepts children)
    const bool bIsGroupNode = IsGroupNode();
    assert(bIsGroupNode);
    if (!bIsGroupNode)
    {
        return;
    }

    AnimNodeType nodeType;
    GetIEditor()->GetMovieSystem()->SerializeNodeType(nodeType, xmlNode, /*bLoading=*/ true, IAnimSequence::kSequenceVersion, m_animSequence->GetFlags());
    
    if (nodeType == AnimNodeType::Component)
    {
        // When pasting Component Nodes, the parent Component Entity Node would have already added all its Components as part of its OnEntityActivated() sync.
        // Here we need to go copy any Components Tracks as well. For pasting, Components matched by ComponentId, which assumes the pasted Track View Component Node
        // refers to the copied Component Entity referenced in the level

        // Find the pasted parent Component Entity Node
        int parentId = 0;
        xmlNode->getAttr("ParentNode", parentId);
        if (copiedIdToNodeMap.find(parentId) != copiedIdToNodeMap.end())
        {
            CTrackViewAnimNode* componentEntityNode = FindNodeByAnimNode(copiedIdToNodeMap[parentId]);
            if (componentEntityNode)
            {
                // Find the copied Component Id on the pasted Component Entity Node, if it exists
                AZ::ComponentId componentId = AZ::InvalidComponentId;
                xmlNode->getAttr("ComponentId", componentId);

                for (int i = componentEntityNode->GetChildCount(); --i >= 0;)
                {
                    CTrackViewNode* childNode = componentEntityNode->GetChild(i);
                    if (childNode->GetNodeType() == eTVNT_AnimNode)
                    {
                        CTrackViewAnimNode* componentNode = static_cast<CTrackViewAnimNode*>(childNode);

                        if (componentNode->GetComponentId() == componentId)
                        {
                            componentNode->PasteTracksFrom(xmlNode);
                            break;
                        }
                    }
                }
            }
        }   
    }
    else
    {
        // Pasting a non-Component Node - create and add nodes to CryMovie and TrackView

        // Check if the node's director or sequence already contains a node with this name
        CTrackViewAnimNode* director = GetDirector();
        director = director ? director : GetSequence();
        if (director->GetAnimNodesByName(name.toUtf8().data()).GetCount() > 0)
        {
            return;
        }

        IAnimNode* newAnimNode = m_animSequence->CreateNode(xmlNode);
        if (!newAnimNode)
        {
            return;
        }

        // add new node to mapping of copied Id's to pasted nodes
        int id;
        xmlNode->getAttr("Id", id);
        copiedIdToNodeMap[id] = newAnimNode;

        // search for the parent Node among the pasted nodes - if not found, parent to the group node doing the pasting
        IAnimNode* parentAnimNode = m_animNode.get();
        int parentId = 0;
        if (xmlNode->getAttr("ParentNode", parentId))
        {
            if (copiedIdToNodeMap.find(parentId) != copiedIdToNodeMap.end())
            {
                parentAnimNode = copiedIdToNodeMap[parentId];
            }
        }
        newAnimNode->SetParent(parentAnimNode);

        // Find the TrackViewNode corresponding to the parentNode
        CTrackViewAnimNode* parentNode = FindNodeByAnimNode(parentAnimNode);
        if (!parentNode)
        {
            parentNode = this;
        }

        CTrackViewAnimNodeFactory animNodeFactory;
        CTrackViewAnimNode* newNode = animNodeFactory.BuildAnimNode(m_animSequence, newAnimNode, parentNode);

        parentNode->AddNode(newNode);

        // Add node to sequence, let AZ Undo take care of undo/redo
        m_animSequence->AddNode(newNode->m_animNode.get());
    }

    // Make sure there are no duplicate track Ids
    AZStd::vector<unsigned int> usedTrackIds;

    int nodeCount = m_animSequence->GetNodeCount();
    for (int nodeIndex = 0; nodeIndex < nodeCount; nodeIndex++)
    {
        IAnimNode* animNode = m_animSequence->GetNode(nodeIndex);
        AZ_Assert(animNode, "Expected valid animNode");

        int trackCount = animNode->GetTrackCount();
        for (int trackIndex = 0; trackIndex < trackCount; trackIndex++)
        {
            IAnimTrack* track = animNode->GetTrackByIndex(trackIndex);
            AZ_Assert(track, "Expected valid track");

            // If the Track Id is already used, generate a new one
            if (AZStd::find(usedTrackIds.begin(), usedTrackIds.end(), track->GetId()) != usedTrackIds.end())
            {
                track->SetId(m_animSequence->GetUniqueTrackIdAndGenerateNext());
            }

            usedTrackIds.push_back(track->GetId());

            int subTrackCount = track->GetSubTrackCount();
            for (int subTrackIndex = 0; subTrackIndex < subTrackCount; subTrackIndex++)
            {
                IAnimTrack* subTrack = track->GetSubTrack(subTrackIndex);
                AZ_Assert(subTrack, "Expected valid subtrack.");

                // If the Track Id is already used, generate a new one
                if (AZStd::find(usedTrackIds.begin(), usedTrackIds.end(), subTrack->GetId()) != usedTrackIds.end())
                {
                    subTrack->SetId(m_animSequence->GetUniqueTrackIdAndGenerateNext());
                }

                usedTrackIds.push_back(subTrack->GetId());
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNode* CTrackViewAnimNode::FindNodeByAnimNode(const IAnimNode* animNode)
{
    // Depth-first search for TrackViewAnimNode associated with the given animNode. Returns the first match found.
    CTrackViewAnimNode* retNode = nullptr;

    for (const std::unique_ptr<CTrackViewNode>& childNode : m_childNodes)
    {
        if (childNode->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* childAnimNode = static_cast<CTrackViewAnimNode*>(childNode.get());

            // recurse to search children of group nodes
            if (childNode->IsGroupNode())
            {
                retNode = childAnimNode->FindNodeByAnimNode(animNode);
                if (retNode)
                {
                    break;
                }
            }

            if (childAnimNode->GetAnimNode() == animNode)
            {
                retNode = childAnimNode;
                break;
            }
        }
    }
    return retNode;
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsValidReparentingTo(CTrackViewAnimNode* pNewParent)
{
    if (pNewParent == GetParentNode() || !pNewParent->IsGroupNode() || pNewParent->GetType() == AnimNodeType::AzEntity)
    {
        return false;
    }

    // Check if the new parent already contains a node with this name
    CTrackViewAnimNodeBundle foundNodes = pNewParent->GetAnimNodesByName(GetName());
    if (foundNodes.GetCount() > 1 || (foundNodes.GetCount() == 1 && foundNodes.GetNode(0) != this))
    {
        return false;
    }

    // Check if another node already owns this entity in the new parent's tree
    CEntityObject* owner = GetNodeEntity();
    if (owner)
    {
        CTrackViewAnimNodeBundle ownedNodes = pNewParent->GetAllOwnedNodes(owner);
        if (ownedNodes.GetCount() > 0 && ownedNodes.GetNode(0) != this)
        {
            return false;
        }
    }

    return true;
}

void CTrackViewAnimNode::SetParentsInChildren(CTrackViewAnimNode* currentNode)
{
    const uint numChildren = currentNode->GetChildCount();

    for (uint childIndex = 0; childIndex < numChildren; ++childIndex)
    {
        CTrackViewAnimNode* childAnimNode = static_cast<CTrackViewAnimNode*>(currentNode->GetChild(childIndex));

        if (childAnimNode->GetNodeType() != eTVNT_Track)
        {
            childAnimNode->m_animNode->SetParent(currentNode->m_animNode.get());

            if (childAnimNode->GetChildCount() > 0 && childAnimNode->GetNodeType() != eTVNT_AnimNode)
            {
                SetParentsInChildren(childAnimNode);
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetNewParent(CTrackViewAnimNode* newParent)
{
    if (newParent == GetParentNode())
    {
        return;
    }

    assert(IsValidReparentingTo(newParent));

    CTrackViewSequence* sequence = newParent->GetSequence();
    AZ_Assert(sequence, "Expected valid sequence.");

    AzToolsFramework::ScopedUndoBatch undoBatch("Set New Track View Anim Node Parent");

    UnBindFromEditorObjects();

    // Remove from the old parent's children and hang on to a ref.
    std::unique_ptr<CTrackViewNode> storedTrackViewNode;
    CTrackViewAnimNode* lastParent = static_cast<CTrackViewAnimNode*>(m_pParentNode);
    if (nullptr != lastParent)
    {
        for (auto iter = lastParent->m_childNodes.begin(); iter != lastParent->m_childNodes.end(); ++iter)
        {
            std::unique_ptr<CTrackViewNode>& currentNode = *iter;

            if (currentNode.get() == this)
            {
                currentNode.swap(storedTrackViewNode);
                lastParent->m_childNodes.erase(iter);
                break;
            }
        }
    }
    AZ_Assert(nullptr != storedTrackViewNode.get(), "Existing Parent of node not found");

    sequence->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Removed);

    // Set new parent
    m_pParentNode = newParent;
    m_animNode->SetParent(newParent->m_animNode.get());
    SetParentsInChildren(this);

    // Add node to the new parent's children.
    static_cast<CTrackViewAnimNode*>(m_pParentNode)->AddNode(storedTrackViewNode.release());

    BindToEditorObjects();

    undoBatch.MarkEntityDirty(sequence->GetSequenceComponentEntityId());    
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::CanBeEnabled() const
{
    bool canBeEnabled = true;
    // If this node was disabled because the component was disabled,
    // do not allow it to be re-enabled until that is resolved.
    if (m_animNode)
    {
        canBeEnabled = !(m_animNode->GetFlags() & eAnimNodeFlags_DisabledForComponent);
    }
    return canBeEnabled;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetDisabled(bool disabled)
{
    {
        CTrackViewSequence* sequence = GetSequence();
        AZ_Assert(sequence, "Expected valid sequence.");
        AZ_Assert(m_animNode, "Expected valid m_animNode.");

        if (disabled)
        {
            m_animNode->SetFlags(m_animNode->GetFlags() | eAnimNodeFlags_Disabled);
            sequence->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Disabled);

            // Call OnReset to disable the effects of the node.
            m_animNode->OnReset();
        }
        else
        {
            m_animNode->SetFlags(m_animNode->GetFlags() & ~eAnimNodeFlags_Disabled);
            sequence->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_Enabled);
        }
    }
    MarkAsModified();
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsDisabled() const
{
    return m_animNode ? m_animNode->GetFlags() & eAnimNodeFlags_Disabled : false;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetPos(const Vec3& position)
{
    const float time = GetSequence()->GetTime();
    CTrackViewTrack* track = GetTrackForParameter(AnimParamType::Position);
    CRenderViewport* renderViewport = static_cast<CRenderViewport*>(GetIEditor()->GetViewManager()->GetGameViewport());

    if (track)
    {
        if (!GetIEditor()->GetAnimation()->IsRecording())
        {
            // Offset all keys by move amount.
            Vec3 offset = m_animNode->GetOffsetPosition(position);
            
            track->OffsetKeyPosition(offset);

            GetSequence()->OnKeysChanged();
        }
        else if (m_pNodeEntity->IsSelected() || renderViewport->GetCameraObject() == m_pNodeEntity)
        {
            CTrackViewSequence* sequence = GetSequence();

            AZ_Assert(sequence, "Expected valid sequence");
            if (sequence != nullptr)
            {
                const int flags = m_animNode->GetFlags();
                // This is required because the entity movement system uses Undo to
                // undo a previous move delta as the entity is dragged.
                CUndo::Record(new CUndoComponentEntityTrackObject(track));

                // Set the selected flag to enable record when unselected camera is moved through viewport
                m_animNode->SetFlags(flags | eAnimNodeFlags_EntitySelected);
                m_animNode->SetPos(sequence->GetTime(), position);
                m_animNode->SetFlags(flags);
                    
                // We don't want to use ScopedUndoBatch here because we don't want a separate Undo operation
                // generate for every frame as the user moves an entity.
                AzToolsFramework::ToolsApplicationRequests::Bus::Broadcast(
                    &AzToolsFramework::ToolsApplicationRequests::Bus::Events::AddDirtyEntity, 
                    sequence->GetSequenceComponentEntityId()
                );                    

                sequence->OnKeysChanged();
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetScale(const Vec3& scale)
{
    CTrackViewTrack* track = GetTrackForParameter(AnimParamType::Scale);

    if (GetIEditor()->GetAnimation()->IsRecording() && m_pNodeEntity->IsSelected() && track)
    {
        CTrackViewSequence* sequence = GetSequence();

        AZ_Assert(sequence, "Expected valid sequence");
        if (sequence != nullptr)
        {
            // This is required because the entity movement system uses Undo to
            // undo a previous move delta as the entity is dragged.
            CUndo::Record(new CUndoComponentEntityTrackObject(track));

            m_animNode->SetScale(sequence->GetTime(), scale);

            // We don't want to use ScopedUndoBatch here because we don't want a separate Undo operation
            // generate for every frame as the user scales an entity.
            AzToolsFramework::ToolsApplicationRequests::Bus::Broadcast(
                &AzToolsFramework::ToolsApplicationRequests::Bus::Events::AddDirtyEntity,
                sequence->GetSequenceComponentEntityId()
            );
            sequence->OnKeysChanged();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetRotation(const Quat& rotation)
{
    CTrackViewTrack* track = GetTrackForParameter(AnimParamType::Rotation);
    CRenderViewport* renderViewport = static_cast<CRenderViewport*>(GetIEditor()->GetViewManager()->GetGameViewport());

    if (GetIEditor()->GetAnimation()->IsRecording() && (m_pNodeEntity->IsSelected() || renderViewport->GetCameraObject() == m_pNodeEntity) && track)
    {
        CTrackViewSequence* sequence = GetSequence();

        AZ_Assert(sequence, "Expected valid sequence");
        if (sequence != nullptr)
        {
            const int flags = m_animNode->GetFlags();
            // This is required because the entity movement system uses Undo to
            // undo a previous move delta as the entity is dragged.
            CUndo::Record(new CUndoComponentEntityTrackObject(track));

            // Set the selected flag to enable record when unselected camera is moved through viewport
            m_animNode->SetFlags(flags | eAnimNodeFlags_EntitySelected);
            m_animNode->SetRotate(sequence->GetTime(), rotation);
            m_animNode->SetFlags(flags);

            // We don't want to use ScopedUndoBatch here because we don't want a separate Undo operation
            // generate for every frame as the user rotates an entity.
            AzToolsFramework::ToolsApplicationRequests::Bus::Broadcast(
                &AzToolsFramework::ToolsApplicationRequests::Bus::Events::AddDirtyEntity,
                sequence->GetSequenceComponentEntityId()
            );
            sequence->OnKeysChanged();
        }
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsActive()
{
    CTrackViewSequence* pSequence = GetSequence();
    const bool bInActiveSequence = pSequence ? GetSequence()->IsBoundToEditorObjects() : false;

    CTrackViewAnimNode* director = GetDirector();
    const bool bMemberOfActiveDirector = director ? GetDirector()->IsActiveDirector() : true;

    return bInActiveSequence && bMemberOfActiveDirector;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnSelectionChanged(const bool selected)
{
    if (m_animNode)
    {
        const AnimNodeType animNodeType = GetType();
        AZ_Assert(animNodeType == AnimNodeType::AzEntity, "Expected AzEntity for selection changed");

        const EAnimNodeFlags flags = (EAnimNodeFlags)m_animNode->GetFlags();
        m_animNode->SetFlags(selected ? (flags | eAnimNodeFlags_EntitySelected) : (flags & ~eAnimNodeFlags_EntitySelected));
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetPosRotScaleTracksDefaultValues(bool positionAllowed, bool rotationAllowed, bool scaleAllowed)
{
    const CEntityObject* entity = nullptr;
    bool entityIsBoundToEditorObjects = false;

    if (m_animNode)
    {
        if (m_animNode->GetType() == AnimNodeType::Component)
        {
            // get entity from the parent Component Entity
           CTrackViewNode* parentNode = GetParentNode();
           if (parentNode && parentNode->GetNodeType() == eTVNT_AnimNode)
           {
               CTrackViewAnimNode* parentAnimNode = static_cast<CTrackViewAnimNode*>(parentNode);
               if (parentAnimNode)
               {
                   entity = parentAnimNode->GetNodeEntity(false);
                   entityIsBoundToEditorObjects = parentAnimNode->IsBoundToEditorObjects();
               }
           }
        }
        else
        {
            // not a component - get the entity on this node directly
            entity = GetNodeEntity(false);
            entityIsBoundToEditorObjects = IsBoundToEditorObjects();
        }

        if (entity && entityIsBoundToEditorObjects)
        {
            const float time = GetSequence()->GetTime();
            if (positionAllowed)
            {
                m_animNode->SetPos(time, entity->GetPos());
            }
            if (rotationAllowed)
            {
                m_animNode->SetRotate(time, entity->GetRotation());
            }
            if (scaleAllowed)
            {
                m_animNode->SetScale(time, entity->GetScale());
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::UpdateTrackGizmo()
{
    if (IsActive() && m_pNodeEntity && !m_pNodeEntity->IsHidden())
    {
        if (!m_trackGizmo)
        {
            CTrackGizmo* pTrackGizmo = new CTrackGizmo;
            pTrackGizmo->SetAnimNode(this);
            m_trackGizmo = pTrackGizmo;
            GetIEditor()->GetObjectManager()->GetGizmoManager()->AddGizmo(m_trackGizmo);
        }
    }
    else
    {
        GetIEditor()->GetObjectManager()->GetGizmoManager()->RemoveGizmo(m_trackGizmo);
        m_trackGizmo = nullptr;
    }

    if (m_pNodeEntity && m_trackGizmo)
    {
        Matrix34 gizmoMatrix;
        gizmoMatrix.SetIdentity();

        if (GetType() == AnimNodeType::AzEntity)
        {
            // Key data are always relative to the parent (or world if there is no parent). So get the parent
            // entity id if there is one.
            AZ::EntityId parentId;
            AZ::TransformBus::EventResult(parentId, GetAzEntityId(), &AZ::TransformBus::Events::GetParentId);
            if (parentId.IsValid())
            {
                AZ::Transform azWorldTM = GetEntityWorldTM(parentId);
                gizmoMatrix = AZTransformToLYTransform(azWorldTM);
            }
        }
        else
        {
            // Legacy system.
            gizmoMatrix = m_pNodeEntity->GetParentAttachPointWorldTM();
        }

        m_trackGizmo->SetMatrix(gizmoMatrix);
    }
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::CheckTrackAnimated(const CAnimParamType& paramType) const
{
    if (!m_animNode)
    {
        return false;
    }

    CTrackViewTrack* pTrack = GetTrackForParameter(paramType);
    return pTrack && pTrack->GetKeyCount() > 0;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnNodeAnimated(IAnimNode* node)
{
    if (m_pNodeEntity)
    {
        m_pNodeEntity->InvalidateTM(0);
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnNodeVisibilityChanged(IAnimNode* node, const bool bHidden)
{
    if (m_pNodeEntity)
    {
        m_pNodeEntity->SetHidden(bHidden);

        // Need to do this to force recreation of gizmos
        bool bUnhideSelected = !m_pNodeEntity->IsHidden() && m_pNodeEntity->IsSelected();
        if (bUnhideSelected)
        {
            GetIEditor()->GetObjectManager()->UnselectObject(m_pNodeEntity);
            GetIEditor()->GetObjectManager()->SelectObject(m_pNodeEntity);
        }

        UpdateTrackGizmo();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnNodeReset(IAnimNode* node)
{
    if (gEnv->IsEditing() && m_pNodeEntity)
    {
        // If the node has an event track, one should also reload the script when the node is reset.
        CTrackViewTrack* pAnimTrack = GetTrackForParameter(AnimParamType::Event);
        if (pAnimTrack && pAnimTrack->GetKeyCount())
        {
            CEntityScript* script = m_pNodeEntity->GetScript();
            script->Reload();
            m_pNodeEntity->Reload(true);
        }
    }
}

void CTrackViewAnimNode::SetComponent(AZ::ComponentId componentId, const AZ::Uuid& componentTypeId)
{
    if (m_animNode)
    {
        m_animNode->SetComponent(componentId, componentTypeId);
    }
}

//////////////////////////////////////////////////////////////////////////
AZ::ComponentId CTrackViewAnimNode::GetComponentId() const
{
    return m_animNode ? m_animNode->GetComponentId() : AZ::InvalidComponentId;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::MarkAsModified()
{
    GetSequence()->MarkAsModified();
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::ContainsComponentWithId(AZ::ComponentId componentId) const
{
    bool retFound = false;

    if (GetType() == AnimNodeType::AzEntity)
    {
        // search for a matching componentId on all children
        for (int i = 0; i < GetChildCount(); i++)
        {
            CTrackViewNode* childNode = GetChild(i);
            if (childNode->GetNodeType() == eTVNT_AnimNode)
            {
                if (static_cast<CTrackViewAnimNode*>(childNode)->GetComponentId() == componentId)
                {
                    retFound = true;
                    break;
                }
            }
        }
    }

    return retFound;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnStartPlayInEditor()
{
    if (m_animSequence->GetSequenceEntityId().IsValid())
    {
        AZ::EntityId remappedId;
        AzToolsFramework::EditorEntityContextRequestBus::Broadcast(&AzToolsFramework::EditorEntityContextRequestBus::Events::MapEditorIdToRuntimeId, m_animSequence->GetSequenceEntityId(), remappedId);
            
        if (remappedId.IsValid())
        {
            // stash and remap the AZ::EntityId of the SequenceComponent entity to restore it when we switch back to Edit mode
            m_stashedAnimSequenceEditorAzEntityId = m_animSequence->GetSequenceEntityId();
            m_animSequence->SetSequenceEntityId(remappedId);
        }
    }

    if (m_animNode && m_animNode->GetAzEntityId().IsValid())
    {
        AZ::EntityId remappedId;
        AzToolsFramework::EditorEntityContextRequestBus::Broadcast(&AzToolsFramework::EditorEntityContextRequestBus::Events::MapEditorIdToRuntimeId, m_animNode->GetAzEntityId(), remappedId);

        if (remappedId.IsValid())
        {
            // stash the AZ::EntityId of the SequenceComponent entity to restore it when we switch back to Edit mode
            m_stashedAnimNodeEditorAzEntityId = m_animNode->GetAzEntityId();
            m_animNode->SetAzEntityId(remappedId);
        }
    }
    
    if (m_animNode)
    {
        m_animNode->OnStartPlayInEditor();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnStopPlayInEditor()
{
    // restore sequenceComponent entity Ids back to their original Editor Ids
    if (m_animSequence && m_stashedAnimSequenceEditorAzEntityId.IsValid())
    {
        m_animSequence->SetSequenceEntityId(m_stashedAnimSequenceEditorAzEntityId);

        // invalidate the stashed Id now that we've restored it
        m_stashedAnimSequenceEditorAzEntityId.SetInvalid();
    }

    if (m_animNode && m_stashedAnimNodeEditorAzEntityId.IsValid())
    {
        m_animNode->SetAzEntityId(m_stashedAnimNodeEditorAzEntityId);

        // invalidate the stashed Id now that we've restored it
        m_stashedAnimNodeEditorAzEntityId.SetInvalid();
    }

    if (m_animNode)
    {
        m_animNode->OnStopPlayInEditor();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnEntityActivated(const AZ::EntityId& activatedEntityId)
{
    if (GetAzEntityId() != activatedEntityId)
    {
        // This can happen when we're exiting Game/Sim Mode and entity Id's are remapped. Do nothing in such a circumstance.
        return;
    }

    CTrackViewDialog* dialog = CTrackViewDialog::GetCurrentInstance();
    if ((dialog && dialog->IsDoingUndoOperation()) || GetAzEntityId() != activatedEntityId)
    {
        // Do not respond during Undo. We'll get called when we connect to the AZ::EntityBus in SetEntityNode(),
        // which would result in adding component nodes twice during Undo.

        // Also do not respond to entity activation notifications for entities not associated with this animNode,
        // although this should never happen
        return;
    }

    // Ensure the components on the Entity match the components on the Entity Node in Track View.
    //
    // Note this gets called as soon as we connect to AZ::EntityBus - so in effect SetNodeEntity() on an AZ::Entity results
    // in all of it's component nodes being added.
    //
    // If the component exists in Track View but not in the entity, we remove it from Track View.

    AZ::Entity* entity = nullptr;
    AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, activatedEntityId);

    // check if all Track View components are (still) on the entity. If not, remove it from TrackView
    for (int i = GetChildCount(); --i >= 0;)
    {
        if (GetChild(i)->GetNodeType() == eTVNT_AnimNode)
        {
            CTrackViewAnimNode* childAnimNode = static_cast<CTrackViewAnimNode*>(GetChild(i));
            if (childAnimNode->GetComponentId() != AZ::InvalidComponentId && !(entity->FindComponent(childAnimNode->GetComponentId())))
            {
                // Check to see if the component is still on the entity, but just disabled. Don't remove it in that case.
                AZ::Entity::ComponentArrayType disabledComponents;
                AzToolsFramework::EditorDisabledCompositionRequestBus::Event(entity->GetId(), &AzToolsFramework::EditorDisabledCompositionRequests::GetDisabledComponents, disabledComponents);

                bool isDisabled = false;
                for (auto disabledComponent : disabledComponents)
                {
                    if (disabledComponent->GetId() == childAnimNode->GetComponentId())
                    {
                        isDisabled = true;
                        break;
                    }
                }

                // Check to see if the component is still on the entity, but just pending. Don't remove it in that case.
                AZ::Entity::ComponentArrayType pendingComponents;
                AzToolsFramework::EditorPendingCompositionRequestBus::Event(entity->GetId(), &AzToolsFramework::EditorPendingCompositionRequests::GetPendingComponents, pendingComponents);

                bool isPending = false;
                for (auto pendingComponent : pendingComponents)
                {
                    if (pendingComponent->GetId() == childAnimNode->GetComponentId())
                    {
                        isPending = true;
                        break;
                    }
                }

                if (!isDisabled && !isPending)
                {
                    AzToolsFramework::ScopedUndoBatch undoBatch("Remove Track View Component Node");
                    RemoveSubNode(childAnimNode);
                    CTrackViewSequence* sequence = GetSequence();
                    AZ_Assert(sequence != nullptr, "Sequence should not be null");
                    undoBatch.MarkEntityDirty(sequence->GetSequenceComponentEntityId());
                }
                else
                {
                    // don't remove this node, but do disable it.
                    if (childAnimNode->m_animNode)
                    {
                        int flags = childAnimNode->m_animNode->GetFlags();
                        flags |= eAnimNodeFlags_DisabledForComponent;
                        childAnimNode->m_animNode->SetFlags(flags);
                        childAnimNode->SetDisabled(true);
                    }
                }
            }
            else
            {
                // re-enable the node if it was disabled because of a missing component
                if (childAnimNode->m_animNode)
                {
                    int flags = childAnimNode->m_animNode->GetFlags();
                    if (flags & eAnimNodeFlags_DisabledForComponent)
                    {
                        flags &= ~eAnimNodeFlags_DisabledForComponent;
                        childAnimNode->m_animNode->SetFlags(flags);
                        childAnimNode->SetDisabled(false);
                    }
                }
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////
    // check that all animatable components on the Entity are in Track View

    AZStd::vector<AZ::ComponentId> animatableComponentIds;

    // Get all components animated through the behavior context
    Maestro::EditorSequenceComponentRequestBus::Event(GetSequence()->GetSequenceComponentEntityId(), &Maestro::EditorSequenceComponentRequestBus::Events::GetAnimatableComponents,
                                                          animatableComponentIds, activatedEntityId);

    // Append all components animated outside the behavior context
    AppendNonBehaviorAnimatableComponents(animatableComponentIds);

    for (const AZ::ComponentId& componentId : animatableComponentIds)
    {
        bool componentFound = false;
        for (int i = GetChildCount(); --i >= 0;)
        {
            if (GetChild(i)->GetNodeType() == eTVNT_AnimNode)
            {
                CTrackViewAnimNode* childAnimNode = static_cast<CTrackViewAnimNode*>(GetChild(i));
                if (childAnimNode->GetComponentId() == componentId)
                {
                    componentFound = true;
                    break;
                }
            }
        }
        if (!componentFound)
        {
            bool disabled = false;
            const AZ::Component* component = entity->FindComponent(componentId);

            // If not found in enabled components, check disabled and pending components
            if (!component)
            {
                // Disable the node when it is created because the component is not enabled.
                disabled = true;

                // Check in disabled components
                AZ::Entity::ComponentArrayType disabledComponents;
                AzToolsFramework::EditorDisabledCompositionRequestBus::Event(entity->GetId(), &AzToolsFramework::EditorDisabledCompositionRequests::GetDisabledComponents, disabledComponents);

                for (AZ::Component* currentComponent : disabledComponents)
                {
                    if (currentComponent->GetId() == componentId)
                    {
                        component = currentComponent;
                        break;
                    }
                }

                // Check in pending components
                if (!component)
                {
                    AZ::Entity::ComponentArrayType pendingComponents;
                    AzToolsFramework::EditorPendingCompositionRequestBus::Event(entity->GetId(), &AzToolsFramework::EditorPendingCompositionRequests::GetPendingComponents, pendingComponents);
                    for (AZ::Component* currentComponent : pendingComponents)
                    {
                        if (currentComponent->GetId() == componentId)
                        {
                            component = currentComponent;
                            break;
                        }
                    }
                }
            }

            if (component)
            {
                AddComponent(component, disabled);
            }
        }
    }

    // Refresh the sequence because things may have been enabled/disabled.
    GetSequence()->ForceAnimation();
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnEntityRemoved()
{
    // This is called by CTrackViewSequenceManager for both legacy and AZ Entities. When we deprecate legacy entities,
    // we could (should) probably handles this via ComponentApplicationEventBus::Events::OnEntityRemoved

    m_pNodeEntity = nullptr;    // invalidate cached node entity pointer

    if (IsBoundToAzEntity())
    {
        AZ::EntityId entityId = GetAzEntityId();
        AZ::TransformNotificationBus::Handler::BusDisconnect(entityId);
        AZ::EntityBus::Handler::BusDisconnect(entityId);
    }

    // notify the change. This leads to Track View updating it's UI to account for the entity removal
    GetSequence()->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_NodeOwnerChanged);
}


//////////////////////////////////////////////////////////////////////////
CTrackViewAnimNode* CTrackViewAnimNode::AddComponent(const AZ::Component* component, bool disabled)
{
    CTrackViewAnimNode* retNewComponentNode = nullptr;
    AZStd::string componentName;
    AZ::Uuid      componentTypeId(AZ::Uuid::CreateNull());

    AzFramework::ApplicationRequests::Bus::BroadcastResult(componentTypeId, &AzFramework::ApplicationRequests::Bus::Events::GetComponentTypeId, GetAzEntityId(), component->GetId());

    AzToolsFramework::EntityCompositionRequestBus::BroadcastResult(componentName, &AzToolsFramework::EntityCompositionRequests::GetComponentName, component);

    if (!componentName.empty() && !componentTypeId.IsNull())
    {
        CTrackViewSequence* sequence = GetSequence();
        AZ_Assert(sequence, "Expected valid sequence.");

        AzToolsFramework::ScopedUndoBatch undoBatch("Add TrackView Component");
        retNewComponentNode = CreateSubNode(componentName.c_str(), AnimNodeType::Component, nullptr, componentTypeId, component->GetId());
        undoBatch.MarkEntityDirty(sequence->GetSequenceComponentEntityId());
    }
    else
    {
        AZ_Warning("TrackView", false, "Could not determine component name or type for adding component - skipping...");
    }

    if (retNewComponentNode)
    {
        retNewComponentNode->SetDisabled(disabled);
    }

    return retNewComponentNode;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::MatrixInvalidated()
{
    UpdateTrackGizmo();
}

//////////////////////////////////////////////////////////////////////////
Vec3 CTrackViewAnimNode::GetTransformDelegatePos(const Vec3& basePos) const
{
    const Vec3 position = GetPos();

    return Vec3(CheckTrackAnimated(AnimParamType::PositionX) ? position.x : basePos.x,
        CheckTrackAnimated(AnimParamType::PositionY) ? position.y : basePos.y,
        CheckTrackAnimated(AnimParamType::PositionZ) ? position.z : basePos.z);
}

//////////////////////////////////////////////////////////////////////////
Quat CTrackViewAnimNode::GetTransformDelegateRotation(const Quat& baseRotation) const
{
    if (!CheckTrackAnimated(AnimParamType::Rotation))
    {
        return baseRotation;
    }

    // Pass the sequence time to get the rotation from the
    // track data if it is present. We don't want to go all the way out
    // to the current rotation in component transform because that would mean
    // we are going from Quat to Euler and then back to Quat and that could lead
    // to the data drifting away from the original value.
    Quat nodeRotation = GetRotation(GetSequenceConst()->GetTime());

    const Ang3 angBaseRotation(baseRotation);
    const Ang3 angNodeRotation(nodeRotation);
    return Quat(Ang3(CheckTrackAnimated(AnimParamType::RotationX) ? angNodeRotation.x : angBaseRotation.x,
            CheckTrackAnimated(AnimParamType::RotationY) ? angNodeRotation.y : angBaseRotation.y,
            CheckTrackAnimated(AnimParamType::RotationZ) ? angNodeRotation.z : angBaseRotation.z));
}

//////////////////////////////////////////////////////////////////////////
Vec3 CTrackViewAnimNode::GetTransformDelegateScale(const Vec3& baseScale) const
{
    const Vec3 scale = GetScale();

    return Vec3(CheckTrackAnimated(AnimParamType::ScaleX) ? scale.x : baseScale.x,
        CheckTrackAnimated(AnimParamType::ScaleY) ? scale.y : baseScale.y,
        CheckTrackAnimated(AnimParamType::ScaleZ) ? scale.z : baseScale.z);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetTransformDelegatePos(const Vec3& position)
{
    SetPos(position);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetTransformDelegateRotation(const Quat& rotation)
{
    SetRotation(rotation);
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::SetTransformDelegateScale(const Vec3& scale)
{
    SetScale(scale);
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsPositionDelegated() const
{
    const bool bDelegated = (GetIEditor()->GetAnimation()->IsRecording() && m_pNodeEntity->IsSelected() && GetTrackForParameter(AnimParamType::Position)) || CheckTrackAnimated(AnimParamType::Position);
    return bDelegated;
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsRotationDelegated() const
{
    const bool bDelegated = (GetIEditor()->GetAnimation()->IsRecording() && m_pNodeEntity->IsSelected() && GetTrackForParameter(AnimParamType::Rotation)) || CheckTrackAnimated(AnimParamType::Rotation);
    return bDelegated;
}

//////////////////////////////////////////////////////////////////////////
bool CTrackViewAnimNode::IsScaleDelegated() const
{
    const bool bDelegated = (GetIEditor()->GetAnimation()->IsRecording() && m_pNodeEntity->IsSelected() && GetTrackForParameter(AnimParamType::Scale)) || CheckTrackAnimated(AnimParamType::Scale);
    return bDelegated;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnDone()
{
    SetNodeEntity(nullptr);
    UpdateTrackGizmo();
}

//////////////////////////////////////////////////////////////////////////
AZ::Transform CTrackViewAnimNode::GetEntityWorldTM(const AZ::EntityId entityId)
{
    AZ::Entity* entity = nullptr;
    AZ::ComponentApplicationBus::BroadcastResult(entity, &AZ::ComponentApplicationBus::Events::FindEntity, entityId);

    AZ::Transform worldTM = AZ::Transform::Identity();
    if (entity != nullptr)
    {
        AZ::TransformInterface* transformInterface = entity->GetTransform();
        if (transformInterface != nullptr)
        {
            worldTM = transformInterface->GetWorldTM();
        }
    }

    return worldTM;
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::UpdateKeyDataAfterParentChanged(const AZ::Transform& oldParentWorldTM, const AZ::Transform& newParentWorldTM)
{
    // Update the Position, Rotation and Scale tracks.
    AZStd::vector<AnimParamType> animParamTypes{ AnimParamType::Position, AnimParamType::Rotation, AnimParamType::Scale };
    for (AnimParamType animParamType : animParamTypes)
    {
        CTrackViewTrack* track = GetTrackForParameter(animParamType);
        if (track != nullptr)
        {
            track->UpdateKeyDataAfterParentChanged(oldParentWorldTM, newParentWorldTM);
        }
    }

    // Refresh after key data changed or parent changed.
    CTrackViewSequence* sequence = GetSequence();
    if (sequence != nullptr)
    {
        sequence->OnKeysChanged();
    }
}

//////////////////////////////////////////////////////////////////////////
void CTrackViewAnimNode::OnParentChanged(AZ::EntityId oldParent, AZ::EntityId newParent)
{
    // If the change is from no parent to parent, or the other way around,
    // update the key data, because that action is like going from world space to
    // relative to a new parent.
    if (!oldParent.IsValid() || !newParent.IsValid())
    {
        // Get the world transforms, Identity if there was no parent
        AZ::Transform oldParentWorldTM = GetEntityWorldTM(oldParent);
        AZ::Transform newParentWorldTM = GetEntityWorldTM(newParent);

        UpdateKeyDataAfterParentChanged(oldParentWorldTM, newParentWorldTM);
    }

    // Refresh after key data changed or parent changed.
    CTrackViewSequence* sequence = GetSequence();
    if (sequence != nullptr)
    {
        sequence->OnNodeChanged(this, ITrackViewSequenceListener::eNodeChangeType_NodeOwnerChanged);
    }
}
