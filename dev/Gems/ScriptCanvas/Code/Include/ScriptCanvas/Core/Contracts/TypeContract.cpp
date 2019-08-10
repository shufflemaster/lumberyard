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

#include "TypeContract.h"
#include <ScriptCanvas/Core/ContractBus.h>
#include <ScriptCanvas/Core/NodeBus.h>
#include <ScriptCanvas/Core/Slot.h>

namespace ScriptCanvas
{
    void RestrictedTypeContract::Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<RestrictedTypeContract, Contract>()
                ->Version(1)
                ->Field("flags", &RestrictedTypeContract::m_flags)
                ->Field("types", &RestrictedTypeContract::m_types)
                ;
        }
    }

    AZ::Outcome<void, AZStd::string> RestrictedTypeContract::OnEvaluate(const Slot& sourceSlot, const Slot& targetSlot) const
    {
        bool valid = false;

        if (m_types.empty())
        {
            valid = m_flags != Exclusive;
        }
        else
        {
            if (m_flags == Inclusive)
            {
                for (const auto& type : m_types)
                {
                    if (targetSlot.IsTypeMatchFor(type))
                    {
                        valid = true;
                        break;
                    }
                }
            }
            else
            {
                valid = true;
                for (const auto& type : m_types)
                {
                    if (targetSlot.IsTypeMatchFor(type))
                    {
                        if (!targetSlot.IsDynamicSlot())
                        {
                            valid = false;
                            break;
                        }
                        else if (targetSlot.HasDisplayType())
                        {
                            valid = false;
                            break;
                        }
                    }
                }
            }
        }

        if (valid)
        {
            return AZ::Success();
        }

        AZStd::string errorMessage = AZStd::string::format("Connection cannot be created between source slot \"%s\" and target slot \"%s\" as the types do not satisfy the type requirement. (%s)\n\rValid types are:\n\r"
            , sourceSlot.GetName().data()
            , targetSlot.GetName().data()
            , RTTI_GetTypeName());

        for (const auto& type : m_types)
        {
            if (Data::IsValueType(type))
            {
                errorMessage.append(AZStd::string::format("%s\n", Data::GetName(type).data()));
            }
            else
            {
                errorMessage.append(Data::GetBehaviorClassName(type.GetAZType()));
            }
        }

        return AZ::Failure(errorMessage);
    }
}
