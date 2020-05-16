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

#include "PhysX_precompiled.h"

#include <AzCore/UnitTest/TestTypes.h>
#include <AzToolsFramework/UnitTest/AzToolsFrameworkTestHelpers.h>
#include <EditorColliderComponent.h>
#include <EditorShapeColliderComponent.h>
#include <EditorRigidBodyComponent.h>
#include <LmbrCentral/Shape/BoxShapeComponentBus.h>
#include <PhysX/EditorColliderComponentRequestBus.h>
#include <Tests/EditorTestUtilities.h>
#include <Tests/PhysXTestCommon.h>

namespace PhysXEditorTests
{
    TEST_F(PhysXEditorFixture, EditorRigidBodyComponent_EntityLocalScaleChangedAndPhysicsUpdateHappened_RigidBodyScaleWasUpdated)
    {
        // Create editor entity
        EntityPtr editorEntity = CreateInactiveEditorEntity("Entity");
        editorEntity->CreateComponent<PhysX::EditorShapeColliderComponent>();        
        editorEntity->CreateComponent(LmbrCentral::EditorBoxShapeComponentTypeId);

        const auto* rigidBodyComponent = editorEntity->CreateComponent<PhysX::EditorRigidBodyComponent>();

        editorEntity->Activate();

        const AZ::Aabb originalAabb = rigidBodyComponent->GetRigidBody()->GetAabb();

        // Update the scale
        const AZ::Vector3 scale(2.0f);
        AZ::TransformBus::Event(editorEntity->GetId(), &AZ::TransformInterface::SetLocalScale, scale);

        // Trigger editor physics world update so EditorRigidBodyComponent can process scale change
        AZStd::shared_ptr<Physics::World> editorWorld;
        Physics::EditorWorldBus::BroadcastResult(editorWorld, &Physics::EditorWorldRequests::GetEditorWorld);
        editorWorld->Update(0.1f);

        const AZ::Aabb finalAabb = rigidBodyComponent->GetRigidBody()->GetAabb();

        EXPECT_THAT(finalAabb.GetMax(), UnitTest::IsClose(originalAabb.GetMax() * scale));
        EXPECT_THAT(finalAabb.GetMin(), UnitTest::IsClose(originalAabb.GetMin() * scale));
    }

    TEST_F(PhysXEditorFixture, EditorRigidBodyComponent_EntityScaledAndColliderHasNonZeroOffset_RigidBodyAabbMatchesScaledOffset)
    {
        // Create editor entity
        EntityPtr editorEntity = CreateInactiveEditorEntity("Entity");

        const auto* rigidBodyComponent = editorEntity->CreateComponent<PhysX::EditorRigidBodyComponent>();
        const auto* colliderComponent = editorEntity->CreateComponent<PhysX::EditorColliderComponent>();

        editorEntity->Activate();

        AZ::EntityComponentIdPair idPair(editorEntity->GetId(), colliderComponent->GetId());

        // Set collider to be a sphere
        const Physics::ShapeType shapeType = Physics::ShapeType::Sphere;
        PhysX::EditorColliderComponentRequestBus::Event(
            idPair, &PhysX::EditorColliderComponentRequests::SetShapeType, shapeType);

        // Set collider sphere radius
        const float sphereRadius = 1.0f;
        PhysX::EditorColliderComponentRequestBus::Event(
            idPair, &PhysX::EditorColliderComponentRequests::SetSphereRadius, sphereRadius);

        // Notify listeners that collider has changed
        PhysX::ColliderComponentEventBus::Event(editorEntity->GetId(), &PhysX::ColliderComponentEvents::OnColliderChanged);

        AZStd::shared_ptr<Physics::World> editorWorld;
        Physics::EditorWorldBus::BroadcastResult(editorWorld, &Physics::EditorWorldRequests::GetEditorWorld);

        // Update editor world to let updates to be applied
        editorWorld->Update(0.1f);

        const AZ::Aabb originalAabb = rigidBodyComponent->GetRigidBody()->GetAabb();
        
        const AZ::Vector3 offset(5.0f, 0.0f, 0.0f);
        PhysX::EditorColliderComponentRequestBus::Event(
            idPair, &PhysX::EditorColliderComponentRequests::SetColliderOffset, offset);

        // Update the scale
        const AZ::Vector3 scale(2.0f);
        AZ::TransformBus::Event(editorEntity->GetId(), &AZ::TransformInterface::SetLocalScale, scale);

        // Update editor world to let updates to be applied
        editorWorld->Update(0.1f);

        const AZ::Aabb finalAabb = rigidBodyComponent->GetRigidBody()->GetAabb();

        EXPECT_THAT(finalAabb.GetMax(), UnitTest::IsClose((originalAabb.GetMax() + offset) * scale));
        EXPECT_THAT(finalAabb.GetMin(), UnitTest::IsClose((originalAabb.GetMin() + offset) * scale));
    }
} // namespace PhysXEditorTests
