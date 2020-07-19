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

#pragma once

// Qt
#include <QString>

// Landscape Canvas
#include <Editor/Core/Core.h>
#include <Editor/Nodes/BaseNode.h>

namespace AZ
{
    class ReflectContext;
}

namespace LandscapeCanvas
{
    class AssetWeightSelectorNode
        : public BaseNode
    {
    public:
        AZ_CLASS_ALLOCATOR(AssetWeightSelectorNode, AZ::SystemAllocator, 0);
        AZ_RTTI(AssetWeightSelectorNode, "{083CA722-638B-4E14-836B-2614451C2A91}", BaseNode);

        static void Reflect(AZ::ReflectContext* context);

        AssetWeightSelectorNode() = default;
        explicit AssetWeightSelectorNode(GraphModel::GraphPtr graph);

        const BaseNodeType GetBaseNodeType() const override;
        const bool ShouldShowEntityName() const override;

        static const QString TITLE;
        const char* GetTitle() const override;

    protected:
        void RegisterSlots() override;
    };
}
