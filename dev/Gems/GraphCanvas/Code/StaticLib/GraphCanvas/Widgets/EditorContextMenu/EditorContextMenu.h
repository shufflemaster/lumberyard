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

#include <AzCore/PlatformDef.h>

AZ_PUSH_DISABLE_WARNING(4251, "-Wunknown-warning-option")
#include <QMenu>
AZ_POP_DISABLE_WARNING

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>
#include <AzCore/std/containers/unordered_set.h>

#include <GraphCanvas/Widgets/EditorContextMenu/ContextMenuActions/ContextMenuAction.h>
#include <GraphCanvas/Widgets/NodePalette/NodePaletteWidget.h>

namespace GraphCanvas
{
    class EditorContextMenu
        : public QMenu
    {
        Q_OBJECT
    public:    
        AZ_CLASS_ALLOCATOR(EditorContextMenu, AZ::SystemAllocator, 0);
        
        EditorContextMenu(EditorId editorId, QWidget* parent = nullptr);
        virtual ~EditorContextMenu() = default;

        enum class MenuActionSection
        {
            Front,
            Default,
            Back
        };

        void SetIsToolBarMenu(bool isToolBarMenu);
        bool IsToolBarMenu() const;

        EditorId GetEditorId() const;

        void AddActionGroup(const ActionGroupId& actionGroup);
        void AddMenuAction(QAction* contextMenuAction, MenuActionSection section = MenuActionSection::Default);
        void AddMenuActionFront(QAction* contextMenuAction);
        void AddMenuActionBack(QAction* contextMenuAction);
        void AddNodePaletteMenuAction(const NodePaletteConfig& config);       

        bool   IsFinalized() const;
        QMenu* FindSubMenu(AZStd::string_view subMenuPath);

        void RefreshActions(const GraphId& graphId, const AZ::EntityId& targetMemberId);
        
        void showEvent(QShowEvent* showEvent) override;

        const NodePaletteWidget* GetNodePalette() const;
        void ResetSourceSlotFilter();
        void FilterForSourceSlot(const GraphId& graphId, const AZ::EntityId& sourceSlotId);

    protected slots:
        virtual void SetupDisplay();
        virtual void HandleContextMenuSelection();

    protected:
        virtual void OnRefreshActions(const GraphId& graphId, const AZ::EntityId& targetMemberId);

        void keyPressEvent(QKeyEvent* keyEvent) override;

        NodePaletteWidget* m_nodePalette = nullptr;

    private:
    
        void ConstructMenu();
        void AddUnprocessedActions(AZStd::vector<QAction*>& actions);
    
        bool m_finalized;
        bool m_isToolBarMenu;

        EditorId                                m_editorId;

        AZStd::vector< ActionGroupId >          m_actionGroupOrdering;
        AZStd::unordered_set< ActionGroupId >   m_actionGroups;

        AZStd::vector<QAction*> m_unprocessedFrontActions;
        AZStd::vector<QAction*> m_unprocessedActions;
        AZStd::vector<QAction*> m_unprocessedBackActions;

        AZStd::unordered_map< AZStd::string, QMenu* > m_subMenuMap;
    };
}
