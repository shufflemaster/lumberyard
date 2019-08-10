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

#include <AzFramework/API/ApplicationAPI_ios.h>
#include <AzFramework/Application/Application.h>

#include <UIKit/UIKit.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AzFramework
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    class ApplicationIos
        : public Application::Implementation
        , public IosLifecycleEvents::Bus::Handler
    {
    public:
        ////////////////////////////////////////////////////////////////////////////////////////////
        AZ_CLASS_ALLOCATOR(ApplicationIos, AZ::SystemAllocator, 0);
        ApplicationIos();
        ~ApplicationIos() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // IosLifecycleEvents
        void OnWillResignActive() override;
        void OnDidBecomeActive() override;
        void OnDidEnterBackground() override;
        void OnWillEnterForeground() override;
        void OnWillTerminate() override;
        void OnDidReceiveMemoryWarning() override;

        ////////////////////////////////////////////////////////////////////////////////////////////
        // Application::Implementation
        void PumpSystemEventLoopOnce() override;
        void PumpSystemEventLoopUntilEmpty() override;

    private:
        ApplicationLifecycleEvents::Event m_lastEvent;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
    Application::Implementation* Application::Implementation::Create()
    {
        return aznew ApplicationIos();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    const char* Application::Implementation::GetAppRootPath()
    {
        return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ApplicationIos::ApplicationIos()
        : m_lastEvent(ApplicationLifecycleEvents::Event::None)
    {
        IosLifecycleEvents::Bus::Handler::BusConnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    ApplicationIos::~ApplicationIos()
    {
        IosLifecycleEvents::Bus::Handler::BusDisconnect();
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::OnWillResignActive()
    {
        EBUS_EVENT(ApplicationLifecycleEvents::Bus, OnApplicationConstrained, m_lastEvent);
        m_lastEvent = ApplicationLifecycleEvents::Event::Constrain;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::OnDidBecomeActive()
    {
        EBUS_EVENT(ApplicationLifecycleEvents::Bus, OnApplicationUnconstrained, m_lastEvent);
        m_lastEvent = ApplicationLifecycleEvents::Event::Unconstrain;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::OnDidEnterBackground()
    {
        EBUS_EVENT(ApplicationLifecycleEvents::Bus, OnApplicationSuspended, m_lastEvent);
        m_lastEvent = ApplicationLifecycleEvents::Event::Suspend;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::OnWillEnterForeground()
    {
        EBUS_EVENT(ApplicationLifecycleEvents::Bus, OnApplicationResumed, m_lastEvent);
        m_lastEvent = ApplicationLifecycleEvents::Event::Resume;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::OnWillTerminate()
    {
        EBUS_EVENT(ApplicationLifecycleEvents::Bus, OnMobileApplicationWillTerminate);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::OnDidReceiveMemoryWarning()
    {
        EBUS_EVENT(ApplicationLifecycleEvents::Bus, OnMobileApplicationLowMemoryWarning);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::PumpSystemEventLoopOnce()
    {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, TRUE);
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////
    void ApplicationIos::PumpSystemEventLoopUntilEmpty()
    {
        SInt32 result;
        do
        {
            result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, DBL_EPSILON, TRUE);
        }
        while (result == kCFRunLoopRunHandledSource);
    }
} // namespace AzFramework
