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

#include <EMotionFX/Source/ActorInstance.h>
#include <EMotionFX/Source/Actor.h>
#include <EMotionFX/Source/SkeletalMotion.h>
#include <EMotionFX/Source/SkeletalSubMotion.h>
#include <EMotionFX/Source/TransformData.h>
#include <EMotionFX/Source/Pose.h>
#include <EMotionFX/Source/MotionLayerSystem.h>
#include <Tests/ActorFixture.h>
#include <Tests/Matchers.h>
#include <string>
#include <vector>

namespace EMotionFX
{
    class MotionSamplingFixture
        : public ActorFixture
    {
    public:
        void CreateSubMotionLikeBindPose(const std::string& name)
        {
            const Skeleton* skeleton = m_actor->GetSkeleton();
            AZ::u32 jointIndex = InvalidIndex32;
            const Node* node = skeleton->FindNodeAndIndexByName(name.c_str(), jointIndex);
            ASSERT_NE(node, nullptr);
            ASSERT_NE(jointIndex, InvalidIndex32);

            const Pose* bindPose = m_actorInstance->GetTransformData()->GetBindPose();
            const Transform& transform = bindPose->GetLocalSpaceTransform(jointIndex);
            CreateSubMotion(name, transform);
        }

        void CreateSubMotion(const std::string& name, const Transform& transform)
        {
            // Find and store the joint index.
            const Skeleton* skeleton = m_actor->GetSkeleton();
            AZ::u32 jointIndex = InvalidIndex32;
            const Node* node = skeleton->FindNodeAndIndexByName(name.c_str(), jointIndex);
            ASSERT_NE(node, nullptr);
            ASSERT_NE(jointIndex, InvalidIndex32);
            m_jointIndices.emplace_back(jointIndex);

            SkeletalSubMotion* subMotion = SkeletalSubMotion::Create(name.c_str());
            subMotion->SetBindPosePos(transform.mPosition);
            subMotion->SetBindPoseRot(transform.mRotation);
            subMotion->SetPosePos(transform.mPosition);
            subMotion->SetPoseRot(transform.mRotation);
            EMFX_SCALECODE
            (
                subMotion->SetBindPoseScale(transform.mScale);
                subMotion->SetPoseScale(transform.mScale);
            )
            m_motion->AddSubMotion(subMotion);
        }

        void SetUp() override
        {
            ActorFixture::SetUp();

            // Get the joint that isn't in the motion data.
            Node* footNode = m_actor->GetSkeleton()->FindNodeAndIndexByName("l_ball", m_footIndex);
            ASSERT_NE(footNode, nullptr);
            ASSERT_NE(m_footIndex, InvalidIndex32);

            m_motion = SkeletalMotion::Create("TestMotion");

            // Create submotions for joints that exist in the actor.
            for (const std::string& jointName : m_jointNames)
            {
                CreateSubMotionLikeBindPose(jointName);
            }

            // Update the motion duration and mark it as additive.
            m_motion->UpdateMaxTime();
            m_motion->SetIsAdditive(true);

            // Play this motion, creating a motion instance, so we can easily sample it.
            MotionSystem* motionSystem = m_actorInstance->GetMotionSystem();
            ASSERT_NE(motionSystem, nullptr);
            m_motionInstance = motionSystem->PlayMotion(m_motion);
        }

        void TearDown() override
        {
            if (m_motion)
            {
                m_motion->Destroy();
            }

            ActorFixture::TearDown();
        }

    protected:
        SkeletalMotion* m_motion = nullptr;
        MotionInstance* m_motionInstance = nullptr; // Automatically deleted internally when deleting the actor instance.
        std::vector<AZ::u32> m_jointIndices;
        std::vector<std::string> m_jointNames { "l_upLeg", "l_loLeg", "l_ankle" };
        AZ::u32 m_footIndex = InvalidIndex32;
    };

    TEST_F(MotionSamplingFixture, SampleAdditiveJoint)
    {
        const Skeleton* skeleton = m_actor->GetSkeleton();

        // Sample the joints that exist in our actor skeleton as well as inside the motion data.
        const Pose* bindPose = m_actorInstance->GetTransformData()->GetBindPose();
        for (AZ::u32 jointIndex : m_jointIndices)
        {
            // Sample the motion.
            Transform transform;
            transform.Zero(); // Set all to Zero, not identity as this methods might return identity and we want to verify that.
            m_motion->CalcNodeTransform(m_motionInstance, &transform, m_actor.get(), skeleton->GetNode(jointIndex), /*timeValue=*/0.0f, /*enableRetargeting=*/false);

            const Transform& bindTransform = bindPose->GetLocalSpaceTransform(jointIndex);
            EXPECT_THAT(transform, IsClose(bindTransform));
        }

        // Sample the motion for the foot node.
        Transform footTransform;
        footTransform.Zero(); // Set all to Zero, not identity as this methods might return identity and we want to verify that.
        m_motion->CalcNodeTransform(m_motionInstance, &footTransform, m_actor.get(), skeleton->GetNode(m_footIndex), /*timeValue=*/0.0f, /*enableRetargeting=*/false);

        // Make sure we get an identity transform back as we try to sample a node that doesn't have a submotion in an additive motion.
        EXPECT_THAT(footTransform, IsClose(Transform::CreateIdentity()));

        // Make it a non-additive motion, or at least act like it.
        m_motion->SetIsAdditive(false);

        // Make sure we do not get an identity transform back now that it is a non-additive motion.
        footTransform.Zero(); // Set all to Zero, not identity as this methods might return identity and we want to verify that.
        const Transform& expectedFootTransform = m_actorInstance->GetTransformData()->GetCurrentPose()->GetLocalSpaceTransform(m_footIndex);
        m_motion->CalcNodeTransform(m_motionInstance, &footTransform, m_actor.get(), skeleton->GetNode(m_footIndex), /*timeValue=*/0.0f, /*enableRetargeting=*/false);
        EXPECT_THAT(footTransform, IsClose(expectedFootTransform));
    }

    TEST_F(MotionSamplingFixture, SampleAdditivePose)
    {
        // Sample a pose from the motion.
        Pose pose;
        pose.LinkToActorInstance(m_actorInstance);
        pose.InitFromBindPose(m_actor.get());
        pose.Zero();
        m_motion->Update(&pose, &pose, m_motionInstance);
     
        // Test if the joints that exist in both motion and actor have the expected transforms.
        const Pose* bindPose = m_actorInstance->GetTransformData()->GetBindPose();
        for (AZ::u32 jointIndex : m_jointIndices)
        {
            const Transform& transform = pose.GetLocalSpaceTransform(jointIndex);
            const Transform& bindTransform = bindPose->GetLocalSpaceTransform(jointIndex);
            EXPECT_THAT(transform, IsClose(bindTransform));
        }

        // Sample the foot.
        Transform footTransform = pose.GetLocalSpaceTransform(m_footIndex);
        EXPECT_THAT(footTransform, IsClose(Transform::CreateIdentity()));

        // Make it a non-additive motion, or at least act like it.
        m_motion->SetIsAdditive(false);

        // Make sure we do not get an identity transform back now that it is a non-additive motion.
        pose.Zero();
        m_motion->Update(&pose, &pose, m_motionInstance);
        footTransform = pose.GetLocalSpaceTransform(m_footIndex);
        Transform expectedFootTransform;
        expectedFootTransform.Zero();
        EXPECT_THAT(footTransform, IsClose(expectedFootTransform));
    }
} // end namespace EMotionFX
