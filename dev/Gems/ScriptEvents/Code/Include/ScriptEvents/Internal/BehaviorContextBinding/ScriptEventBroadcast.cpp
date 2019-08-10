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

#include "precompiled.h"

#include "ScriptEventBroadcast.h"
#include "ScriptEventsBindingBus.h"

#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Asset/AssetCommon.h>

#include <ScriptEvents/ScriptEvent.h>
#include <ScriptEvents/Internal/VersionedProperty.h>

namespace ScriptEvents
{
    ScriptEventBroadcast::ScriptEventBroadcast(AZ::BehaviorContext* behaviorContext, const ScriptEvent& definition, AZStd::string eventName)
        : AZ::BehaviorMethod(behaviorContext)
        , m_returnType(AZ::Uuid::CreateNull())
    {
        m_name = AZStd::move(eventName);

        const AZStd::string& busName = definition.GetName();
        m_busBindingId = AZ::Uuid::CreateName(busName.c_str());

        Method method;
        if (!definition.FindMethod(m_name, method))
        {
            AZ_Warning("Script Events", false, "Method %s was not in Script Event: %s", m_name.c_str(), m_name.c_str());
        }

        if (!method.GetReturnTypeProperty().IsEmpty())
        {
            method.GetReturnTypeProperty().Get(m_returnType);
            m_returnType = method.GetReturnType();
        }

        m_result.m_name = "Result";
        Internal::Utils::BehaviorParameterFromType(m_returnType, false, m_result);

        ReserveArguments(method.GetParameters().size() + 1);

        size_t index = 1;

        for (const Parameter& parameter : method.GetParameters())
        {
            const AZStd::string& argumentName = parameter.GetName();
            if (parameter.GetType().IsNull())
            {
                AZ_Warning("Script Events", false, "Argument type for parameter %s cannot be null", argumentName.c_str());
                continue;
            }

            SetArgumentName(index, argumentName);

            m_behaviorParameters.push_back();
            Internal::Utils::BehaviorParameterFromParameter(behaviorContext, parameter, m_argumentNames[index].c_str(), m_behaviorParameters.back());

            const AZStd::string& tooltip = parameter.GetTooltip();
            if (!tooltip.empty())
            {
                SetArgumentToolTip(index, tooltip.c_str());
            }

            ++index;
        }

        //AZ_TracePrintf("Script Events", "Script Broadcast Method: %s %s::%s (Arguments: %zu)\n", m_returnType.ToString<AZStd::string>().c_str(), busName.c_str(), m_name.c_str(), method.GetParameters().size());
    }

    bool ScriptEventBroadcast::Call(AZ::BehaviorValueParameter* params, unsigned int paramCount, AZ::BehaviorValueParameter* returnValue)
    {
        Internal::BindingRequest::BindingParameters parameters;
        parameters.m_eventName = m_name;
        parameters.m_address = nullptr;
        parameters.m_parameters = params;
        parameters.m_parameterCount = paramCount;
        parameters.m_returnValue = returnValue;

        Internal::BindingRequestBus::Event(m_busBindingId, &Internal::BindingRequest::Bind, parameters);
        
        if (returnValue && returnValue->m_onAssignedResult)
        {
            returnValue->m_onAssignedResult();
        }

        return true;
    }

    void ScriptEventBroadcast::ReserveArguments(size_t numArguments)
    {
        m_behaviorParameters.reserve(numArguments);
        m_argumentNames.resize(numArguments);
        m_argumentToolTips.resize(numArguments);
    }

    void ScriptEventBroadcast::SetArgumentName(size_t index, const AZStd::string& name)
    {
        if (index >= m_argumentNames.size())
        {
            m_argumentNames.resize(index + 1);
        }

        m_argumentNames[index] = name;
    }

    size_t ScriptEventBroadcast::GetMinNumberOfArguments() const
    {
        // Iterate from end of parameters and count the number of consecutive valid BehaviorValue objects
        size_t numDefaultArguments = 0;
        for (size_t i = GetNumArguments() - 1; i >= 0 && GetDefaultValue(i); --i, ++numDefaultArguments)
        {
        }
        return GetNumArguments() - numDefaultArguments;
    }

    AZ::BehaviorDefaultValuePtr ScriptEventBroadcast::GetDefaultValue(size_t) const
    {
        // Default values for Script Events are not implemented.
        return nullptr;
    }

}
