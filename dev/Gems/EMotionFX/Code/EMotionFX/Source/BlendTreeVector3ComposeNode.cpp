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

#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Serialization/EditContext.h>
#include "BlendTreeVector3ComposeNode.h"


namespace EMotionFX
{
    AZ_CLASS_ALLOCATOR_IMPL(BlendTreeVector3ComposeNode, AnimGraphAllocator, 0)

    BlendTreeVector3ComposeNode::BlendTreeVector3ComposeNode()
        : AnimGraphNode()
    {
        // setup the input ports
        InitInputPorts(3);
        SetupInputPortAsNumber("x", INPUTPORT_X, PORTID_INPUT_X);
        SetupInputPortAsNumber("y", INPUTPORT_Y, PORTID_INPUT_Y);
        SetupInputPortAsNumber("z", INPUTPORT_Z, PORTID_INPUT_Z);

        // setup the output ports
        InitOutputPorts(1);
        SetupOutputPort("Vector", OUTPUTPORT_VECTOR, MCore::AttributeVector3::TYPE_ID, PORTID_OUTPUT_VECTOR);
    }

    BlendTreeVector3ComposeNode::~BlendTreeVector3ComposeNode()
    {
    }

    bool BlendTreeVector3ComposeNode::InitAfterLoading(AnimGraph* animGraph)
    {
        if (!AnimGraphNode::InitAfterLoading(animGraph))
        {
            return false;
        }

        InitInternalAttributesForAllInstances();

        Reinit();
        return true;
    }

    const char* BlendTreeVector3ComposeNode::GetPaletteName() const
    {
        return "Vector3 Compose";
    }

    AnimGraphObject::ECategory BlendTreeVector3ComposeNode::GetPaletteCategory() const
    {
        return AnimGraphObject::CATEGORY_MATH;
    }

    void BlendTreeVector3ComposeNode::Update(AnimGraphInstance* animGraphInstance, float timePassedInSeconds)
    {
        UpdateAllIncomingNodes(animGraphInstance, timePassedInSeconds);
        UpdateOutputPortValues(animGraphInstance);
    }

    void BlendTreeVector3ComposeNode::Output(AnimGraphInstance* animGraphInstance)
    {
        OutputAllIncomingNodes(animGraphInstance);
        UpdateOutputPortValues(animGraphInstance);
    }

    void BlendTreeVector3ComposeNode::UpdateOutputPortValues(AnimGraphInstance* animGraphInstance)
    {
        const float x = GetInputNumberAsFloat(animGraphInstance, INPUTPORT_X);
        const float y = GetInputNumberAsFloat(animGraphInstance, INPUTPORT_Y);
        const float z = GetInputNumberAsFloat(animGraphInstance, INPUTPORT_Z);
        GetOutputVector3(animGraphInstance, OUTPUTPORT_VECTOR)->SetValue(AZ::PackedVector3f(x, y, z));
    }

    void BlendTreeVector3ComposeNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (!serializeContext)
        {
            return;
        }

        serializeContext->Class<BlendTreeVector3ComposeNode, AnimGraphNode>()
            ->Version(1);


        AZ::EditContext* editContext = serializeContext->GetEditContext();
        if (!editContext)
        {
            return;
        }

        editContext->Class<BlendTreeVector3ComposeNode>("Vector3 Compose", "Vector3 compose attributes")
            ->ClassElement(AZ::Edit::ClassElements::EditorData, "")
            ->Attribute(AZ::Edit::Attributes::AutoExpand, "")
            ->Attribute(AZ::Edit::Attributes::Visibility, AZ::Edit::PropertyVisibility::ShowChildrenOnly)
            ;
    }
} // namespace EMotionFX