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

#pragma once

#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Vector2.h>

#include <QMenu>
#include <QWidgetAction>

#include <GraphCanvas/Widgets/EditorContextMenu/EditorContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/SceneContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/ConnectionContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenus/SlotContextMenu.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenuActions/SceneMenuActions/SceneContextMenuAction.h>
#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenuActions/SlotMenuActions/SlotContextMenuAction.h>

namespace ScriptCanvasEditor
{
    class NodePaletteModel;

    namespace Widget
    {
        class NodePaletteDockWidget;
    }

    //////////////////
    // CustomActions
    //////////////////

    class AddSelectedEntitiesAction
        : public GraphCanvas::ContextMenuAction
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(AddSelectedEntitiesAction, AZ::SystemAllocator, 0);

        AddSelectedEntitiesAction(QObject* parent);
        virtual ~AddSelectedEntitiesAction() = default;

        GraphCanvas::ActionGroupId GetActionGroupId() const override;

        void RefreshAction(const GraphCanvas::GraphId& graphCanvasGraphId, const AZ::EntityId& targetId) override;
        SceneReaction TriggerAction(const GraphCanvas::GraphId& graphCanvasGraphId, const AZ::Vector2& scenePos) override;
    };

    class EndpointSelectionAction
        : public QAction
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(EndpointSelectionAction, AZ::SystemAllocator, 0);

        EndpointSelectionAction(const GraphCanvas::Endpoint& endpoint);
        ~EndpointSelectionAction() = default;

        const GraphCanvas::Endpoint& GetEndpoint() const;

    private:
        GraphCanvas::Endpoint m_endpoint;
    };

    class RemoveUnusedVariablesMenuAction
        : public GraphCanvas::SceneContextMenuAction
    {
    public:
        AZ_CLASS_ALLOCATOR(RemoveUnusedVariablesMenuAction, AZ::SystemAllocator, 0);

        RemoveUnusedVariablesMenuAction(QObject* parent);
        virtual ~RemoveUnusedVariablesMenuAction() = default;

        bool IsInSubMenu() const override;
        AZStd::string GetSubMenuPath() const override;

        void RefreshAction(const GraphCanvas::GraphId& graphId, const AZ::EntityId& targetId) override;
        GraphCanvas::ContextMenuAction::SceneReaction TriggerAction(const GraphCanvas::GraphId& graphId, const AZ::Vector2& scenePos) override;
    };

    class ConvertVariableNodeToReferenceAction
        : public GraphCanvas::ContextMenuAction
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(ConvertVariableNodeToReferenceAction, AZ::SystemAllocator, 0);

        ConvertVariableNodeToReferenceAction(QObject* parent);
        virtual ~ConvertVariableNodeToReferenceAction() = default;

        GraphCanvas::ActionGroupId GetActionGroupId() const override;

        void RefreshAction(const GraphCanvas::GraphId& graphId, const AZ::EntityId& targetId) override;
        GraphCanvas::ContextMenuAction::SceneReaction TriggerAction(const GraphCanvas::GraphId& graphId, const AZ::Vector2& scenePos) override;

    private:

        AZ::EntityId m_targetId;
    };

    class SlotManipulationMenuAction
        : public GraphCanvas::ContextMenuAction
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(SlotManipulationMenuAction, AZ::SystemAllocator, 0);

        SlotManipulationMenuAction(AZStd::string_view actionName, QObject* parent);

    protected:
        ScriptCanvas::Slot* GetScriptCanvasSlot(const GraphCanvas::Endpoint& endpoint) const;
    };

    class ConvertReferenceToVariableNodeAction
        : public SlotManipulationMenuAction
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(ConvertReferenceToVariableNodeAction, AZ::SystemAllocator, 0);

        ConvertReferenceToVariableNodeAction(QObject* parent);
        virtual ~ConvertReferenceToVariableNodeAction() = default;

        GraphCanvas::ActionGroupId GetActionGroupId() const override;

        void RefreshAction(const GraphCanvas::GraphId& graphId, const AZ::EntityId& targetId) override;
        GraphCanvas::ContextMenuAction::SceneReaction TriggerAction(const GraphCanvas::GraphId& graphId, const AZ::Vector2& scenePos) override;

    private:

        AZ::EntityId m_targetId;
    };

    /////////////////
    // ContextMenus
    /////////////////

    class SceneContextMenu
        : public GraphCanvas::SceneContextMenu
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(SceneContextMenu, AZ::SystemAllocator, 0);

        SceneContextMenu(const NodePaletteModel& nodePaletteModel, AzToolsFramework::AssetBrowser::AssetBrowserFilterModel* assetModel);
        ~SceneContextMenu() = default;

        void ResetSourceSlotFilter();
        void FilterForSourceSlot(const AZ::EntityId& scriptCanvasGraphId, const AZ::EntityId& sourceSlotId);
        const Widget::NodePaletteDockWidget* GetNodePalette() const;

        // EditConstructContextMenu
        void OnRefreshActions(const GraphCanvas::GraphId& graphId, const AZ::EntityId& targetMemberId) override;
        ////

    public slots:

        void HandleContextMenuSelection();
        void SetupDisplay();

    protected:
        void keyPressEvent(QKeyEvent* keyEvent) override;
        
        AZ::EntityId                      m_sourceSlotId;
        Widget::NodePaletteDockWidget*    m_palette;
    };

    class ConnectionContextMenu
        : public GraphCanvas::ConnectionContextMenu
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(ConnectionContextMenu, AZ::SystemAllocator, 0);

        ConnectionContextMenu(const NodePaletteModel& nodePaletteModel, AzToolsFramework::AssetBrowser::AssetBrowserFilterModel* assetModel);
        ~ConnectionContextMenu() = default;

        const Widget::NodePaletteDockWidget* GetNodePalette() const;

    protected:

        void OnRefreshActions(const GraphCanvas::GraphId& graphId, const AZ::EntityId& targetMemberId);

    public slots:

        void HandleContextMenuSelection();
        void SetupDisplay();

    protected:
        void keyPressEvent(QKeyEvent* keyEvent) override;

    private:

        AZ::EntityId                      m_connectionId;
        Widget::NodePaletteDockWidget*    m_palette;
    };

    class SlotContextMenu
        : public GraphCanvas::SlotContextMenu
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(SlotContextMenu, AZ::SystemAllocator, 0);
    };
}
