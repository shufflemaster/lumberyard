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

#include "ManipulatorDebug.h"

#include <AzFramework/Entity/EntityDebugDisplayBus.h>

namespace AzToolsFramework
{
    void DrawAxis(
        AzFramework::DebugDisplayRequests& display, const AZ::Vector3& position, const AZ::Vector3& direction)
    {
        display.SetLineWidth(4.0f);
        display.SetColor(AZ::Color{ 1.0f, 1.0f, 0.0f, 1.0f });
        display.DrawLine(position, position + direction);
        display.SetLineWidth(1.0f);
    }

    void DrawTransformAxes(
        AzFramework::DebugDisplayRequests& display, const AZ::Transform& transform)
    {
        const float AxisLength = 0.5f;

        display.SetLineWidth(4.0f);
        display.SetColor(AZ::Color{ 1.0f, 0.0f, 0.0f, 1.0f });
        display.DrawLine(
            transform.GetTranslation(),
            transform.GetTranslation() + transform.GetBasisX().GetNormalizedSafeExact() * AxisLength);
        display.SetColor(AZ::Color{ 0.0f, 1.0f, 0.0f, 1.0f });
        display.DrawLine(
            transform.GetTranslation(),
            transform.GetTranslation() + transform.GetBasisY().GetNormalizedSafeExact() * AxisLength);
        display.SetColor(AZ::Color{ 0.0f, 0.0f, 1.0f, 1.0f });
        display.DrawLine(
            transform.GetTranslation(),
            transform.GetTranslation() + transform.GetBasisZ().GetNormalizedSafeExact() * AxisLength);
        display.SetLineWidth(1.0f);
    }
} // namespace AzToolsFramework
