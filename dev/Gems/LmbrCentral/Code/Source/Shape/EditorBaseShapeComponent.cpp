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

#include "LmbrCentral_precompiled.h"
#include "EditorBaseShapeComponent.h"

#include <AzCore/Serialization/EditContext.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>

namespace LmbrCentral
{
    void EditorBaseShapeComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC("ShapeService", 0xe86aa5fe));
    }

    void EditorBaseShapeComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC("ShapeService", 0xe86aa5fe));
    }

    void EditorBaseShapeComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
    }

    void EditorBaseShapeComponent::Reflect(AZ::SerializeContext& context)
    {
        context.Class<EditorBaseShapeComponent, EditorComponentBase>()
            ->Version(2)
            ->Field("Visible", &EditorBaseShapeComponent::m_visibleInEditor)
            ->Field("GameView", &EditorBaseShapeComponent::m_visibleInGameView)
            ->Field("DisplayFilled", &EditorBaseShapeComponent::m_displayFilled)
            ->Field("ShapeColor", &EditorBaseShapeComponent::m_shapeColor)
            ;

        if (auto editContext = context.GetEditContext())
        {
            editContext->Class<EditorBaseShapeComponent>("EditorBaseShapeComponent", "Editor base shape component")
                ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
                ->DataElement(AZ::Edit::UIHandlers::CheckBox, &EditorBaseShapeComponent::m_visibleInEditor, "Visible", "Always display this shape in the editor viewport")
                ->DataElement(AZ::Edit::UIHandlers::CheckBox, &EditorBaseShapeComponent::m_visibleInGameView, "Game View", "Display the shape while in Game View")
                // ->DataElement(AZ::Edit::UIHandlers::CheckBox, &EditorBaseShapeComponent::m_displayFilled, "Filled", "Display the shape as either filled or wireframe") // hidden before selection is resolved
                //   ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorBaseShapeComponent::OnDisplayFilledChanged)
                ->DataElement(AZ::Edit::UIHandlers::Default, &EditorBaseShapeComponent::m_shapeColor, "Shape Color", "The color to use when rendering the faces of the shape object")
                    ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorBaseShapeComponent::OnShapeColorChanged)
                ;
        }
    }

    void EditorBaseShapeComponent::Activate()
    {
        EditorComponentBase::Activate();
        AZ::TransformNotificationBus::Handler::BusConnect(GetEntityId());
        EditorShapeComponentRequestsBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusConnect(GetEntityId());
        AzToolsFramework::EditorComponentSelectionNotificationsBus::Handler::BusConnect(GetEntityId());
    }

    void EditorBaseShapeComponent::Deactivate()
    {
        AzToolsFramework::EditorComponentSelectionNotificationsBus::Handler::BusDisconnect();
        AzToolsFramework::EditorComponentSelectionRequestsBus::Handler::BusDisconnect();
        EditorShapeComponentRequestsBus::Handler::BusDisconnect();
        AZ::TransformNotificationBus::Handler::BusDisconnect();
        EditorComponentBase::Deactivate();
    }

    void EditorBaseShapeComponent::SetVisibleInEditor(bool visible)
    {
        m_visibleInEditor = visible;
    }

    void EditorBaseShapeComponent::SetShapeColor(const AZ::Color& shapeColor)
    {
        m_shapeColor = shapeColor;
    }

    void EditorBaseShapeComponent::SetShapeWireframeColor(const AZ::Color& wireColor)
    {
        m_shapeWireColor = wireColor;
    }

    bool EditorBaseShapeComponent::CanDraw() const
    {
        return IsSelected() || m_visibleInEditor;
    }

    void EditorBaseShapeComponent::SetShapeComponentConfig(ShapeComponentConfig* shapeConfig)
    {
        m_shapeConfig = shapeConfig;
    }

    AZ::Aabb EditorBaseShapeComponent::GetEditorSelectionBoundsViewport(
        const AzFramework::ViewportInfo& /*viewportInfo*/)
    {
        AZ::Aabb aabb = AZ::Aabb::CreateNull();
        ShapeComponentRequestsBus::EventResult(aabb, GetEntityId(), &ShapeComponentRequests::GetEncompassingAabb);
        return aabb;
    }

    bool EditorBaseShapeComponent::EditorSelectionIntersectRayViewport(
        const AzFramework::ViewportInfo& /*viewportInfo*/,
        const AZ::Vector3& src, const AZ::Vector3& dir, AZ::VectorFloat & distance)
    {
        // if we are not drawing this or it is wireframe, do not allow selection
        if (!CanDraw() || !m_displayFilled)
        {
            return false;
        }

        // Don't intersect with shapes when the camera is inside them
        bool isInside = false;
        ShapeComponentRequestsBus::EventResult(isInside, GetEntityId(), &ShapeComponentRequests::IsPointInside, src);
        if (isInside)
        {
            return false;
        }
        
        bool rayHit = false;
        ShapeComponentRequestsBus::EventResult(rayHit, GetEntityId(), &ShapeComponentRequests::IntersectRay, src, dir, distance);
        return rayHit;
    }

    void EditorBaseShapeComponent::OnAccentTypeChanged(AzToolsFramework::EntityAccentType accent)
    {
        if (accent == AzToolsFramework::EntityAccentType::Hover || IsSelected())
        {
            SetShapeWireframeColor(AzFramework::ViewportColors::HoverColor);
        }
        else
        {
            SetShapeWireframeColor(AzFramework::ViewportColors::WireColor);
        }
    }

    void EditorBaseShapeComponent::OnShapeColorChanged()
    {
        m_shapeColor.SetA(AzFramework::ViewportColors::DeselectedColor.GetA());

        if (m_shapeConfig)
        {
            m_shapeConfig->SetDrawColor(m_shapeColor);
        }
    }

    void EditorBaseShapeComponent::OnDisplayFilledChanged()
    {
        if (m_shapeConfig)
        {
            m_shapeConfig->SetIsFilled(m_displayFilled);
        }
    }

} // namespace LmbrCentral