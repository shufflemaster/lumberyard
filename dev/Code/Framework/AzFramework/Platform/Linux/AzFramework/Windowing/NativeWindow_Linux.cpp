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

#include <AzFramework/Windowing/NativeWindow.h>

namespace AzFramework
{
    class NativeWindowImpl_Linux final
        : public NativeWindow::Implementation
    {
    public:
        AZ_CLASS_ALLOCATOR(NativeWindowImpl_Linux, AZ::SystemAllocator, 0);
        NativeWindowImpl_Linux() = default;
        ~NativeWindowImpl_Linux() override = default;

        // NativeWindow::Implementation overrides...
        void InitWindow(const AZStd::string& title, const WindowGeometry& geometry, const WindowType windowType) override;
        NativeWindowHandle GetWindowHandle() const override;
    };

    NativeWindow::Implementation* NativeWindow::Implementation::Create()
    {
        return aznew NativeWindowImpl_Linux();
    }

    void NativeWindowImpl_Linux::InitWindow(const AZStd::string& title, const WindowGeometry& geometry, const WindowType windowType)
    {
        AZ_UNUSED(windowType);
        AZ_UNUSED(title);

        m_width = geometry.m_width;
        m_height = geometry.m_height;
    }

    NativeWindowHandle NativeWindowImpl_Linux::GetWindowHandle() const
    {
        AZ_Assert(false, "NativeWindow not implemented for Linux");
        return nullptr;
    }

} // namespace AzFramework
