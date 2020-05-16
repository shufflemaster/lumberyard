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

#include <PythonSystemComponent.h>
#include <PythonReflectionComponent.h>
#include <PythonMarshalComponent.h>
#include <PythonLogSymbolsComponent.h>

namespace EditorPythonBindings
{
    class EditorPythonBindingsModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(EditorPythonBindingsModule, "{851B9E35-4FD5-49B1-8207-E40D4BBA36CC}", AZ::Module);
        AZ_CLASS_ALLOCATOR(EditorPythonBindingsModule, AZ::SystemAllocator, 0);

        EditorPythonBindingsModule()
            : AZ::Module()
        {
            m_descriptors.insert(m_descriptors.end(), 
            {
                PythonSystemComponent::CreateDescriptor(),
                PythonReflectionComponent::CreateDescriptor(),
                PythonMarshalComponent::CreateDescriptor(),
                PythonLogSymbolsComponent::CreateDescriptor(),
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList
            {
                azrtti_typeid<PythonSystemComponent>(),
                azrtti_typeid<PythonReflectionComponent>(),
                azrtti_typeid<PythonMarshalComponent>(),
                azrtti_typeid<PythonLogSymbolsComponent>()
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(EditorPythonBindings_b658359393884c4381c2fe2952b1472a, EditorPythonBindings::EditorPythonBindingsModule)
