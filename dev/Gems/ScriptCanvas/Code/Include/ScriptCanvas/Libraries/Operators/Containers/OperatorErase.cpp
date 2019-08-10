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

#include "OperatorErase.h"

#include <ScriptCanvas/Core/Contracts/SupportsMethodContract.h>
#include <ScriptCanvas/Libraries/Core/MethodUtility.h>
#include <ScriptCanvas/Core/Core.h>

namespace ScriptCanvas
{
    namespace Nodes
    {
        namespace Operators
        {
            void OperatorErase::ConfigureContracts(SourceType sourceType, AZStd::vector<ContractDescriptor>& contractDescs)
            {
                if (sourceType == SourceType::SourceInput)
                {
                    ContractDescriptor supportsMethodContract;
                    supportsMethodContract.m_createFunc = [this]() -> SupportsMethodContract* { return aznew SupportsMethodContract("Erase"); };
                    contractDescs.push_back(AZStd::move(supportsMethodContract));
                }
            }

            void OperatorErase::OnSourceTypeChanged()
            {
                if (Data::IsVectorContainerType(GetSourceAZType()))
                {
                    // Add the INDEX as the INPUT slot
                    m_inputSlots.insert(AddInputDatumSlot("Index", "", Data::Type::Number(), Datum::eOriginality::Original, false));
                }
                else if (Data::IsMapContainerType(GetSourceAZType()))
                {
                    AZStd::vector<AZ::Uuid> types = ScriptCanvas::Data::GetContainedTypes(GetSourceAZType());

                    // Only add the KEY as INPUT slot
                    m_inputSlots.insert(AddInputDatumSlot(Data::GetName(Data::FromAZType(types[0])), "", Data::FromAZType(types[0]), Datum::eOriginality::Original, false));
                }
                
                m_outputSlots.insert(AddOutputTypeSlot(Data::GetName(GetSourceType()), "Container", GetSourceType(), Node::OutputStorage::Required, false));
            }

            void OperatorErase::InvokeOperator()
            {
                const SlotSet& slotSets = GetSourceSlots();

                if (!slotSets.empty())
                {
                    SlotId sourceSlotId = (*slotSets.begin());

                    if (const Datum* containerDatum = GetInput(sourceSlotId))
                    {
                        if (containerDatum && !containerDatum->Empty())
                        {
                            const Datum* inputKeyDatum = GetInput(*m_inputSlots.begin());
                            AZ::Outcome<Datum, AZStd::string> valueOutcome = BehaviorContextMethodHelper::CallMethodOnDatum(*containerDatum, "Erase", *inputKeyDatum);
                            if (!valueOutcome.IsSuccess())
                            {
                                SCRIPTCANVAS_REPORT_ERROR((*this), "Failed to get key in container: %s", valueOutcome.GetError().c_str());
                                return;
                            }

                            if (Data::IsVectorContainerType(containerDatum->GetType()))
                            {
                                PushOutput(valueOutcome.TakeValue(), *GetSlot(*m_outputSlots.begin()));
                            }
                            else if (Data::IsSetContainerType(containerDatum->GetType()) || Data::IsMapContainerType(containerDatum->GetType()))
                            {
                                Datum keyDatum = valueOutcome.TakeValue();
                                if (keyDatum.Empty())
                                {
                                    SCRIPTCANVAS_REPORT_ERROR((*this), "Behavior Context call failed; unable to retrieve element from container.");
                                    return;
                                }

                                PushOutput(keyDatum, *GetSlot(*m_outputSlots.begin()));
                            }

                            // Push the source container as an output to support chaining
                            PushOutput(*containerDatum, *GetSlot(*m_outputSlots.begin()));
                        }
                    }
                }

                SignalOutput(GetSlotId("Out"));
            }

            void OperatorErase::OnInputSignal(const SlotId& slotId)
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

#include <Include/ScriptCanvas/Libraries/Operators/Containers/OperatorErase.generated.cpp>