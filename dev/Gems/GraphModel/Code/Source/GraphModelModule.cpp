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

#if defined(GRAPHMODEL_EDITOR)
#include <GraphModelSystemComponent.h>
#endif

namespace GraphModel
{
    class GraphModelModule
        : public AZ::Module
    {
    public:
        AZ_RTTI(GraphModelModule, "{217B9E5D-C0FC-4D9D-AD75-AA3B23566A96}", AZ::Module);
        AZ_CLASS_ALLOCATOR(GraphModelModule, AZ::SystemAllocator, 0);

        GraphModelModule()
            : AZ::Module()
        {
            m_descriptors.insert(m_descriptors.end(), {
#if defined(GRAPHMODEL_EDITOR)
                GraphModelSystemComponent::CreateDescriptor(),
#endif
            });
        }

        /**
         * Add required SystemComponents to the SystemEntity.
         */
        AZ::ComponentTypeList GetRequiredSystemComponents() const override
        {
            return AZ::ComponentTypeList{
#if defined(GRAPHMODEL_EDITOR)
                azrtti_typeid<GraphModelSystemComponent>(),
#endif
            };
        }
    };
}

// DO NOT MODIFY THIS LINE UNLESS YOU RENAME THE GEM
// The first parameter should be GemName_GemIdLower
// The second should be the fully qualified name of the class above
AZ_DECLARE_MODULE_CLASS(GraphModel_0844f64a3acf4f5abf3a535dc9b63bc9, GraphModel::GraphModelModule)
