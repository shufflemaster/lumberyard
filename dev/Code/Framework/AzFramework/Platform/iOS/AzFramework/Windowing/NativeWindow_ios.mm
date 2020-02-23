
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

#include <UIKit/UIKit.h>
@class NSWindow;

namespace AzFramework
{
    class NativeWindowImpl_Ios final
        : public NativeWindow::Implementation
    {
    public:
        AZ_CLASS_ALLOCATOR(NativeWindowImpl_Ios, AZ::SystemAllocator, 0);
        NativeWindowImpl_Ios() = default;
        ~NativeWindowImpl_Ios() override;
        
        // NativeWindow::Implementation overrides...
        void InitWindow(const AZStd::string& title, const WindowGeometry& geometry, const WindowType windowType) override;
        NativeWindowHandle GetWindowHandle() const override;

    private:
        UIWindow* m_nativeWindow;
    };
    
    NativeWindow::Implementation* NativeWindow::Implementation::Create()
    {
        return aznew NativeWindowImpl_Ios();
    }
    
    NativeWindowImpl_Ios::~NativeWindowImpl_Ios()
    {
        if (m_nativeWindow)
        {
            [m_nativeWindow release];
            m_nativeWindow = nil;
        }
    }
    
    void NativeWindowImpl_Ios::InitWindow(const AZStd::string& title, const WindowGeometry& geometry, const WindowType windowType)
    {
        AZ_UNUSED(windowType);
        AZ_UNUSED(title);

        CGRect screenBounds = [[UIScreen mainScreen] bounds];
        m_nativeWindow = [[UIWindow alloc] initWithFrame: screenBounds];
        [m_nativeWindow makeKeyAndVisible];

        m_width = geometry.m_width;
        m_height = geometry.m_height;
    }
    
    NativeWindowHandle NativeWindowImpl_Ios::GetWindowHandle() const
    {
        return m_nativeWindow;
    }
   
} // namespace AzFramework

