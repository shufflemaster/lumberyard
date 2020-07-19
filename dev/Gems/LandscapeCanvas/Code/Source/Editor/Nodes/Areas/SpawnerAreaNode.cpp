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

// Qt
#include <QObject>

// AZ
#include <AzCore/Serialization/SerializeContext.h>

// Landscape Canvas
#include "SpawnerAreaNode.h"

namespace LandscapeCanvas
{
    void SpawnerAreaNode::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<SpawnerAreaNode, BaseAreaNode>()
                ->Version(0)
                ;
        }
    }

    const QString SpawnerAreaNode::TITLE = QObject::tr("Vegetation Layer Spawner");

    SpawnerAreaNode::SpawnerAreaNode(GraphModel::GraphPtr graph)
        : BaseAreaNode(graph)
    {
        RegisterSlots();
        CreateSlotData();
    }
} // namespace LandscapeCanvas
