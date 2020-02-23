/*
* All or portions of this file Copyright(c) Amazon.com, Inc.or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution(the "License").All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file.Do not
* remove or modify any license notices.This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
#include <AzCore/std/sort.h>
#include <AzToolsFramework/Fingerprinting/TypeFingerprinter.h>
#include "AzCore/Component/Component.h"

namespace AzToolsFramework
{
    namespace Fingerprinting
    {
        TypeFingerprinter::TypeFingerprinter(const AZ::SerializeContext& serializeContext)
            : m_serializeContext(serializeContext)
        {
            m_serializeContext.EnumerateAll(
                [this](const AZ::SerializeContext::ClassData* classData, const AZ::TypeId&)
            {
                CreateFingerprint(*classData);
                return true;
            });
        }

        TypeFingerprint TypeFingerprinter::CreateFingerprint(const AZ::SerializeContext::ClassData& classData)
        {
            // Ensures hash will change if a service is moved from, say, required to dependent.
            static const size_t kRequiredServiceKey = AZ_CRC("RequiredServiceKey", 0x22e125a6);
            static const size_t kDependentServiceKey = AZ_CRC("DependentServiceKey", 0x380e6c63);
            static const size_t kProvidedServiceKey = AZ_CRC("ProvidedServiceKey", 0xd3cc7058);
            static const size_t kIncompatibleServiceKey = AZ_CRC("IncompatibleServiceKey", 0x95ee560f);

            TypeFingerprint fingerprint = 0;

            AZStd::hash_combine(fingerprint, classData.m_typeId);
            AZStd::hash_combine(fingerprint, classData.m_version);

            bool isComponentObject = classData.m_azRtti ? classData.m_azRtti->IsTypeOf(azrtti_typeid<AZ::Component>()) : false;

            if(isComponentObject)
            {
                AZ::ComponentDescriptor::DependencyArrayType services;
                AZ::ComponentDescriptor* componentDescriptor = nullptr;
                AZ::ComponentDescriptorBus::EventResult(componentDescriptor, classData.m_azRtti->GetTypeId(), &AZ::ComponentDescriptorBus::Events::GetDescriptor);

                if (componentDescriptor)
                {
                    services.clear();
                    componentDescriptor->GetRequiredServices(services, nullptr);
                    AZStd::sort(services.begin(), services.end());
                    AZStd::hash_combine(fingerprint, kRequiredServiceKey);
                    AZStd::hash_range(fingerprint, services.begin(), services.end());

                    services.clear();
                    componentDescriptor->GetDependentServices(services, nullptr);
                    AZStd::sort(services.begin(), services.end());
                    AZStd::hash_combine(fingerprint, kDependentServiceKey);
                    AZStd::hash_range(fingerprint, services.begin(), services.end());

                    services.clear();
                    componentDescriptor->GetProvidedServices(services, nullptr);
                    AZStd::sort(services.begin(), services.end());
                    AZStd::hash_combine(fingerprint, kProvidedServiceKey);
                    AZStd::hash_range(fingerprint, services.begin(), services.end());

                    services.clear();
                    componentDescriptor->GetIncompatibleServices(services, nullptr);
                    AZStd::sort(services.begin(), services.end());
                    AZStd::hash_combine(fingerprint, kIncompatibleServiceKey);
                    AZStd::hash_range(fingerprint, services.begin(), services.end());
                }
            }

            for (const AZ::SerializeContext::ClassElement& element : classData.m_elements)
            {
                AZStd::hash_combine(fingerprint, element.m_typeId);
                AZStd::hash_range(fingerprint, element.m_name, element.m_name + strlen(element.m_name));
                AZStd::hash_combine(fingerprint, element.m_flags);
            }

            m_typeFingerprints[classData.m_typeId] = fingerprint;
            return fingerprint;
        }

        void TypeFingerprinter::GatherAllTypesInObject(const void* instance, const AZ::TypeId& typeId, TypeCollection& outTypeCollection) const
        {
            AZ::SerializeContext::BeginElemEnumCB elementCallback =
                [&outTypeCollection](void* /* instance pointer */, const AZ::SerializeContext::ClassData* classData, const AZ::SerializeContext::ClassElement* /* classElement*/)
            {
                outTypeCollection.insert(classData->m_typeId);
                return true;
            };

            m_serializeContext.EnumerateInstanceConst(instance, typeId, elementCallback, nullptr, AZ::SerializeContext::ENUM_ACCESS_FOR_READ, nullptr, nullptr);
        }

        TypeFingerprint TypeFingerprinter::GenerateFingerprintForAllTypes(const TypeCollection& typeCollection) const
        {
            // Sort all types before fingerprinting
            AZStd::vector<AZ::TypeId> sortedTypes;
            sortedTypes.reserve(typeCollection.size());
            sortedTypes.insert(sortedTypes.begin(), typeCollection.begin(), typeCollection.end());
            AZStd::sort(sortedTypes.begin(), sortedTypes.end());

            TypeFingerprint fingerprint = 0;

            for (const AZ::TypeId& typeId : sortedTypes)
            {
                TypeFingerprint typeFingerprint = GetFingerprint(typeId);
                AZStd::hash_combine(fingerprint, typeFingerprint);
            }

            return fingerprint;
        }

        TypeFingerprint TypeFingerprinter::GenerateFingerprintForAllTypesInObject(const void* instance, const AZ::TypeId& typeId) const
        {
            TypeCollection types;
            GatherAllTypesInObject(instance, typeId, types);
            return GenerateFingerprintForAllTypes(types);
        }

    } // namespace Fingerprinting
} // namespace AzToolsFramework
