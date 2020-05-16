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
#include <AzCore/Module/Module.h>

#include <ExpressionEvaluationSystemComponent.h>

namespace ExpressionEvaluation
{
    class ExpressionEvaluationModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(ExpressionEvaluationModule, "{3183322D-3AE1-4B8B-86D7-870DA60DC175}", AZ::Module);
        AZ_CLASS_ALLOCATOR(ExpressionEvaluationModule, AZ::SystemAllocator, 0);

        ExpressionEvaluationModule()
            : AZ::Module()
        {
            // Push results of [MyComponent]::CreateDescriptor() into m_descriptors here.
            m_descriptors.insert(m_descriptors.end(), {
                ExpressionEvaluationSystemComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
                azrtti_typeid<ExpressionEvaluationSystemComponent>(),
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(ExpressionEvaluation_4c6f9df57ca2468f93c8d860ee6a1167, ExpressionEvaluation::ExpressionEvaluationModule)
