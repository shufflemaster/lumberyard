/**
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

#include <Tests/MockGraphCanvas.h>

namespace MockGraphCanvasServices
{
    // MockSlotComponent
    void MockSlotComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MockSlotComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    AZ::Entity* MockSlotComponent::CreateCoreSlotEntity()
    {
        AZ::Entity* entity = aznew AZ::Entity("Slot");
        return entity;
    }

    MockSlotComponent::MockSlotComponent(const GraphCanvas::SlotType& slotType)
        : m_slotType(slotType)
    {
    }

    MockSlotComponent::MockSlotComponent(const GraphCanvas::SlotType& slotType, const GraphCanvas::SlotConfiguration& configuration)
        : m_slotType(slotType)
        , m_slotConfiguration(configuration)
    {
    }

    void MockSlotComponent::Activate()
    {
    }

    void MockSlotComponent::Deactivate()
    {
    }

    // MockDataSlotComponent
    void MockDataSlotComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MockDataSlotComponent, MockSlotComponent>()
                ->Version(0)
                ;
        }
    }

    AZ::Entity* MockDataSlotComponent::CreateDataSlot(const GraphCanvas::DataSlotConfiguration& dataSlotConfiguration)
    {
        AZ::Entity* entity = MockSlotComponent::CreateCoreSlotEntity();

        MockDataSlotComponent* dataSlot = aznew MockDataSlotComponent(dataSlotConfiguration);

        if (!entity->AddComponent(dataSlot))
        {
            delete dataSlot;
            delete entity;
            return nullptr;
        }

        return entity;
    }

    MockDataSlotComponent::MockDataSlotComponent()
        : MockSlotComponent(GraphCanvas::SlotTypes::DataSlot)
    {
    }

    MockDataSlotComponent::MockDataSlotComponent(const GraphCanvas::DataSlotConfiguration& dataSlotConfiguration)
        : MockSlotComponent(GraphCanvas::SlotTypes::DataSlot, dataSlotConfiguration)
        , m_dataSlotConfiguration(dataSlotConfiguration)
    {
    }

    void MockDataSlotComponent::Activate()
    {
        GraphCanvas::DataSlotRequestBus::Handler::BusConnect(GetEntityId());
    }

    void MockDataSlotComponent::Deactivate()
    {
        GraphCanvas::DataSlotRequestBus::Handler::BusDisconnect();
    }

    bool MockDataSlotComponent::ConvertToReference()
    {
        return false;
    }

    bool MockDataSlotComponent::CanConvertToReference() const
    {
        return false;
    }

    bool MockDataSlotComponent::ConvertToValue()
    {
        return false;
    }

    bool MockDataSlotComponent::CanConvertToValue() const
    {
        return false;
    }

    GraphCanvas::DataSlotType MockDataSlotComponent::GetDataSlotType() const
    {
        return m_dataSlotConfiguration.m_dataSlotType;
    }

    GraphCanvas::DataValueType MockDataSlotComponent::GetDataValueType() const
    {
        return m_dataSlotConfiguration.m_dataValueType;
    }

    AZ::Uuid MockDataSlotComponent::GetDataTypeId() const
    {
        return m_dataSlotConfiguration.m_typeId;
    }

    void MockDataSlotComponent::SetDataTypeId(AZ::Uuid typeId)
    {
        m_dataSlotConfiguration.m_typeId = typeId;
    }

    const GraphCanvas::Styling::StyleHelper* MockDataSlotComponent::GetDataColorPalette() const
    {
        return nullptr;
    }

    size_t MockDataSlotComponent::GetContainedTypesCount() const
    {
        return m_dataSlotConfiguration.m_containerTypeIds.size();
    }

    AZ::Uuid MockDataSlotComponent::GetContainedTypeId(size_t index) const
    {
        return m_dataSlotConfiguration.m_containerTypeIds[index];
    }

    const GraphCanvas::Styling::StyleHelper* MockDataSlotComponent::GetContainedTypeColorPalette(size_t index) const
    {
        return nullptr;
    }

    void MockDataSlotComponent::SetDataAndContainedTypeIds(AZ::Uuid typeId, const AZStd::vector<AZ::Uuid>& typeIds, GraphCanvas::DataValueType valueType)
    {
        AZ_UNUSED(typeId);
        AZ_UNUSED(typeIds);
        AZ_UNUSED(valueType);
    }

    // MockExecutionSlotComponent
    void MockExecutionSlotComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MockExecutionSlotComponent, MockSlotComponent>()
                ->Version(0)
                ;
        }
    }

    AZ::Entity* MockExecutionSlotComponent::CreateExecutionSlot(const AZ::EntityId& nodeId, const GraphCanvas::SlotConfiguration& slotConfiguration)
    {
        AZ_UNUSED(nodeId);

        AZ::Entity* entity = MockSlotComponent::CreateCoreSlotEntity();

        MockExecutionSlotComponent* executionSlot = aznew MockExecutionSlotComponent(slotConfiguration);

        if (!entity->AddComponent(executionSlot))
        {
            delete executionSlot;
            delete entity;
            return nullptr;
        }

        return entity;
    }

    MockExecutionSlotComponent::MockExecutionSlotComponent()
        : MockSlotComponent(GraphCanvas::SlotTypes::ExecutionSlot)
    {
    }

    MockExecutionSlotComponent::MockExecutionSlotComponent(const GraphCanvas::SlotConfiguration& slotConfiguration)
        : MockSlotComponent(GraphCanvas::SlotTypes::ExecutionSlot, slotConfiguration)
        , m_executionSlotConfiguration(slotConfiguration)
    {
    }

    // MockExtenderSlotComponent
    void MockExtenderSlotComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MockExtenderSlotComponent, MockSlotComponent>()
                ->Version(0)
                ;
        }
    }

    AZ::Entity* MockExtenderSlotComponent::CreateExtenderSlot(const AZ::EntityId& nodeId, const GraphCanvas::ExtenderSlotConfiguration& slotConfiguration)
    {
        AZ_UNUSED(nodeId);

        AZ::Entity* entity = MockSlotComponent::CreateCoreSlotEntity();

        MockExtenderSlotComponent* extenderSlot = aznew MockExtenderSlotComponent(slotConfiguration);

        if (!entity->AddComponent(extenderSlot))
        {
            delete extenderSlot;
            delete entity;
            return nullptr;
        }

        return entity;
    }

    MockExtenderSlotComponent::MockExtenderSlotComponent()
        : MockSlotComponent(GraphCanvas::SlotTypes::ExtenderSlot)
    {
    }

    MockExtenderSlotComponent::MockExtenderSlotComponent(const GraphCanvas::ExtenderSlotConfiguration& slotConfiguration)
        : MockSlotComponent(GraphCanvas::SlotTypes::ExtenderSlot, slotConfiguration)
        , m_extenderSlotConfiguration(slotConfiguration)
    {
    }

    void MockExtenderSlotComponent::Activate()
    {
        GraphCanvas::ExtenderSlotRequestBus::Handler::BusConnect(GetEntityId());
    }

    void MockExtenderSlotComponent::Deactivate()
    {
        GraphCanvas::ExtenderSlotRequestBus::Handler::BusDisconnect();
    }

    void MockExtenderSlotComponent::TriggerExtension()
    {
    }

    GraphCanvas::Endpoint MockExtenderSlotComponent::ExtendForConnectionProposal(const GraphCanvas::ConnectionId& connectionId, const GraphCanvas::Endpoint& endpoint)
    {
        AZ_UNUSED(connectionId);
        AZ_UNUSED(endpoint);

        return GraphCanvas::Endpoint();
    }

    // MockNodeComponent
    void MockNodeComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MockNodeComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    AZ::Entity* MockNodeComponent::CreateCoreNodeEntity(const GraphCanvas::NodeConfiguration& config)
    {
        // Create this Node's entity.
        AZ::Entity* entity = aznew AZ::Entity();

        entity->CreateComponent<MockNodeComponent>(config);

        return entity;
    }

    MockNodeComponent::MockNodeComponent(const GraphCanvas::NodeConfiguration& config)
        : m_configuration(config)
    {
    }

    void MockNodeComponent::Activate()
    {
        GraphCanvas::NodeRequestBus::Handler::BusConnect(GetEntityId());
    }

    void MockNodeComponent::Deactivate()
    {
        GraphCanvas::NodeRequestBus::Handler::BusDisconnect();
    }

    void MockNodeComponent::SetTooltip(const AZStd::string& tooltip)
    {
        m_configuration.SetTooltip(tooltip);
    }

    void MockNodeComponent::SetTranslationKeyedTooltip(const GraphCanvas::TranslationKeyedString& tooltip)
    {
        m_configuration.SetTooltip(tooltip.GetDisplayString());
    }

    const AZStd::string MockNodeComponent::GetTooltip() const
    {
        return m_configuration.GetTooltip();
    }

    void MockNodeComponent::SetShowInOutliner(bool showInOutliner)
    {
        m_configuration.SetShowInOutliner(showInOutliner);
    }
    bool MockNodeComponent::ShowInOutliner() const
    {
        return m_configuration.GetShowInOutliner();
    }

    void MockNodeComponent::AddSlot(const AZ::EntityId& slotId)
    {
        AZ_Assert(slotId.IsValid(), "Slot entity (ID: %s) is not valid!", slotId.ToString().data());

        m_slotIds.emplace_back(slotId);
    }

    void MockNodeComponent::RemoveSlot(const AZ::EntityId& slotId)
    {
        AZ_Assert(slotId.IsValid(), "Slot (ID: %s) is not valid!", slotId.ToString().data());

        auto entry = AZStd::find(m_slotIds.begin(), m_slotIds.end(), slotId);
        AZ_Assert(entry != m_slotIds.end(), "Slot (ID: %s) is unknown", slotId.ToString().data());

        if (entry != m_slotIds.end())
        {
            m_slotIds.erase(entry);
        }
    }

    AZStd::vector<AZ::EntityId> MockNodeComponent::GetSlotIds() const
    {
        return m_slotIds;
    }

    AZStd::vector<GraphCanvas::SlotId> MockNodeComponent::GetVisibleSlotIds() const
    {
        return m_slotIds;
    }

    AZStd::vector<GraphCanvas::SlotId> MockNodeComponent::FindVisibleSlotIdsByType(const GraphCanvas::ConnectionType& connectionType, const GraphCanvas::SlotType& slotType) const
    {
        AZStd::vector<GraphCanvas::SlotId> empty;
        return empty;
    }

    bool MockNodeComponent::HasConnections() const
    {
        bool hasConnections = false;

        for (auto slotId : m_slotIds)
        {
            GraphCanvas::SlotRequestBus::EventResult(hasConnections, slotId, &GraphCanvas::SlotRequests::HasConnections);

            if (hasConnections)
            {
                break;
            }
        }

        return hasConnections;
    }

    AZStd::any* MockNodeComponent::GetUserData()
    {
        return &m_userData;
    }

    bool MockNodeComponent::IsWrapped() const
    {
        return false;
    }

    void MockNodeComponent::SetWrappingNode(const AZ::EntityId& wrappingNode)
    {
    }

    AZ::EntityId MockNodeComponent::GetWrappingNode() const
    {
        return AZ::EntityId();
    }

    void MockNodeComponent::SignalBatchedConnectionManipulationBegin()
    {
    }

    void MockNodeComponent::SignalBatchedConnectionManipulationEnd()
    {
    }

    GraphCanvas::RootGraphicsItemEnabledState MockNodeComponent::UpdateEnabledState()
    {
        return GraphCanvas::RootGraphicsItemEnabledState::ES_Enabled;
    }

    bool MockNodeComponent::IsHidingUnusedSlots() const
    {
        return false;
    }

    void MockNodeComponent::ShowAllSlots()
    {
    }

    void MockNodeComponent::HideUnusedSlots()
    {
    }

    bool MockNodeComponent::HasHideableSlots() const
    {
        return false;
    }

    void MockNodeComponent::SignalConnectionMoveBegin(const GraphCanvas::ConnectionId& connectionId)
    {
    }

    // MockGraphCanvasSystemComponent
    void MockGraphCanvasSystemComponent::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<MockGraphCanvasSystemComponent, AZ::Component>()
                ->Version(0)
                ;
        }
    }

    void MockGraphCanvasSystemComponent::Activate()
    {
        GraphCanvas::GraphCanvasRequestBus::Handler::BusConnect();
    }
    void MockGraphCanvasSystemComponent::Deactivate()
    {
        GraphCanvas::GraphCanvasRequestBus::Handler::BusDisconnect();
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateBookmarkAnchor() const
    {
        return nullptr;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateScene() const
    {
        AZ::Entity* entity = aznew AZ::Entity("GraphCanvasScene");
        return entity;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateCoreNode() const
    {
        return nullptr;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateGeneralNode(const char* nodeType) const
    {
        // Create this Node's entity.
        AZ::Entity* entity = MockNodeComponent::CreateCoreNodeEntity();

        return entity;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateCommentNode() const
    {
        return nullptr;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateWrapperNode(const char* nodeType) const
    {
        return nullptr;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateNodeGroup() const
    {
        return nullptr;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateCollapsedNodeGroup(const GraphCanvas::CollapsedNodeGroupConfiguration& groupedNodeConfiguration) const
    {
        return nullptr;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreateSlot(const AZ::EntityId& nodeId, const GraphCanvas::SlotConfiguration& slotConfiguration) const
    {
        if (const GraphCanvas::DataSlotConfiguration* dataSlotConfiguration = azrtti_cast<const GraphCanvas::DataSlotConfiguration*>(&slotConfiguration))
        {
            return MockDataSlotComponent::CreateDataSlot((*dataSlotConfiguration));
        }
        else if (const GraphCanvas::ExecutionSlotConfiguration* executionSlotConfiguration = azrtti_cast<const GraphCanvas::ExecutionSlotConfiguration*>(&slotConfiguration))
        {
            return MockExecutionSlotComponent::CreateExecutionSlot(nodeId, (*executionSlotConfiguration));
        }
        else if (const GraphCanvas::ExtenderSlotConfiguration* extenderSlotConfiguration = azrtti_cast<const GraphCanvas::ExtenderSlotConfiguration*>(&slotConfiguration))
        {
            return MockExtenderSlotComponent::CreateExtenderSlot(nodeId, (*extenderSlotConfiguration));
        }
        else
        {
            AZ_Error("GraphCanvas", false, "Trying to create using an unknown Slot Configuration");
        }

        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateBooleanNodePropertyDisplay(GraphCanvas::BooleanDataInterface* dataInterface) const
    {
        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateNumericNodePropertyDisplay(GraphCanvas::NumericDataInterface* dataInterface) const
    {
        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateComboBoxNodePropertyDisplay(GraphCanvas::ComboBoxDataInterface* dataInterface) const
    {
        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateEntityIdNodePropertyDisplay(GraphCanvas::EntityIdDataInterface* dataInterface) const
    {
        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateReadOnlyNodePropertyDisplay(GraphCanvas::ReadOnlyDataInterface* dataInterface) const
    {
        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateStringNodePropertyDisplay(GraphCanvas::StringDataInterface* dataInterface) const
    {
        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateVectorNodePropertyDisplay(GraphCanvas::VectorDataInterface* dataInterface) const
    {
        return nullptr;
    }

    GraphCanvas::NodePropertyDisplay* MockGraphCanvasSystemComponent::CreateAssetIdNodePropertyDisplay(GraphCanvas::AssetIdDataInterface* dataInterface) const
    {
        return nullptr;
    }

    AZ::Entity* MockGraphCanvasSystemComponent::CreatePropertySlot(const AZ::EntityId& nodeId, const AZ::Crc32& propertyId, const GraphCanvas::SlotConfiguration& slotConfiguration) const
    {
        return nullptr;
    }
}
