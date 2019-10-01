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

#include <Components/Slots/SlotComponent.h>

#include <GraphCanvas/Components/Connections/ConnectionBus.h>
#include <GraphCanvas/Components/Slots/Extender/ExtenderSlotBus.h>
#include <GraphCanvas/Components/Nodes/NodeBus.h>

namespace GraphCanvas
{
    class ExtenderSlotComponent
        : public SlotComponent
        , public ExtenderSlotRequestBus::Handler
        , public ConnectionNotificationBus::Handler        
    {
    private:
        enum SaveVersion
        {
            InitialVersion = 0,
            
            // Should always be last
            Current
        };
        
    public:
        AZ_COMPONENT(ExtenderSlotComponent, "{A86D8623-9D63-4D19-A4B3-344054FB8435}", SlotComponent);
        static void Reflect(AZ::ReflectContext* reflectContext);
        
        static AZ::Entity* CreateExtenderSlot(const AZ::EntityId& nodeId, const ExtenderSlotConfiguration& dataSlotConfiguration);
        
        ExtenderSlotComponent();
        ExtenderSlotComponent(const ExtenderSlotConfiguration& dataSlotConfiguration);
        ~ExtenderSlotComponent();
        
        // Component
        void Init();
        void Activate();
        void Deactivate();
        ////

        // SceneMemberNotifications
        void OnSceneMemberAboutToSerialize(GraphSerialization& sceneSerialization) override;
        ////

        // SlotRequestBus
        void AddConnectionId(const AZ::EntityId& connectionId, const Endpoint& endpoint) override;
        void RemoveConnectionId(const AZ::EntityId& connectionId, const Endpoint& endpoint) override;

        void SetNode(const AZ::EntityId& nodeId) override;

        SlotConfiguration* CloneSlotConfiguration() const override;

        int GetLayoutPriority() const override;
        ////

        // ConnectionNotificationBus        
        void OnMoveFinalized(bool isValidConnection) override;

        void OnSourceSlotIdChanged(const SlotId& oldSlotId, const SlotId& newSlotId) override;
        void OnTargetSlotIdChanged(const SlotId& oldSlotId, const SlotId& newSlotId) override;
        ////

        // ExtenderSlotComponent
        void TriggerExtension() override;
        Endpoint ExtendForConnectionProposal(const ConnectionId& connectionId, const Endpoint& endpoint) override;
        ////

    protected:
        // SlotComponent
        void OnFinalizeDisplay() override;
        ////

    private:
        ExtenderSlotComponent(const ExtenderSlotComponent&) = delete;
        ExtenderSlotComponent& operator=(const ExtenderSlotComponent&) = delete;

        void ConstructSlot();
        void EraseSlot();
        void CleanupProposedSlot();

        bool m_proposedSlot;
        
        AZ::Entity* ConstructConnectionEntity(const Endpoint& sourceEndpoint, const Endpoint& targetEndpoint, bool createModelConnection) override;
        
        AZ::EntityId m_trackedConnectionId;
        AZ::EntityId m_createdSlot;

        ExtenderId m_extenderId;
    };
}