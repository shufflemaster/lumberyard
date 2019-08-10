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

#include "OperatorClear.h"

#include <ScriptCanvas/Core/Contracts/SupportsMethodContract.h>
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Core.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            void OperatorClear::ConfigureContracts(SourceType sourceType, AZStd::vector<ContractDescriptor>& contractDescs)
            {
                if (sourceType == SourceType::SourceInput)
                {
                    ContractDescriptor supportsMethodContract;
                    supportsMethodContract.m_createFunc = [this]() -> SupportsMethodContract* { return aznew SupportsMethodContract("Clear"); };
                    contractDescs.push_back(AZStd::move(supportsMethodContract));
                }
            }

            void OperatorClear::OnSourceTypeChanged()
            {
                m_outputSlots.insert(AddOutputTypeSlot("Container", "", GetSourceType(), Node::OutputStorage::Required, false));
            }

            void OperatorClear::InvokeOperator()
            {
                const SlotSet& slotSets = GetSourceSlots();

                if (!slotSets.empty())
                {
                    SlotId sourceSlotId = (*slotSets.begin());

                    if (const Datum* containerDatum = GetInput(sourceSlotId))
                    {
                        if (containerDatum && !containerDatum->Empty())
                        {
                            AZ::Outcome<Datum, AZStd::string> clearOutcome = BehaviorContextMethodHelper::CallMethodOnDatum(*containerDatum, "Clear");
                            if (!clearOutcome.IsSuccess())
                            {
                                SCRIPTCANVAS_REPORT_ERROR((*this), "Failed to call Clear on container: %s", clearOutcome.GetError().c_str());
                                return;
                            }

                            if (Data::IsVectorContainerType(containerDatum->GetType()))
                            {
                                PushOutput(clearOutcome.TakeValue(), *GetSlot(*m_outputSlots.begin()));
                            }

                            // Push the source container as an output to support chaining
                            PushOutput(*containerDatum, *GetSlot(*m_outputSlots.begin()));
                        }
                    }
                }

                SignalOutput(GetSlotId("Out"));
            }

            void OperatorClear::OnInputSignal(const SlotId& slotId)
            {
                const SlotId inSlotId = OperatorBaseProperty::GetInSlotId(this);
                if (slotId == inSlotId)
                {
                    InvokeOperator();
                }
            }
        }
    }
}

#include <Include/ScriptCanvas/Libraries/Operators/Containers/OperatorClear.generated.cpp>