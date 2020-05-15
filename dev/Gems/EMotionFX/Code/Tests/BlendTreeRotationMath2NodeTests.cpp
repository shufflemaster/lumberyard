
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

#include "AnimGraphFixture.h"
#include <EMotionFX/Source/AnimGraph.h>
#include <EMotionFX/Source/AnimGraphStateMachine.h>
#include <EMotionFX/Source/BlendTree.h>
#include <EMotionFX/Source/BlendTreeFinalNode.h>
#include <EMotionFX/Source/BlendTreeParameterNode.h>
#include <EMotionFX/Source/EMotionFXManager.h>
#include <EMotionFX/Source/BlendTreeGetTransformNode.h>
#include <EMotionFX/Source/BlendTreeSetTransformNode.h>
#include <EMotionFX/Source/AnimGraphBindPoseNode.h>
#include <EMotionFX/Source/BlendTreeRotationMath2Node.h>


namespace EMotionFX
{
    class BlendTreeRotationMath2NodeTests : public AnimGraphFixture
    {
    public:
        void ConstructGraph() override
        {
            AnimGraphFixture::ConstructGraph();
            m_blendTreeAnimGraph = AnimGraphFactory::Create<OneBlendTreeNodeAnimGraph>();
            m_rootStateMachine = m_blendTreeAnimGraph->GetRootStateMachine();
            m_blendTree = m_blendTreeAnimGraph->GetBlendTreeNode();

            AnimGraphBindPoseNode* bindPoseNode = aznew AnimGraphBindPoseNode();
            m_getTransformNode = aznew BlendTreeGetTransformNode();
            m_rotationMathNode = aznew BlendTreeRotationMath2Node();
            m_setTransformNode = aznew BlendTreeSetTransformNode();
            BlendTreeFinalNode* finalNode = aznew BlendTreeFinalNode();

            m_blendTree->AddChildNode(bindPoseNode);
            m_blendTree->AddChildNode(m_getTransformNode);
            m_blendTree->AddChildNode(m_rotationMathNode);
            m_blendTree->AddChildNode(m_setTransformNode);
            m_blendTree->AddChildNode(finalNode);

            m_getTransformNode->AddUnitializedConnection(bindPoseNode, AnimGraphBindPoseNode::PORTID_OUTPUT_POSE, BlendTreeFinalNode::PORTID_INPUT_POSE);
            m_setTransformNode->AddUnitializedConnection(bindPoseNode, AnimGraphBindPoseNode::PORTID_OUTPUT_POSE, BlendTreeFinalNode::PORTID_INPUT_POSE);
            m_rotationMathNode->AddUnitializedConnection(m_getTransformNode, BlendTreeGetTransformNode::OUTPUTPORT_ROTATION, BlendTreeRotationMath2Node::INPUTPORT_X);
            m_setTransformNode->AddUnitializedConnection(m_rotationMathNode, BlendTreeRotationMath2Node::OUTPUTPORT_RESULT_QUATERNION, BlendTreeSetTransformNode::INPUTPORT_ROTATION);
            finalNode->AddUnitializedConnection(m_setTransformNode, BlendTreeSetTransformNode::PORTID_OUTPUT_POSE, BlendTreeFinalNode::PORTID_INPUT_POSE);

            m_blendTreeAnimGraph->InitAfterLoading();
        }

        void SetUp() override
        {
            AnimGraphFixture::SetUp();
            m_animGraphInstance->Destroy();
            m_animGraphInstance = m_blendTreeAnimGraph->GetAnimGraphInstance(m_actorInstance, m_motionSet);
        }

        AZStd::unique_ptr<OneBlendTreeNodeAnimGraph> m_blendTreeAnimGraph;
        BlendTree* m_blendTree = nullptr;
        BlendTreeGetTransformNode* m_getTransformNode = nullptr;
        BlendTreeRotationMath2Node* m_rotationMathNode = nullptr;
        BlendTreeSetTransformNode* m_setTransformNode = nullptr;
    };

    TEST_F(BlendTreeRotationMath2NodeTests, EvalauteTranslationBlending)
    {
        BlendTreeGetTransformNode::UniqueData* getNodeUniqueData = static_cast<BlendTreeGetTransformNode::UniqueData*>(m_getTransformNode->FindUniqueNodeData(m_animGraphInstance));
        getNodeUniqueData->m_nodeIndex = 0;

        BlendTreeSetTransformNode::UniqueData* setNodeUniqueData = static_cast<BlendTreeSetTransformNode::UniqueData*>(m_setTransformNode->FindUniqueNodeData(m_animGraphInstance));
        setNodeUniqueData->m_nodeIndex = 0;

        m_getTransformNode->OnUpdateUniqueData(m_animGraphInstance);
        m_setTransformNode->OnUpdateUniqueData(m_animGraphInstance);

        AZ::Quaternion expectedRotation = AZ::Quaternion::CreateRotationY(MCore::Math::pi * 0.25f);

        m_rotationMathNode->SetDefaultValue(expectedRotation);

        Evaluate();
        Transform outputRoot = GetOutputTransform();
        Transform expected;
        expected.Identity();
        expected.Set(AZ::Vector3::CreateZero(), expectedRotation);
        bool success = AZ::IsClose(expected.mRotation.GetW(), outputRoot.mRotation.GetW(), 0.0001f);
        success = success && AZ::IsClose(expected.mRotation.GetX(), outputRoot.mRotation.GetX(), 0.0001f);
        success = success && AZ::IsClose(expected.mRotation.GetY(), outputRoot.mRotation.GetY(), 0.0001f);
        success = success && AZ::IsClose(expected.mRotation.GetZ(), outputRoot.mRotation.GetZ(), 0.0001f);

        m_rotationMathNode->SetMathFunction(EMotionFX::BlendTreeRotationMath2Node::MATHFUNCTION_INVERSE_MULTIPLY);
        
        expectedRotation = expectedRotation.GetInverseFull();
        Evaluate();
        outputRoot = GetOutputTransform();
        expected.Identity();
        expected.Set(AZ::Vector3::CreateZero(), expectedRotation);
        success = success && AZ::IsClose(expected.mRotation.GetW(), outputRoot.mRotation.GetW(), 0.0001f);
        success = success && AZ::IsClose(expected.mRotation.GetX(), outputRoot.mRotation.GetX(), 0.0001f);
        success = success && AZ::IsClose(expected.mRotation.GetY(), outputRoot.mRotation.GetY(), 0.0001f);
        success = success && AZ::IsClose(expected.mRotation.GetZ(), outputRoot.mRotation.GetZ(), 0.0001f);


        ASSERT_TRUE(success);
    }

} // end namespace EMotionFX
