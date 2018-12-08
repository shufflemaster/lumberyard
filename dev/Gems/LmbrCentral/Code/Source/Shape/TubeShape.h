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

#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/containers/vector.h>
#include <Cry_Math.h>
#include <LmbrCentral/Shape/ShapeComponentBus.h>
#include <LmbrCentral/Shape/TubeShapeComponentBus.h>
#include <LmbrCentral/Shape/SplineComponentBus.h>
#include <LmbrCentral/Shape/SplineAttribute.h>

#include "ShapeGeometryUtil.h"

namespace AzFramework
{
    class EntityDebugDisplayRequests;
}

namespace LmbrCentral
{
    class TubeShape
        : public ShapeComponentRequestsBus::Handler
        , public TubeShapeComponentRequestsBus::Handler
        , private SplineComponentNotificationBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_RTTI(TubeShape, "{8DF865F3-D155-402D-AF64-9342CE9E9E60}")
        AZ_CLASS_ALLOCATOR(TubeShape, AZ::SystemAllocator, 0)

        static void Reflect(AZ::SerializeContext& context);

        void Activate(AZ::EntityId entityId);
        void Deactivate();

        // ShapeComponentRequestsBus
        AZ::Crc32 GetShapeType() override { return AZ_CRC("Tube", 0xfd30de9e); }
        AZ::Aabb GetEncompassingAabb() override;
        bool IsPointInside(const AZ::Vector3& point)  override;
        float DistanceSquaredFromPoint(const AZ::Vector3& point) override;
        bool IntersectRay(const AZ::Vector3& src, const AZ::Vector3& dir, AZ::VectorFloat& distance) override;

        // TubeShapeComponentRequestsBus
        void SetRadius(float radius) override;
        float GetRadius() const override;
        float GetVariableRadius(int index) const override;
        void SetVariableRadius(int index, float radius) override;
        float GetTotalRadius(const AZ::SplineAddress& address) const override;
        const SplineAttribute<float>& GetRadiusAttribute() const override;

        AZ::SplinePtr GetSpline();
        const AZ::Transform& GetCurrentTransform() const { return m_currentTransform; }

    private:
        // TransformNotificationBus
        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

        // SplineComponentNotificationBus
        void OnSplineChanged() override;

        AZ::SplinePtr m_spline; ///< Spline pointer.
        SplineAttribute<float> m_variableRadius; ///< Variable radius defined at each spline point.
        AZ::Transform m_currentTransform; ///< Caches the current World transform.
        AZ::EntityId m_entityId; ///< The Id of the entity the shape is attached to.
        float m_radius = 1.f; ///< Radius of the tube.
    };

    /**
     * Generates a Tube mesh with filled surface and outlines.
     */
    void GenerateTubeMesh(
        const AZ::SplinePtr& spline, const SplineAttribute<float>& variableRadius,
        float radius, AZ::u32 capSegments, AZ::u32 sides, AZStd::vector<AZ::Vector3>& vertexBufferOut,
        AZStd::vector<AZ::u32>& indexBufferOut, AZStd::vector<AZ::Vector3>& lineBufferOut);

    /**
     * Configuration for how TubeShape debug drawing should appear (tesselation parameters etc).
     */
    struct TubeShapeMeshConfig
    {
        AZ_CLASS_ALLOCATOR(TubeShapeMeshConfig, AZ::SystemAllocator, 0)
        AZ_RTTI(TubeShapeMeshConfig, "{90791900-060F-4F0B-9552-D6E67572B317}")

        TubeShapeMeshConfig() = default;
        virtual ~TubeShapeMeshConfig() = default;

        static void Reflect(AZ::ReflectContext* context);

        AZ::u32 m_endSegments = 9; ///< The number of endcap segments displayed in the editor.
        AZ::u32 m_sides = 32; ///< The number of sides of the tube displayed in the editor.
    };
} // namespace LmbrCentral