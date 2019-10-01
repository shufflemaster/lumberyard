﻿/*
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

#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Math/Quaternion.h>

#include <AzFramework/Entity/EntityDebugDisplayBus.h>
#include <AzFramework/Physics/RigidBody.h>
#include <AzFramework/Physics/Shape.h>
#include <AzFramework/Physics/ShapeConfiguration.h>

#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <AzToolsFramework/ComponentMode/ComponentModeDelegate.h>
#include <AzToolsFramework/Manipulators/BoxManipulatorRequestBus.h>
#include <AzToolsFramework/ToolsComponents/EditorComponentBase.h>

#include <PhysX/ColliderShapeBus.h>
#include <PhysX/ConfigurationBus.h>
#include <PhysX/MeshAsset.h>
#include <PhysX/MeshColliderComponentBus.h>

namespace PhysX
{
    /// Proxy container for only displaying a specific shape configuration depending on the shapeType selected.
    struct EditorProxyShapeConfig
    {
        AZ_CLASS_ALLOCATOR(EditorProxyShapeConfig, AZ::SystemAllocator, 0);
        AZ_RTTI(EditorProxyShapeConfig, "{531FB42A-42A9-4234-89BA-FD349EF83D0C}");
        static void Reflect(AZ::ReflectContext* context);

        EditorProxyShapeConfig() = default;
        EditorProxyShapeConfig(const Physics::ShapeConfiguration& shapeConfiguration);
        virtual ~EditorProxyShapeConfig() = default;

        Physics::ShapeType m_shapeType = Physics::ShapeType::PhysicsAsset;
        Physics::SphereShapeConfiguration m_sphere;
        Physics::BoxShapeConfiguration m_box;
        Physics::CapsuleShapeConfiguration m_capsule;
        Physics::PhysicsAssetShapeConfiguration m_physicsAsset;

        bool IsSphereConfig() const;
        bool IsBoxConfig() const;
        bool IsCapsuleConfig() const;
        bool IsAssetConfig() const;
        Physics::ShapeConfiguration& GetCurrent();

        AZ::u32 OnConfigurationChanged();
    };

    /// Editor PhysX Collider Component.
    ///
    class EditorColliderComponent
        : public AzToolsFramework::Components::EditorComponentBase
        , protected AzFramework::EntityDebugDisplayEventBus::Handler
        , protected AzToolsFramework::EntitySelectionEvents::Bus::Handler
        , private AzToolsFramework::BoxManipulatorRequestBus::Handler
        , private AZ::Data::AssetBus::MultiHandler
        , private PhysX::MeshColliderComponentRequestsBus::Handler
        , private AZ::TransformNotificationBus::Handler
        , public AZ::TickBus::Handler
        , private PhysX::ConfigurationNotificationBus::Handler
        , private PhysX::ColliderShapeRequestBus::Handler
    {
    public:
        AZ_EDITOR_COMPONENT(EditorColliderComponent, "{FD429282-A075-4966-857F-D0BBF186CFE6}", AzToolsFramework::Components::EditorComponentBase);
        static void Reflect(AZ::ReflectContext* context);
        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
        {
            provided.push_back(AZ_CRC("PhysXColliderService", 0x4ff43f7c));
            provided.push_back(AZ_CRC("PhysXTriggerService", 0x3a117d7b));
        }

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
        {
            required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
        }

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
        {
            // Not compatible with Legacy Cry Physics services
            incompatible.push_back(AZ_CRC("ColliderService", 0x902d4e93));
            incompatible.push_back(AZ_CRC("LegacyCryPhysicsService", 0xbb370351));
        }

        EditorColliderComponent() = default;
        EditorColliderComponent(
            const Physics::ColliderConfiguration& colliderConfiguration,
            const Physics::ShapeConfiguration& shapeConfiguration);

        // these functions are made virtual because we call them from other modules
        virtual const EditorProxyShapeConfig& GetShapeConfiguration() const;
        virtual const Physics::ColliderConfiguration& GetColliderConfiguration() const;

        void BuildGameEntity(AZ::Entity* gameEntity) override;

    private:
        AZ_DISABLE_COPY_MOVE(EditorColliderComponent)

        // AZ::Component
        void Activate() override;
        void Deactivate() override;

        /// AzToolsFramework::EntitySelectionEvents
        void OnSelected() override;
        void OnDeselected() override;

        // AzFramework::EntityDebugDisplayEventBus
        void DisplayEntityViewport(
            const AzFramework::ViewportInfo& viewportInfo,
            AzFramework::DebugDisplayRequests& debugDisplay) override;

        void Display(AzFramework::DebugDisplayRequests& debugDisplay);

        // AZ::Data::AssetBus::Handler
        void OnAssetReady(AZ::Data::Asset<AZ::Data::AssetData> asset) override;
        void OnAssetReloaded(AZ::Data::Asset<AZ::Data::AssetData> asset) override;

        // PhysXMeshColliderComponentRequestBus
        AZ::Data::Asset<Pipeline::MeshAsset> GetMeshAsset() const override;
        void GetStaticWorldSpaceMeshTriangles(AZStd::vector<AZ::Vector3>& verts, AZStd::vector<AZ::u32>& indices) const override;
        Physics::MaterialId GetMaterialId() const override;
        void SetMeshAsset(const AZ::Data::AssetId& id) override;
        void SetMaterialAsset(const AZ::Data::AssetId& id) override;
        void SetMaterialId(const Physics::MaterialId& id) override;
        void UpdateMaterialSlotsFromMeshAsset();

        // TickBus::Handler
        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        // TransformBus
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

        // AzToolsFramework::BoxManipulatorRequestBus
        AZ::Vector3 GetDimensions() override;
        void SetDimensions(const AZ::Vector3& dimensions) override;
        AZ::Transform GetCurrentTransform() override;
        AZ::Vector3 GetBoxScale() override;

        // PhysX::ConfigurationNotificationBus
        virtual void OnConfigurationRefreshed(const Configuration& configuration) override;

        // PhysX::ColliderShapeBus
        AZ::Aabb GetColliderShapeAabb() override;
        bool IsTrigger() override;

        AZ::Transform GetColliderTransform() const;
        float GetUniformScale() const;
        AZ::Vector3 GetNonUniformScale() const;

        EditorProxyShapeConfig m_shapeConfiguration;
        Physics::ColliderConfiguration m_configuration;

        AZ::u32 OnConfigurationChanged();
        void UpdateShapeConfigurationScale();

        void DrawSphere(AzFramework::DebugDisplayRequests& debugDisplay, const Physics::SphereShapeConfiguration& config);
        void DrawBox(AzFramework::DebugDisplayRequests& debugDisplay, const Physics::BoxShapeConfiguration& config);
        void DrawCapsule(AzFramework::DebugDisplayRequests& debugDisplay, const Physics::CapsuleShapeConfiguration& config);
        void DrawMesh(AzFramework::DebugDisplayRequests& debugDisplay);

        // Mesh collider
        void DrawTriangleMesh(AzFramework::DebugDisplayRequests& debugDisplay, physx::PxBase* meshData) const;
        void DrawConvexMesh(AzFramework::DebugDisplayRequests& debugDisplay, physx::PxBase* meshData) const;
        void UpdateColliderMeshColor(AZ::Color& baseColor, AZ::u32 triangleCount) const;
        bool IsAssetConfig() const;
        void UpdateMeshAsset();
        void CreateStaticEditorCollider();

        AZ::Crc32 ShouldShowBoxComponentModeButton() const;

        using ComponentModeDelegate = AzToolsFramework::ComponentModeFramework::ComponentModeDelegate;
        ComponentModeDelegate m_componentModeDelegate; ///< Responsible for detecting ComponentMode activation
                                                       ///< and creating a concrete ComponentMode.

        AZ::Data::Asset<Pipeline::MeshAsset> m_meshColliderAsset;
        mutable AZStd::vector<AZ::Vector3> m_verts;
        mutable AZStd::vector<AZ::Vector3> m_points;
        mutable AZStd::vector<AZ::u32> m_indices;

        mutable AZ::Color m_triangleCollisionMeshColor;
        mutable AZ::Color m_convexCollisionMeshColor;
        mutable bool m_meshDirty = true;

        void BuildMeshes() const;
        void BuildAABBVerts(const AZ::Vector3& boxMin, const AZ::Vector3& boxMax) const;
        void BuildTriangleMesh(physx::PxBase* meshData) const;
        void BuildConvexMesh(physx::PxBase* meshData) const;
        AZ::Color m_wireFrameColor;
        AZ::Color m_warningColor;

        float m_colliderVisualizationLineWidth = 2.0f;

        double m_time = 0.0f;

        /// Determine if the debug draw preference should be visible to the user
        /// @param requiredState the collider debug state required to check for
        /// @return true iff global collider debug is enabled.
        static bool IsGlobalColliderDebugCheck(PhysX::EditorConfiguration::GlobalCollisionDebugState requiredState);

        /// Open the PhysX Settings Window on the Global Settings tab
        static void OpenPhysXSettingsWindow();

        bool m_debugDrawButtonState = false;
        bool m_debugDraw = true; ///< Display the collider in editor view.

        // Capsule collider
        AZ::Vector3 GetCapsuleScale();

        AZStd::unique_ptr<Physics::RigidBodyStatic> m_editorBody;
    };
} // namespace PhysX