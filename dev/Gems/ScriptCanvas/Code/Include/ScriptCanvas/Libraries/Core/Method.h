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

#include <AzCore/std/containers/map.h>
#include <AzCore/std/parallel/mutex.h>
#include <ScriptCanvas/Core/Graph.h>
#include <ScriptCanvas/Core/Node.h>

namespace AZ
{
    class BehaviorMethod;
}

namespace ScriptCanvas
{
    using Namespaces = AZStd::vector<AZStd::string>;

    AZ::Outcome<void, AZStd::string> IsExposable(const AZ::BehaviorMethod& method);

    namespace Nodes
    {
        namespace Core
        {
            enum class MethodType
            {
                Event,
                Free,
                Member,
                Count,
            };

            class Method : public Node
            {
            public:
                AZ_COMPONENT(Method, "{E42861BD-1956-45AE-8DD7-CCFC1E3E5ACF}", Node);

                static void Reflect(AZ::ReflectContext* reflectContext);

                Method() = default;

                ~Method() = default;

                AZ_INLINE AZStd::string GetNodeName() const override { return GetName(); }

                AZ_INLINE const AZStd::string& GetName() const { return m_methodName; }
                AZ_INLINE const AZStd::string& GetRawMethodClassName() const { return m_className; }

                AZ_INLINE const AZStd::string& GetMethodClassName() const { return m_classNamePretty; }
                AZ_INLINE MethodType GetMethodType() const { return m_methodType; }

                bool IsObjectClass(AZStd::string_view objectClass) const { return objectClass.compare(m_className) == 0; }
                bool IsMethod(AZStd::string_view methodName) const { return methodName.compare(m_methodName) == 0; }

                void InitializeClass(const Namespaces& namespaces, const AZStd::string& className, const AZStd::string& methodName);

                void InitializeClassOrBus(const Namespaces& namespaces, const AZStd::string& className, const AZStd::string& methodName);

                void InitializeEvent(const Namespaces& namespaces, const AZStd::string& busName, const AZStd::string& eventName);

                void InitializeFree(const Namespaces& namespaces, const AZStd::string& methodName);

                AZ_INLINE bool IsValid() const { return m_method != nullptr; }

                bool HasBusID() const { return (m_method == nullptr) ? false : m_method->HasBusId(); }
                bool HasResult() const { return (m_method == nullptr) ? false : m_method->HasResult(); }

                SlotId GetBusSlotId() const;

                void OnWriteEnd();

                void Visit(NodeVisitor& visitor) const override { visitor.Visit(*this); }

            protected:

                void InitializeMethod(AZ::BehaviorMethod& method, const AZStd::string* inputNameOverride = nullptr);
                void ConfigureMethod(AZ::BehaviorMethod& method);

                bool FindClass(AZ::BehaviorMethod*& outMethod, const Namespaces& namespaces, AZStd::string_view className, AZStd::string_view methodName);
                bool FindEvent(AZ::BehaviorMethod*& outMethod, const Namespaces& namespaces, AZStd::string_view busName, AZStd::string_view eventName);
                bool FindFree(AZ::BehaviorMethod*& outMethod, const Namespaces& namespaces, AZStd::string_view methodName);

                void InitializeLookUp(const Namespaces& namespaces, const AZStd::string& className, const AZStd::string& methodName, const MethodType methodType, const AZStd::string* nameOverride = nullptr);

                AZ_INLINE bool IsConfigured() const { return m_method != nullptr; }

                bool IsExpectingResult() const;

                void OnInputSignal(const SlotId&) override;

            private:
                friend struct ScriptCanvas::BehaviorContextMethodHelper;

                MethodType m_methodType = MethodType::Count;
                bool m_isOutcomeOutputMethod = false;
                AZStd::map<size_t, AZ::BehaviorMethod*> m_tupleGetMethods;
                AZStd::string m_name;
                AZStd::string m_methodName;
                AZStd::string m_className;
                AZStd::string m_classNamePretty;
                Namespaces m_namespaces;
                AZ::BehaviorMethod* m_method = nullptr;
                AZStd::vector<SlotId> m_resultSlotIDs;
                AZStd::recursive_mutex m_mutex; // post-serialization

                Method(const Method&) = delete;
            };

        } // namespace Core
    } // namespace Nodes
} // namespace ScriptCanvas