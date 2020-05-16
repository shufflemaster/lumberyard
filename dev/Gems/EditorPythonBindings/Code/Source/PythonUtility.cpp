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

#include <Source/PythonUtility.h>
#include <Source/PythonProxyObject.h>
#include <Source/PythonTypeCasters.h>
#include <Source/PythonMarshalComponent.h>

#include <pybind11/embed.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/Math/VectorFloat.h>
#include <AzFramework/StringFunc/StringFunc.h>

namespace EditorPythonBindings
{
    namespace Module
    {
        pybind11::module DeterminePackageModule(PackageMapType& modulePackageMap, AZStd::string_view moduleName, pybind11::module parentModule, pybind11::module fallbackModule, bool alertUsingFallback)
        {
            if (moduleName.empty() || !moduleName[0])
            {
                AZ_Warning("python", !alertUsingFallback, "Could not determine missing or empty module; using fallback module");
                return fallbackModule;
            }
            else if (parentModule.is_none())
            {
                AZ_Warning("python", !alertUsingFallback, "Could not determine using None parent module; using fallback module");
                return fallbackModule;
            }

            AZStd::string parentModuleName(PyModule_GetName(parentModule.ptr()));
            modulePackageMap[parentModuleName] = parentModule;
            pybind11::module currentModule = parentModule;

            AZStd::string fullModuleName(parentModuleName);
            fullModuleName.append(".");
            fullModuleName.append(moduleName);

            AZStd::vector<AZStd::string> moduleParts;
            AzFramework::StringFunc::Tokenize(fullModuleName.c_str(), moduleParts, ".", false, false);
            for (int modulePartsIndex = 0; modulePartsIndex < moduleParts.size(); ++modulePartsIndex)
            {
                AZStd::string currentModulePath;
                AzFramework::StringFunc::Join(currentModulePath, moduleParts.begin(), moduleParts.begin() + modulePartsIndex + 1, ".");

                auto itPackageEntry = modulePackageMap.find(currentModulePath.c_str());
                if (itPackageEntry != modulePackageMap.end())
                {
                    currentModule = itPackageEntry->second;
                }
                else
                {
                    PyObject* newModule = PyImport_AddModule(currentModulePath.c_str());
                    if (!newModule)
                    {
                        AZ_Warning("python", false, "Could not add module named %s; using fallback module", currentModulePath.c_str());
                        return fallbackModule;
                    }
                    else
                    {
                        auto newSubModule = pybind11::reinterpret_borrow<pybind11::module>(newModule);
                        modulePackageMap[currentModulePath] = newSubModule;
                        const char* subModuleName = moduleParts[modulePartsIndex].c_str();
                        currentModule.attr(subModuleName) = newSubModule;
                        currentModule = newSubModule;
                    }
                }
            }
            return currentModule;
        }
    }

    namespace Internal
    {
        // type checks
        bool IsPrimitiveType(const AZ::TypeId& typeId)
        {
            return (typeId == AZ::AzTypeInfo<bool>::Uuid()              ||
                    typeId == AZ::AzTypeInfo<char>::Uuid()              ||
                    typeId == AZ::AzTypeInfo<float>::Uuid()             ||
                    typeId == AZ::AzTypeInfo<double>::Uuid()            ||
                    typeId == AZ::AzTypeInfo<AZ::VectorFloat>::Uuid()   ||
                    typeId == AZ::AzTypeInfo<AZ::s8>::Uuid()            ||
                    typeId == AZ::AzTypeInfo<AZ::u8>::Uuid()            ||
                    typeId == AZ::AzTypeInfo<AZ::s16>::Uuid()           ||
                    typeId == AZ::AzTypeInfo<AZ::u16>::Uuid()           ||
                    typeId == AZ::AzTypeInfo<AZ::s32>::Uuid()           ||
                    typeId == AZ::AzTypeInfo<AZ::u32>::Uuid()           ||
                    typeId == AZ::AzTypeInfo<AZ::s64>::Uuid()           ||
                    typeId == AZ::AzTypeInfo<AZ::u64>::Uuid() );
        }

        bool IsPointerType(const AZ::u32 traits)
        {
            return (((traits & AZ::BehaviorParameter::TR_POINTER) == AZ::BehaviorParameter::TR_POINTER) ||
                    ((traits & AZ::BehaviorParameter::TR_REFERENCE) == AZ::BehaviorParameter::TR_REFERENCE));
        }

        // allocation patterns

        template <typename Allocator>
        bool AllocateBehaviorObjectByClass(const AZ::BehaviorClass* behaviorClass, Allocator& allocator, AZ::BehaviorObject& behaviorObject)
        {
            if (behaviorClass)
            {
                if (behaviorClass->m_defaultConstructor)
                {
                    behaviorObject.m_typeId = behaviorClass->m_typeId;
                    behaviorObject.m_address = allocator.allocate(behaviorClass->m_size, behaviorClass->m_alignment);
                    behaviorClass->m_defaultConstructor(behaviorObject.m_address, nullptr);
                    return true;
                }
                else
                {
                    AZ_Warning("python", behaviorClass->m_defaultConstructor, "Missing default constructor for AZ::BehaviorClass for typeId:%s", behaviorClass->m_name.c_str());
                }
            }
            return false;
        }

        template <typename Allocator>
        bool AllocateBehaviorObjectByTypeId(const AZ::TypeId& typeId, Allocator& allocator, AZ::BehaviorObject& behaviorObject)
        {
            const AZ::BehaviorClass* behaviorClass = AZ::BehaviorContextHelper::GetClass(typeId);
            AZ_Error("python", behaviorClass, "Missing AZ::BehaviorClass for typeId:%s", typeId.ToString<AZStd::string>().c_str());
            if (behaviorClass)
            {
                return AllocateBehaviorObjectByClass(AZ::BehaviorContextHelper::GetClass(typeId), allocator, behaviorObject);
            }
            return false;
        }

        template <typename T>
        bool AllocateBehaviorObjectByType(Convert::StackVariableAllocator& tempAllocator, AZ::BehaviorObject& behaviorObject)
        {
            return AllocateBehaviorObjectByTypeId(AZ::AzTypeInfo<T>::Uuid(), tempAllocator, behaviorObject);
        }
    
        bool AllocateBehaviorValueParameter(const AZ::BehaviorMethod* behaviorMethod, AZ::BehaviorValueParameter& result, Convert::StackVariableAllocator& stackVariableAllocator)
        {
            if (const AZ::BehaviorParameter* resultType = behaviorMethod->GetResult())
            {
                result.Set(*resultType);

                if (resultType->m_traits & AZ::BehaviorParameter::TR_POINTER)
                {
                    result.m_value = result.m_tempData.allocate(sizeof(intptr_t), alignof(intptr_t));
                    return true;
                }

                if (resultType->m_traits & AZ::BehaviorParameter::TR_REFERENCE)
                {
                    return true;
                }

                if (IsPrimitiveType(resultType->m_typeId))
                {
                    result.m_value = result.m_tempData.allocate(sizeof(intptr_t), alignof(intptr_t));
                    return true;
                }

                AZ::BehaviorContext* behaviorContext(nullptr);
                AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
                if (!behaviorContext)
                {
                    AZ_Assert(false, "A behavior context is required!");
                    return false;
                }

                const AZ::BehaviorClass* behaviorClass = AZ::BehaviorContextHelper::GetClass(behaviorContext, resultType->m_typeId);
                AZ_Error("python", behaviorClass, "A behavior class is required!");
                if (behaviorClass)
                {
                    AZ::BehaviorObject behaviorObject;
                    bool allocated = false;
                    if (behaviorClass->m_size < result.m_tempData.get_max_size())
                    {
                        allocated = AllocateBehaviorObjectByClass(behaviorClass, result.m_tempData, behaviorObject);
                    }
                    else
                    {
                        allocated = AllocateBehaviorObjectByClass(behaviorClass, stackVariableAllocator, behaviorObject);
                    }

                    if (allocated)
                    {
                        result.m_value = behaviorObject.m_address;
                        result.m_typeId = resultType->m_typeId;
                        return true;
                    }
                }
            }
            return false;
        }

        void DeallocateBehaviorValueParameter(AZ::BehaviorValueParameter& valueParameter)
        {
            if (IsPointerType(valueParameter.m_traits) || IsPrimitiveType(valueParameter.m_typeId))
            {
                // no constructor was used
                return;
            }

            AZ::BehaviorContext* behaviorContext(nullptr);
            AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
            if (!behaviorContext)
            {
                AZ_Assert(false, "A behavior context is required!");
            }

            const AZ::BehaviorClass* behaviorClass = AZ::BehaviorContextHelper::GetClass(behaviorContext, valueParameter.m_typeId);
            AZ_Error("python", behaviorClass, "Missing AZ::BehaviorClass for typeId:%s", valueParameter.m_typeId.ToString<AZStd::string>().c_str());
            if (behaviorClass)
            {
                if (behaviorClass->m_destructor)
                {
                    behaviorClass->m_destructor(valueParameter.m_value, nullptr);
                }
            }
        }
    }

    namespace Convert
    {
        // StackVariableAllocator

        StackVariableAllocator::~StackVariableAllocator()
        {
            for (auto& cleanUp : m_cleanUpItems)
            {
                cleanUp();
            }
        }
        
        void StackVariableAllocator::StoreVariableDeleter(VariableDeleter&& deleter)
        {
            m_cleanUpItems.emplace_back(deleter);
        }

        // Convert Python List For Primitive Type

        bool PythonProxyObjectToBehaviorValueParameter(const AZ::BehaviorParameter& behaviorArgument, pybind11::object pyObj, AZ::BehaviorValueParameter& parameter)
        {
            auto behaviorObject = pybind11::cast<EditorPythonBindings::PythonProxyObject*>(pyObj)->GetBehaviorObject();
            if (behaviorObject)
            {
                const AZ::BehaviorClass* behaviorClass = AZ::BehaviorContextHelper::GetClass(behaviorObject.value()->m_typeId);
                if (!behaviorClass)
                {
                    AZ_Warning("python", false, "Missing BehaviorClass for typeId %s", behaviorObject.value()->m_typeId.ToString<AZStd::string>().c_str());
                    return false;
                }

                if (behaviorClass->m_azRtti)
                {
                    // is exact type or can be down casted?
                    if (!behaviorClass->m_azRtti->IsTypeOf(behaviorArgument.m_typeId))
                    {
                        return false;
                    }
                }
                else if (behaviorObject.value()->m_typeId != behaviorArgument.m_typeId)
                {
                    // type mismatch detected
                    return false;
                }

                if ((behaviorArgument.m_traits & AZ::BehaviorParameter::TR_POINTER) == AZ::BehaviorParameter::TR_POINTER)
                {
                    parameter.m_value = &behaviorObject.value()->m_address;
                }
                else
                {
                    parameter.m_value = behaviorObject.value()->m_address;
                }
                parameter.m_typeId = behaviorClass->m_typeId;
                parameter.m_azRtti = behaviorClass->m_azRtti;
                parameter.m_traits = behaviorArgument.m_traits;
                parameter.m_name = behaviorArgument.m_name;
                return true;
            }
            return false;
        }

        bool PythonToBehaviorValueParameter(const AZ::BehaviorParameter& behaviorArgument, pybind11::object pyObj, AZ::BehaviorValueParameter& parameter, Convert::StackVariableAllocator& stackVariableAllocator)
        {
            AZStd::optional<PythonMarshalTypeRequests::BehaviorValueResult> result;
            PythonMarshalTypeRequests::BehaviorTraits traits = static_cast<PythonMarshalTypeRequests::BehaviorTraits>(behaviorArgument.m_traits);
            PythonMarshalTypeRequestBus::EventResult(result, behaviorArgument.m_typeId, &PythonMarshalTypeRequestBus::Events::PythonToBehaviorValueParameter, traits, pyObj, parameter);
            if (result && result.value().first)
            {
                auto deleter = AZStd::move(result.value().second);
                if (deleter)
                {
                    stackVariableAllocator.StoreVariableDeleter(AZStd::move(deleter));
                }
                parameter.m_typeId = behaviorArgument.m_typeId;
                parameter.m_traits = behaviorArgument.m_traits;
                parameter.m_name = behaviorArgument.m_name;
                parameter.m_azRtti = behaviorArgument.m_azRtti;
                return true;
            }
            else if (pybind11::isinstance<EditorPythonBindings::PythonProxyObject>(pyObj))
            {
                return PythonProxyObjectToBehaviorValueParameter(behaviorArgument, pyObj, parameter);
            }
            return false;
        }

        // from BehaviorValueParameter to Python

        pybind11::object BehaviorValueParameterToPython(AZ::BehaviorValueParameter& behaviorValue, Convert::StackVariableAllocator& stackVariableAllocator)
        {
            AZStd::optional<PythonMarshalTypeRequests::PythonValueResult> result;
            PythonMarshalTypeRequestBus::EventResult(result, behaviorValue.m_typeId, &PythonMarshalTypeRequestBus::Events::BehaviorValueParameterToPython, behaviorValue);
            if (result.has_value())
            {
                auto deleter = AZStd::move(result.value().second);
                if (deleter)
                {
                    stackVariableAllocator.StoreVariableDeleter(AZStd::move(deleter));
                }
                return result.value().first;
            }
            else if (behaviorValue.m_typeId != AZ::Uuid::CreateNull())
            {
                return PythonProxyObjectManagement::CreatePythonProxyObject(behaviorValue.m_typeId, behaviorValue.GetValueAddress());
            }
            AZ_Warning("python", false, "Cannot convert type %s", behaviorValue.m_name ? behaviorValue.m_name : behaviorValue.m_typeId.ToString<AZStd::string>().c_str());
            return pybind11::cast<pybind11::none>(Py_None);
        }

        AZStd::string GetPythonTypeName(pybind11::object pyObj)
        {
            if (pybind11::isinstance<PythonProxyObject>(pyObj))
            {
                return pybind11::cast<PythonProxyObject*>(pyObj)->GetWrappedTypeName();
            }
            return pybind11::cast<AZStd::string>(pybind11::str(pyObj.get_type()));
        }
    }

    namespace Call
    {
        constexpr size_t MaxBehaviorMethodArguments = 32;
        using BehaviorMethodArgumentArray = AZStd::array<AZ::BehaviorValueParameter, MaxBehaviorMethodArguments>;

        pybind11::object InvokeBehaviorMethodWithResult(AZ::BehaviorMethod* behaviorMethod, pybind11::args pythonInputArgs, AZ::BehaviorObject self, AZ::BehaviorValueParameter& result)
        {
            if (behaviorMethod->GetNumArguments() > MaxBehaviorMethodArguments || pythonInputArgs.size() > MaxBehaviorMethodArguments)
            {
                AZ_Error("python", false, "Too many arguments for class method; set:%zu max:%zu", behaviorMethod->GetMinNumberOfArguments(), MaxBehaviorMethodArguments);
                return pybind11::cast<pybind11::none>(Py_None);
            }

            Convert::StackVariableAllocator stackVariableAllocator;
            BehaviorMethodArgumentArray parameters;
            int parameterCount = 0;
            if (self.IsValid())
            {
                parameters[0].Set(&self);
                ++parameterCount;
            }

            // prepare the parameters for the BehaviorMethod
            for (auto pythonArg : pythonInputArgs)
            {
                if (parameterCount < behaviorMethod->GetNumArguments())
                {
                    auto currentPythonArg = pybind11::cast<pybind11::object>(pythonArg);
                    const AZ::BehaviorParameter* behaviorArgument = behaviorMethod->GetArgument(parameterCount);
                    if (!behaviorArgument)
                    {
                        AZ_Warning("python", false, "Missing argument at index %d in class method %s", parameterCount, behaviorMethod->m_name.c_str());
                        return pybind11::cast<pybind11::none>(Py_None);
                    }
                    if (!Convert::PythonToBehaviorValueParameter(*behaviorArgument, currentPythonArg, parameters[parameterCount], stackVariableAllocator))
                    {
                        AZ_Warning("python", false, "BehaviorMethod %s: Parameter at %d index expects %s for method %s but got type %s",
                            behaviorMethod->m_name.c_str(), parameterCount, behaviorArgument->m_name, behaviorMethod->m_name.c_str(), Convert::GetPythonTypeName(currentPythonArg).c_str());
                        return pybind11::cast<pybind11::none>(Py_None);
                    }
                    ++parameterCount;
                }
            }

            // did the Python script send the right amount of arguments?
            const auto totalPythonArgs = pythonInputArgs.size() + (self.IsValid() ? 1 : 0); // +1 for the 'this' coming in from a marshaled Python/BehaviorObject
            if (totalPythonArgs < behaviorMethod->GetMinNumberOfArguments())
            {
                AZ_Warning("python", false, "Method %s requires at least %zu parameters got %zu", behaviorMethod->m_name.c_str(), behaviorMethod->GetMinNumberOfArguments(), totalPythonArgs);
                return pybind11::cast<pybind11::none>(Py_None);
            }
            else if (totalPythonArgs > behaviorMethod->GetNumArguments())
            {
                AZ_Warning("python", false, "Method %s requires %zu parameters but it got more (%zu) - excess parameters will not be used.", behaviorMethod->m_name.c_str(), behaviorMethod->GetMinNumberOfArguments(), totalPythonArgs);
            }

            if (behaviorMethod->HasResult())
            {
                if (Internal::AllocateBehaviorValueParameter(behaviorMethod, result, stackVariableAllocator))
                {
                    if (behaviorMethod->Call(parameters.begin(), static_cast<unsigned int>(totalPythonArgs), &result))
                    {
                        return Convert::BehaviorValueParameterToPython(result, stackVariableAllocator);
                    }
                }
                AZ_Warning("python", false, "Failed to invoke class method");
            }
            else if (!behaviorMethod->Call(parameters.begin(), static_cast<unsigned int>(totalPythonArgs)))
            {
                AZ_Warning("python", false, "Failed to invoke class method %s", behaviorMethod->m_name.c_str());
            }
            return pybind11::cast<pybind11::none>(Py_None);
        }

        pybind11::object InvokeBehaviorMethod(AZ::BehaviorMethod* behaviorMethod, pybind11::args pythonInputArgs, AZ::BehaviorObject self)
        {
            AZ::BehaviorValueParameter result;
            result.m_value = nullptr;
            pybind11::object pythonOutput = InvokeBehaviorMethodWithResult(behaviorMethod, pythonInputArgs, self, result);
            if (result.m_value)
            {
                Internal::DeallocateBehaviorValueParameter(result);
            }
            return pythonOutput;
        }

        pybind11::object StaticMethod(AZ::BehaviorMethod* behaviorMethod, pybind11::args pythonInputArgs)
        {
            return InvokeBehaviorMethod(behaviorMethod, pythonInputArgs, {});
        }

        pybind11::object ClassMethod(AZ::BehaviorMethod* behaviorMethod, AZ::BehaviorObject self, pybind11::args pythonInputArgs)
        {
            if (behaviorMethod->GetNumArguments() == 0)
            {
                AZ_Error("python", false, "A member level function should require at least one argument");
            }
            else if (!self.IsValid())
            {
                AZ_Error("python", false, "Method %s requires at valid self object to invoke", behaviorMethod->m_name.c_str());
            }
            else
            {
                return InvokeBehaviorMethod(behaviorMethod, pythonInputArgs, self);
            }
            return pybind11::cast<pybind11::none>(Py_None);
        }
    }
}
