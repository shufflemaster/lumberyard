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

#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenuActions/NodeGroupMenuActions/NodeGroupContextMenuActions.h>

#include <GraphCanvas/Components/GridBus.h>
#include <GraphCanvas/Components/Nodes/Comment/CommentBus.h>
#include <GraphCanvas/Components/SceneBus.h>
#include <GraphCanvas/GraphCanvasBus.h>

#include <GraphCanvas/Utils/GraphUtils.h>

namespace GraphCanvas
{
    //////////////////////////////
    // CreateNodeGroupMenuAction
    //////////////////////////////

    CreateNodeGroupMenuAction::CreateNodeGroupMenuAction(QObject* parent, bool collapseGroup)
        : NodeGroupContextMenuAction("", parent)
        , m_collapseGroup(collapseGroup)
    {
        if (!collapseGroup)
        {
            setText("Group");
            setToolTip("Will create a Node Group for the selected nodes.");
        }
        else
        {
            setText("Group [Collapsed]");
            setToolTip("Will create a Node Group for the selected nodes, and then collapse the group to a single node.");
        }
    }

    void CreateNodeGroupMenuAction::RefreshAction()
    {
        const GraphId& graphId = GetGraphId();
        const AZ::EntityId& targetId = GetTargetId();

        bool hasSelection = false;
        SceneRequestBus::EventResult(hasSelection, graphId, &SceneRequests::HasSelectedItems);

        if (hasSelection)
        {
            hasSelection = !GraphUtils::IsNodeGroup(targetId);
        }

        setEnabled(hasSelection);
    }

    ContextMenuAction::SceneReaction CreateNodeGroupMenuAction::TriggerAction(const AZ::Vector2& scenePos)
    {
        const GraphId& graphId = GetGraphId();

        bool hasSelection = false;
        SceneRequestBus::EventResult(hasSelection, graphId, &SceneRequests::HasSelectedItems);

        AZ::Entity* nodeGroupEntity = nullptr;
        GraphCanvasRequestBus::BroadcastResult(nodeGroupEntity, &GraphCanvasRequests::CreateNodeGroupAndActivate);

        if (nodeGroupEntity)
        {
            SceneRequestBus::Event(graphId, &SceneRequests::AddNode, nodeGroupEntity->GetId(), scenePos);

            if (hasSelection)
            {
                AZStd::vector< AZ::EntityId > selectedNodes;
                SceneRequestBus::EventResult(selectedNodes, graphId, &SceneRequests::GetSelectedNodes);

                QGraphicsItem* rootItem = nullptr;
                QRectF boundingArea;

                for (const AZ::EntityId& selectedNode : selectedNodes)
                {
                    SceneMemberUIRequestBus::EventResult(rootItem, selectedNode, &SceneMemberUIRequests::GetRootGraphicsItem);

                    if (rootItem)
                    {
                        if (boundingArea.isEmpty())
                        {
                            boundingArea = rootItem->sceneBoundingRect();
                        }
                        else
                        {
                            boundingArea = boundingArea.united(rootItem->sceneBoundingRect());
                        }
                    }
                }

                AZ::Vector2 gridStep;
                AZ::EntityId grid;
                SceneRequestBus::EventResult(grid, graphId, &SceneRequests::GetGrid);

                GridRequestBus::EventResult(gridStep, grid, &GridRequests::GetMinorPitch);

                boundingArea.adjust(-gridStep.GetX(), -gridStep.GetY(), gridStep.GetX(), gridStep.GetY());

                NodeGroupRequestBus::Event(nodeGroupEntity->GetId(), &NodeGroupRequests::SetGroupSize, boundingArea);
            }

            SceneRequestBus::Event(graphId, &SceneRequests::ClearSelection);

            if (!m_collapseGroup)
            {
                SceneMemberUIRequestBus::Event(nodeGroupEntity->GetId(), &SceneMemberUIRequests::SetSelected, true);
                CommentUIRequestBus::Event(nodeGroupEntity->GetId(), &CommentUIRequests::SetEditable, true);
            }
            else
            {
                NodeGroupRequestBus::Event(nodeGroupEntity->GetId(), &NodeGroupRequests::CollapseGroup);
            }
        }

        if (nodeGroupEntity)
        {
            return SceneReaction::PostUndo;
        }
        else
        {
            return SceneReaction::Nothing;
        }
    }

    ///////////////////////////////
    // UngroupNodeGroupMenuAction
    ///////////////////////////////

    UngroupNodeGroupMenuAction::UngroupNodeGroupMenuAction(QObject* parent)
        : NodeGroupContextMenuAction("Ungroup", parent)
    {

    }

    void UngroupNodeGroupMenuAction::RefreshAction()
    {
        const AZ::EntityId& targetId = GetTargetId();

        setEnabled((GraphUtils::IsNodeGroup(targetId) || GraphUtils::IsCollapsedNodeGroup(targetId)));
    }

    ContextMenuAction::SceneReaction UngroupNodeGroupMenuAction::TriggerAction(const AZ::Vector2& scenePos)
    {
        const GraphId& graphId = GetGraphId();        
        AZ::EntityId groupTarget = GetTargetId();

        if (GraphUtils::IsCollapsedNodeGroup(groupTarget))
        {
            CollapsedNodeGroupRequestBus::EventResult(groupTarget, groupTarget, &CollapsedNodeGroupRequests::GetSourceGroup);
        }

        SceneReaction reaction = SceneReaction::Nothing;

        if (groupTarget.IsValid())
        {
            NodeGroupRequestBus::Event(groupTarget, &NodeGroupRequests::UngroupGroup);
            reaction = SceneReaction::PostUndo;
        }

        return reaction;
    }

    ////////////////////////////////
    // CollapseNodeGroupMenuAction
    ////////////////////////////////

    CollapseNodeGroupMenuAction::CollapseNodeGroupMenuAction(QObject* parent)
        : NodeGroupContextMenuAction("Collapse", parent)
    {
        setToolTip("Collapses the selected group");
    }

    void CollapseNodeGroupMenuAction::RefreshAction()
    {
        const AZ::EntityId& targetId = GetTargetId();

        setEnabled(GraphUtils::IsNodeGroup(targetId));
    }

    ContextMenuAction::SceneReaction CollapseNodeGroupMenuAction::TriggerAction(const AZ::Vector2& scenePos)
    {
        const AZ::EntityId& targetId = GetTargetId();

        NodeGroupRequestBus::Event(targetId, &NodeGroupRequests::CollapseGroup);

        return SceneReaction::PostUndo;
    }

    //////////////////////////////
    // ExpandNodeGroupMenuAction
    //////////////////////////////

    ExpandNodeGroupMenuAction::ExpandNodeGroupMenuAction(QObject* parent)
        : NodeGroupContextMenuAction("Expand", parent)
    {
        setToolTip("Expands the selected group");
    } 

    void ExpandNodeGroupMenuAction::RefreshAction()
    {
        const AZ::EntityId& targetId = GetTargetId();
        setEnabled(GraphUtils::IsCollapsedNodeGroup(targetId));
    }

    ContextMenuAction::SceneReaction ExpandNodeGroupMenuAction::TriggerAction(const AZ::Vector2& scenePos)
    {
        const AZ::EntityId& targetId = GetTargetId();
        CollapsedNodeGroupRequestBus::Event(targetId, &CollapsedNodeGroupRequests::ExpandGroup);

        return SceneReaction::PostUndo;
    }

    /////////////////////////////
    // EditGroupTitleMenuAction
    /////////////////////////////

    EditGroupTitleMenuAction::EditGroupTitleMenuAction(QObject* parent)
        : NodeGroupContextMenuAction("Edit group title", parent)
    {
        setToolTip("Edits the selected group title");
    }

    void EditGroupTitleMenuAction::RefreshAction()
    {
        const AZ::EntityId& targetId = GetTargetId();

        setEnabled(GraphUtils::IsNodeGroup(targetId));
    }

    ContextMenuAction::SceneReaction EditGroupTitleMenuAction::TriggerAction(const AZ::Vector2& scenePos)
    {
        const AZ::EntityId& targetId = GetTargetId();

        CommentUIRequestBus::Event(targetId, &CommentUIRequests::SetEditable, true);

        return SceneReaction::Nothing;
    }
}