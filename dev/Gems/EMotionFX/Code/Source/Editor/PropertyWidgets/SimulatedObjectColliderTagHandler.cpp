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

#include <AzCore/Memory/SystemAllocator.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/PhysicsSetup.h>
#include <Editor/PropertyWidgets/SimulatedObjectColliderTagHandler.h>
#include <QHBoxLayout>
#include <QSignalBlocker>

namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(SimulatedObjectColliderTagSelector, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(SimulatedObjectColliderTagHandler, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(SimulatedJointColliderExclusionTagSelector, AZ::SystemAllocator, 0)
    AZ_CLASS_ALLOCATOR_IMPL(SimulatedJointColliderExclusionTagHandler, AZ::SystemAllocator, 0)

    SimulatedObjectColliderTagSelector::SimulatedObjectColliderTagSelector(QWidget* parent)
        : TagSelector(parent)
    {
        connect(this, &SimulatedObjectColliderTagSelector::TagsChanged, this, [this]()
            {
                EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
                AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::Bus::Handler::OnEditingFinished, this);
            });
    }

    void SimulatedObjectColliderTagSelector::GetAvailableTags(QVector<QString>& outTags) const
    {
        outTags.clear();

        if (!m_simulatedObject)
        {
            AZ_Warning("EMotionFX", false, "Cannot collect available tags from simulated object. Simulated object not valid.");
            return;
        }

        const SimulatedObjectSetup* simulatedObjectSetup = m_simulatedObject->GetSimulatedObjectSetup();
        AZ_Assert(simulatedObjectSetup, "Simulated object does not belong to a valid simulated object setup.");

        const Actor* actor = simulatedObjectSetup->GetActor();
        const Physics::CharacterColliderConfiguration& colliderConfig = actor->GetPhysicsSetup()->GetSimulatedObjectColliderConfig();
        QString tag;
        for (const Physics::CharacterColliderNodeConfiguration& nodeConfig : colliderConfig.m_nodes)
        {
            for (const auto& shapeConfigPair : nodeConfig.m_shapes)
            {
                const Physics::ColliderConfiguration* colliderConfig = shapeConfigPair.first.get();
                tag = colliderConfig->m_tag.c_str();

                if (!tag.isEmpty() &&
                    std::find(outTags.begin(), outTags.end(), tag) == outTags.end())
                {
                    outTags.push_back(tag);
                }
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////

    SimulatedObjectColliderTagHandler::SimulatedObjectColliderTagHandler()
        : QObject()
        , AzToolsFramework::PropertyHandler<AZStd::vector<AZStd::string>, SimulatedObjectColliderTagSelector>()
    {
    }

    AZ::u32 SimulatedObjectColliderTagHandler::GetHandlerName() const
    {
        return AZ_CRC("SimulatedObjectColliderTags", 0x80cfa635);
    }

    QWidget* SimulatedObjectColliderTagHandler::CreateGUI(QWidget* parent)
    {
        return aznew SimulatedObjectColliderTagSelector(parent);
    }

    void SimulatedObjectColliderTagHandler::ConsumeAttribute(SimulatedObjectColliderTagSelector* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == AZ::Edit::Attributes::ReadOnly)
        {
            bool value;
            if (attrValue->Read<bool>(value))
            {
                GUI->setEnabled(!value);
            }
        }
    }

    void SimulatedObjectColliderTagHandler::WriteGUIValuesIntoProperty(size_t index, SimulatedObjectColliderTagSelector* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        instance = GUI->GetTags();
    }

    bool SimulatedObjectColliderTagHandler::ReadValuesIntoGUI(size_t index, SimulatedObjectColliderTagSelector* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);

        if (node && node->GetParent())
        {
            AzToolsFramework::InstanceDataNode* parentNode = node->GetParent();
            m_simulatedObject = static_cast<SimulatedObject*>(parentNode->FirstInstance());
            GUI->SetSimulatedObject(m_simulatedObject);
        }
        else
        {
            GUI->SetSimulatedObject(nullptr);
        }

        GUI->SetTags(instance);
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////

    SimulatedJointColliderExclusionTagSelector::SimulatedJointColliderExclusionTagSelector(QWidget* parent)
        : TagSelector(parent)
    {
        connect(this, &SimulatedJointColliderExclusionTagSelector::TagsChanged, this, [this]()
            {
                EBUS_EVENT(AzToolsFramework::PropertyEditorGUIMessages::Bus, RequestWrite, this);
                AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&AzToolsFramework::PropertyEditorGUIMessages::Bus::Handler::OnEditingFinished, this);
            });
    }

    void SimulatedJointColliderExclusionTagSelector::GetAvailableTags(QVector<QString>& outTags) const
    {
        outTags.clear();

        if (!m_simulatedObject)
        {
            AZ_Warning("EMotionFX", false, "Cannot collect available tags from simulated object. Simulated object not valid.");
            return;
        }

        const SimulatedObjectSetup* simulatedObjectSetup = m_simulatedObject->GetSimulatedObjectSetup();
        AZ_Assert(simulatedObjectSetup, "Simulated object does not belong to a valid simulated object setup.");
        const AZStd::vector<AZStd::string>& colliderTags = m_simulatedObject->GetColliderTags();

        QString tag;
        for (const AZStd::string& colliderTag : colliderTags)
        {
            tag = colliderTag.c_str();

            if (!tag.isEmpty() &&
                std::find(outTags.begin(), outTags.end(), tag) == outTags.end())
            {
                outTags.push_back(tag);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////

    SimulatedJointColliderExclusionTagHandler::SimulatedJointColliderExclusionTagHandler()
        : QObject()
        , AzToolsFramework::PropertyHandler<AZStd::vector<AZStd::string>, SimulatedJointColliderExclusionTagSelector>()
    {
    }

    AZ::u32 SimulatedJointColliderExclusionTagHandler::GetHandlerName() const
    {
        return AZ_CRC("SimulatedJointColliderExclusionTags", 0x27b61cea);
    }

    QWidget* SimulatedJointColliderExclusionTagHandler::CreateGUI(QWidget* parent)
    {
        return aznew SimulatedJointColliderExclusionTagSelector(parent);
    }

    void SimulatedJointColliderExclusionTagHandler::ConsumeAttribute(SimulatedJointColliderExclusionTagSelector* GUI, AZ::u32 attrib, AzToolsFramework::PropertyAttributeReader* attrValue, const char* debugName)
    {
        if (attrib == AZ::Edit::Attributes::ReadOnly)
        {
            bool value;
            if (attrValue->Read<bool>(value))
            {
                GUI->setEnabled(!value);
            }
        }

        if (attrib == AZ_CRC("SimulatedObject", 0x2076ba91))
        {
            attrValue->Read<SimulatedObject*>(m_simulatedObject);
            GUI->SetSimulatedObject(m_simulatedObject);
        }
    }

    void SimulatedJointColliderExclusionTagHandler::WriteGUIValuesIntoProperty(size_t index, SimulatedJointColliderExclusionTagSelector* GUI, property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        instance = GUI->GetTags();
    }

    bool SimulatedJointColliderExclusionTagHandler::ReadValuesIntoGUI(size_t index, SimulatedJointColliderExclusionTagSelector* GUI, const property_t& instance, AzToolsFramework::InstanceDataNode* node)
    {
        QSignalBlocker signalBlocker(GUI);
        GUI->SetTags(instance);
        return true;
    }
} // namespace EMotionFX
