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

#include "WhiteBox_precompiled.h"

#include "WhiteBoxViewportConstants.h"

namespace WhiteBox
{
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxEdgeHoveredColor, AZ::Color::CreateFromRgba(255, 100, 0, 255), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxEdgeMeshColor, AZ::Color::CreateFromRgba(127, 127, 127, 76), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxEdgeUserColor, AZ::Color::CreateFromRgba(0, 0, 0, 76), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxPolygonHoveredColor, AZ::Color::CreateFromRgba(255, 175, 0, 127), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxPolygonHoveredOutlineColor, AZ::Color::CreateFromRgba(255, 100, 0, 255), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxSelectedModifierColor, AZ::Color::CreateFromRgba(0, 150, 255, 255), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxVertexDeselectedColor, AZ::Color::CreateFromRgba(25, 100, 255, 150), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxVertexHiddenRestoreColor, AZ::Color::CreateFromRgba(200, 200, 200, 255), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxVertexHoveredColor, AZ::Color::CreateFromRgba(255, 100, 0, 255), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxVertexRestoreColor, AZ::Color::CreateFromRgba(50, 50, 50, 150), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(
        AZ::Color, cl_whiteBoxVertexSelectedModifierColor, AZ::Color::CreateFromRgba(0, 150, 255, 200), nullptr,
        AZ::ConsoleFunctorFlags::Null, "");

    AZ_CVAR_EXTERNABLE(float, cl_whiteBoxVertexManipulatorSize, 0.125f, nullptr, AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(float, cl_whiteBoxMouseClickDeltaThreshold, 0.001f, nullptr, AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(float, cl_whiteBoxModifierMidpointEpsilon, 0.01f, nullptr, AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(float, cl_whiteBoxEdgeVisualWidth, 5.0f, nullptr, AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(float, cl_whiteBoxEdgeSelectionWidth, 0.075f, nullptr, AZ::ConsoleFunctorFlags::Null, "");
    AZ_CVAR_EXTERNABLE(float, cl_whiteBoxSelectedEdgeVisualWidth, 6.0f, nullptr, AZ::ConsoleFunctorFlags::Null, "");
} // namespace WhiteBox
