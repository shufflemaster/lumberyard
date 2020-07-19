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

#include <AzCore/Serialization/EditContext.h>
#include <AzCore/Serialization/SerializeContext.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <LyViewPaneNames.h>

#include <QObject>

#include <LandscapeCanvas/LandscapeCanvasBus.h>
#include "EditorLandscapeCanvasComponent.h"

namespace LandscapeCanvas
{
    void EditorLandscapeCanvasComponent::Init()
    {
    }

    void EditorLandscapeCanvasComponent::Activate()
    {
    }

    void EditorLandscapeCanvasComponent::Deactivate()
    {
    }

    void EditorLandscapeCanvasComponent::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<EditorLandscapeCanvasComponent, AzToolsFramework::Components::EditorComponentBase>()
                ->Version(2)
                ->Field("Graph", &EditorLandscapeCanvasComponent::m_graph)
                ;

            AZ::EditContext* editContext = serializeContext->GetEditContext();
            if (editContext)
            {
                editContext->Class<EditorLandscapeCanvasComponent>("Landscape Canvas", "The Landscape Canvas component provides a node-based Editor for authoring Dynamic Vegetation")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
                        ->Attribute(AZ::Edit::Attributes::Icon, "Editor/Icons/Components/LandscapeCanvas.svg")
                        ->Attribute(AZ::Edit::Attributes::ViewportIcon, "Editor/Icons/Components/Viewport/LandscapeCanvas.png")
                        ->Attribute(AZ::Edit::Attributes::Category, "Vegetation")
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::AppearsInAddComponentMenu, AZ_CRC("Game", 0x232b318c))
                    ->UIElement(AZ::Edit::UIHandlers::Button, "", "Opens the Landscape Canvas for the current entity")
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &EditorLandscapeCanvasComponent::OnOpenGraphButtonClicked)
                        ->Attribute(AZ::Edit::Attributes::ButtonText, &EditorLandscapeCanvasComponent::GetOpenGraphButtonText);
            }
        }
    }

    AZ::Crc32 EditorLandscapeCanvasComponent::OnOpenGraphButtonClicked()
    {
        // Make sure the Landscape Canvas tool is open
        AzToolsFramework::OpenViewPane(LyViewPane::LandscapeCanvas);

        // Tell the Landscape Canvas tool to graph the entity our Landscape Canvas component is attached to
        LandscapeCanvas::LandscapeCanvasRequestBus::Broadcast(&LandscapeCanvas::LandscapeCanvasRequests::OnGraphEntity, GetEntityId());

        return AZ::Edit::PropertyRefreshLevels::EntireTree;
    }

    AZStd::string EditorLandscapeCanvasComponent::GetOpenGraphButtonText() const
    {
        return QObject::tr("Edit").toUtf8().constData();
    }

    void EditorLandscapeCanvasComponent::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        LandscapeCanvasComponent::GetRequiredServices(required);
    }

    void EditorLandscapeCanvasComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        LandscapeCanvasComponent::GetProvidedServices(provided);
    }

    void EditorLandscapeCanvasComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        LandscapeCanvasComponent::GetIncompatibleServices(incompatible);
    }

    void EditorLandscapeCanvasComponent::BuildGameEntity(AZ::Entity* gameEntity)
    {
        gameEntity->CreateComponent<LandscapeCanvasComponent>();
    }
}
