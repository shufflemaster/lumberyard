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
#include <AzCore/Android/Utils.h>
#include <android/native_window.h>

namespace AzFramework
{
    class NativeWindowImpl_Android final
        : public NativeWindow::Implementation
    {
    public:
        AZ_CLASS_ALLOCATOR(NativeWindowImpl_Android, AZ::SystemAllocator, 0);
        NativeWindowImpl_Android() = default;
        ~NativeWindowImpl_Android() override = default;

        // NativeWindow::Implementation overrides...
        void InitWindow(const AZStd::string& title, const WindowGeometry& geometry, const WindowType windowType) override;
        NativeWindowHandle GetWindowHandle() const override;

    private:
        ANativeWindow* m_nativeWindow = nullptr;
    };

    NativeWindow::Implementation* NativeWindow::Implementation::Create()
    {
        return aznew NativeWindowImpl_Android();
    }

    void NativeWindowImpl_Android::InitWindow(const AZStd::string& title, const WindowGeometry& geometry, const WindowType windowType)
    {
        AZ_UNUSED(windowType);
        AZ_UNUSED(title);
        m_nativeWindow = AZ::Android::Utils::GetWindow();

        m_width = geometry.m_width;
        m_height = geometry.m_height;

        if (m_nativeWindow)
        {
            ANativeWindow_setBuffersGeometry(m_nativeWindow, m_width, m_height, ANativeWindow_getFormat(m_nativeWindow));
        }
    }

    NativeWindowHandle NativeWindowImpl_Android::GetWindowHandle() const
    {
        return reinterpret_cast<NativeWindowHandle>(m_nativeWindow);
    }

} // namespace AzFramework