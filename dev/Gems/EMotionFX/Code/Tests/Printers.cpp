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

#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector3.h>
#include <Tests/Printers.h>

#include <string>
#include <ostream>

namespace AZ
{
    void PrintTo(const AZ::Vector3& vector, ::std::ostream* os)
    {
        *os << '(' << vector.GetX() << ", " << vector.GetY() << ", " << vector.GetZ() << ')';
    }

    void PrintTo(const AZ::Quaternion& quaternion, ::std::ostream* os)
    {
        *os << '(' << quaternion.GetX() << ", " << quaternion.GetY() << ", " << quaternion.GetZ() << ", " << quaternion.GetW() << ')';
    }
} // namespace AZ

namespace AZStd
{
    void PrintTo(const string& string, ::std::ostream* os)
    {
        *os << '"' << std::string(string.data(), string.size()) << '"';
    }
} // namespace AZStd

namespace MCore
{
    void PrintTo(const Quaternion& quaternion, ::std::ostream* os)
    {
        *os << "(x: " << quaternion.x
            << ", y: " << quaternion.y
            << ", z: " << quaternion.z
            << ", w: " << quaternion.w
            << ")";
    }
} // namespace MCore

namespace EMotionFX
{
    void PrintTo(const Transform& transform, ::std::ostream* os)
    {
        *os << "(pos: ";
        PrintTo(transform.mPosition, os);
        *os << ", rot: ";
        PrintTo(transform.mRotation, os);
#if !defined(EMFX_SCALE_DISABLED)
        *os << ", scale: ";
        PrintTo(transform.mScale, os);
#endif
        *os << ")";
    }
} // namespace EMotionFX
