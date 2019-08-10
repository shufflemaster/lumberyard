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
#include <AzCore/UserSettings/UserSettingsProvider.h>
#include <AzCore/Component/Component.h>

namespace AZ
{
    class UserSettingsComponentRequests
        : public AZ::EBusTraits
    {
    public:
        virtual void Load() = 0;
        virtual void Save() = 0;
        virtual void Finalize() = 0;

        // Used for unit tests that shouldn't save user settings.
        virtual void DisableSaveOnFinalize() = 0;
    };
    using UserSettingsComponentRequestBus = AZ::EBus<UserSettingsComponentRequests>;

    /**
     * UserSettingsComponent
     */
    class UserSettingsComponent
        : public Component
        , protected UserSettingsComponentRequestBus::Handler
    {
    public:
        AZ_COMPONENT(UserSettingsComponent, "{A316662A-6C3E-43E6-BC61-4B375D0D83B4}");

        UserSettingsComponent(u32 providerId = UserSettings::CT_GLOBAL);
        ~UserSettingsComponent();

        void SetProviderId(u32 providerId) { m_providerId = providerId; }
        u32 GetProviderId() { return m_providerId; }

    protected:
        //////////////////////////////////////////////////////////////////////////
        // Component base
        void Activate() override;
        void Deactivate() override;
        //////////////////////////////////////////////////////////////////////////

        //////////////////////////////////////////////////////////////////////////
        // UserSettingsComponentRequestBus
        void Load() override;
        void Save() override;
        void Finalize() override;
        void DisableSaveOnFinalize() override { m_saveOnFinalize = false; }
        //////////////////////////////////////////////////////////////////////////

        /// \ref ComponentDescriptor::GetProvidedServices
        static void GetProvidedServices(ComponentDescriptor::DependencyArrayType& provided);
        /// \ref ComponentDescriptor::GetDependentServices
        static void GetDependentServices(ComponentDescriptor::DependencyArrayType& dependent);
        /// \red ComponentDescriptor::Reflect
        static void Reflect(ReflectContext* reflection);

        UserSettingsProvider    m_provider;
        u32                     m_providerId;
        bool                    m_saveOnShutdown = true;
        bool                    m_saveOnFinalize = true;
    };
}   // namespace AZ
