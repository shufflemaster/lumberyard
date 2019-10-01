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


#include "Datum.h"

#include <AzCore/RTTI/AttributeReader.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/RTTI/RTTI.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <AzCore/Serialization/IdUtils.h>
#include <ScriptCanvas/Data/DataRegistry.h>
#include <AzCore/Math/Transform.h>

#include "DatumBus.h"

namespace
{
    using namespace ScriptCanvas;
    
    template<typename t_Value>
    struct ImplicitConversionHelp
    {
        AZ_FORCE_INLINE bool Help(const Data::Type& sourceType, const void* source, const Data::Type& targetType, AZStd::any& target, const AZ::BehaviorClass* targetClass)
        {
            if (targetType.GetType() == Data::eType::Quaternion)
            {
                AZ_Assert(sourceType.GetType() == Data::eType::BehaviorContextObject, "No other types are currently implicitly convertible");
                Data::QuaternionType& targetRotation = AZStd::any_cast<Data::QuaternionType&>(target);
                AZ_Assert(sourceType.GetAZType() == azrtti_typeid<Data::QuaternionType>(), "Quaternion type not valid for ScriptCanvas conversion");
                targetRotation = *reinterpret_cast<const Data::QuaternionType*>(source);
            }
            else
            {
                AZ_Assert(targetType.GetType() == Data::eType::BehaviorContextObject, "No other types are currently implicitly convertible");
                AZ_Assert(targetClass, "Target class unknown, no conversion possible");
                const AZ::BehaviorClass& behaviorClass = *targetClass;
                AZ_Assert(behaviorClass.m_typeId == azrtti_typeid<Data::QuaternionType>(), "Quaternion type not valid for ScriptCanvas conversion");
                const Data::QuaternionType& sourceRotation = *reinterpret_cast<const Data::QuaternionType*>(source);
                target = BehaviorContextObject::Create<Data::QuaternionType>(sourceRotation, behaviorClass);
            }

            return true;
        }
    };

    template<typename t_Value>
    AZ_FORCE_INLINE bool ConvertImplicitlyCheckedGeneric(const Data::Type& sourceType, const void* source, const Data::Type& targetType, AZStd::any& target, const AZ::BehaviorClass* targetClass)
    {
        if (targetType.GetType() == Data::Traits<t_Value>::s_type)
        {
            AZ_Assert(sourceType.GetType() == Data::eType::BehaviorContextObject, "Conversion to %s requires one type to be a BehaviorContextObject", Data::Traits<t_Value>::GetName().c_str());
            t_Value& targetValue = AZStd::any_cast<t_Value&>(target);
            AZ_Assert(sourceType.GetAZType() == azrtti_typeid<t_Value>(), "Value type not valid for ScriptCanvas conversion to %s", Data::Traits<t_Value>::GetName().c_str());
            targetValue = *reinterpret_cast<const t_Value*>(source);
        }
        else
        {
            AZ_Assert(targetType.GetType() == Data::eType::BehaviorContextObject, "Conversion to %s requires one type to be a BehaviorContextObject", Data::Traits<t_Value>::GetName().c_str());
            AZ_Assert(targetClass, "Target class unknown, no conversion possible");
            const AZ::BehaviorClass& behaviorClass = *targetClass;
            AZ_Assert(behaviorClass.m_typeId == azrtti_typeid<t_Value>(), "Value type not valid for ScriptCanvas conversion to %s", Data::Traits<t_Value>::GetName().c_str());
            const t_Value& sourceValue = *reinterpret_cast<const t_Value*>(source);
            target = BehaviorContextObject::Create<t_Value>(sourceValue, behaviorClass);
        }

        return true;
    }

    AZ_FORCE_INLINE bool ConvertImplicitlyCheckedVector2(const void* source, const Data::Type& targetType, AZStd::any& target, const AZ::BehaviorClass* targetClass)
    {
        const AZ::Vector2& sourceVector = *reinterpret_cast<const AZ::Vector2*>(source);

        if (Data::IsVectorType(targetType))
        {
            switch (targetType.GetType())
            {
            case Data::eType::Vector2:
                AZStd::any_cast<AZ::Vector2&>(target) = sourceVector;
                break;

            case Data::eType::Vector3:
                AZStd::any_cast<AZ::Vector3&>(target).Set(sourceVector.GetX(), sourceVector.GetY(), AZ::VectorFloat::CreateZero());
                break;

            case Data::eType::Vector4:
                AZStd::any_cast<AZ::Vector4&>(target).Set(sourceVector.GetX(), sourceVector.GetY(), AZ::VectorFloat::CreateZero(), AZ::VectorFloat::CreateZero());
                break;

            default:
                AZ_Assert(false, "Vector type unaccounted for in ScriptCanvas data model");
                return false;
            }
        }
        else
        {
            AZ_Assert(targetType.GetType() == Data::eType::BehaviorContextObject, "No other types are currently implicitly convertible");
            AZ_Assert(targetClass, "Target class unknown, no conversion possible");

            const AZ::BehaviorClass& behaviorClass = *targetClass;
            const AZ::Uuid& typeID = behaviorClass.m_typeId;

            if (typeID == azrtti_typeid<AZ::Vector3>())
            {
                target = BehaviorContextObject::Create<AZ::Vector3>(AZ::Vector3(sourceVector.GetX(), sourceVector.GetY(), AZ::VectorFloat::CreateZero()), behaviorClass);
            }
            else if (typeID == azrtti_typeid<AZ::Vector2>())
            {
                target = BehaviorContextObject::Create<AZ::Vector2>(sourceVector, behaviorClass);
            }
            else if (typeID == azrtti_typeid<AZ::Vector4>())
            {
                target = BehaviorContextObject::Create<AZ::Vector4>(AZ::Vector4(sourceVector.GetX(), sourceVector.GetY(), AZ::VectorFloat::CreateZero(), AZ::VectorFloat::CreateZero()), behaviorClass);
            }
            else
            {
                AZ_Assert(false, "Vector type unaccounted for in ScriptCanvas data model");
                return false;
            }
        }

        return true;
    }
    
    AZ_FORCE_INLINE bool ConvertImplicitlyCheckedVector3(const void* source, const Data::Type& targetType, AZStd::any& target, const AZ::BehaviorClass* targetClass)
    {
        const AZ::Vector3& sourceVector = *reinterpret_cast<const AZ::Vector3*>(source);

        if (Data::IsVectorType(targetType))
        {
            switch (targetType.GetType())
            {
            case Data::eType::Vector2:
                AZStd::any_cast<AZ::Vector2&>(target).Set(sourceVector.GetX(), sourceVector.GetY());
                break;

            case Data::eType::Vector3:
                AZStd::any_cast<AZ::Vector3&>(target) = sourceVector;
                break;

            case Data::eType::Vector4:
                AZStd::any_cast<AZ::Vector4&>(target).Set(sourceVector, AZ::VectorFloat::CreateZero());
                break;

            default:
                AZ_Assert(false, "Vector type unaccounted for in ScriptCanvas data model");
                return false;
            }
        }
        else
        {
            AZ_Assert(targetType.GetType() == Data::eType::BehaviorContextObject, "No other types are currently implicitly convertible");
            AZ_Assert(targetClass, "Target class unknown, no conversion possible");

            const AZ::BehaviorClass& behaviorClass = *targetClass;
            const AZ::Uuid& typeID = behaviorClass.m_typeId;

            if (typeID == azrtti_typeid<AZ::Vector3>())
            {
                target = BehaviorContextObject::Create<AZ::Vector3>(sourceVector, behaviorClass);
            }
            else if (typeID == azrtti_typeid<AZ::Vector2>())
            {
                target = BehaviorContextObject::Create<AZ::Vector2>(AZ::Vector2(static_cast<float>(sourceVector.GetX()), static_cast<float>(sourceVector.GetY())), behaviorClass);
            }
            else if (typeID == azrtti_typeid<AZ::Vector4>())
            {
                target = BehaviorContextObject::Create<AZ::Vector4>(AZ::Vector4::CreateFromVector3(sourceVector), behaviorClass);
            }
            else
            {
                AZ_Assert(false, "Vector type unaccounted for in ScriptCanvas data model");
                return false;
            }
        }

        return true;
    }

    AZ_FORCE_INLINE bool ConvertImplicitlyCheckedVector4(const void* source, const Data::Type& targetType, AZStd::any& target, const AZ::BehaviorClass* targetClass)
    {
        const AZ::Vector4& sourceVector = *reinterpret_cast<const AZ::Vector4*>(source);

        if (Data::IsVectorType(targetType))
        {
            switch (targetType.GetType())
            {
            case Data::eType::Vector2:
                AZStd::any_cast<AZ::Vector2&>(target).Set(sourceVector.GetX(), sourceVector.GetY());
                break;

            case Data::eType::Vector3:
                AZStd::any_cast<AZ::Vector3&>(target) = sourceVector.GetAsVector3();
                break;

            case Data::eType::Vector4:
                AZStd::any_cast<AZ::Vector4&>(target) = sourceVector;
                break;

            default:
                AZ_Assert(false, "Vector type unaccounted for in ScriptCanvas data model");
                return false;
            }
        }
        else
        {
            AZ_Assert(targetType.GetType() == Data::eType::BehaviorContextObject, "No other types are currently implicitly convertible");
            AZ_Assert(targetClass, "Target class unknown, no conversion possible");

            const AZ::BehaviorClass& behaviorClass = *targetClass;
            const AZ::Uuid& typeID = behaviorClass.m_typeId;

            if (typeID == azrtti_typeid<AZ::Vector3>())
            {
                target = BehaviorContextObject::Create<AZ::Vector3>(sourceVector.GetAsVector3(), behaviorClass);
            }
            else if (typeID == azrtti_typeid<AZ::Vector2>())
            {
                target = BehaviorContextObject::Create<AZ::Vector2>(AZ::Vector2(static_cast<float>(sourceVector.GetX()), static_cast<float>(sourceVector.GetY())), behaviorClass);
            }
            else if (typeID == azrtti_typeid<AZ::Vector4>())
            {
                target = BehaviorContextObject::Create<AZ::Vector4>(sourceVector, behaviorClass);
            }
            else
            {
                AZ_Assert(false, "Vector type unaccounted for in ScriptCanvas data model");
                return false;
            }
        }

        return true;
    }

    AZ_FORCE_INLINE bool IsAnyVectorType(const Data::Type& type)
    {
        return type.GetType() == Data::eType::BehaviorContextObject
            ? Data::IsVectorType(type.GetAZType())
            : Data::IsVectorType(type);
    }
    
    AZ_FORCE_INLINE Data::eType GetVectorType(const Data::Type& type)
    {
        return type.GetType() == Data::eType::BehaviorContextObject 
            ? Data::FromAZType(type.GetAZType()).GetType()
            : type.GetType();
    }

    AZ_FORCE_INLINE bool ConvertImplicitlyCheckedVector(const Data::Type& sourceType, const void* source, const Data::Type& targetType, AZStd::any& target, const AZ::BehaviorClass* targetClass)
    {
        const Data::eType sourceVectorType = GetVectorType(sourceType);
        
        switch (sourceVectorType)
        {
        case Data::eType::Vector2:
            return ConvertImplicitlyCheckedVector2(source, targetType, target, targetClass);
        case Data::eType::Vector3:
            return ConvertImplicitlyCheckedVector3(source, targetType, target, targetClass);
        case Data::eType::Vector4:
            return ConvertImplicitlyCheckedVector4(source, targetType, target, targetClass);
        default:
            AZ_Assert(false, "non vector type in conversion");
            return false;
        }
    }

    AZ_FORCE_INLINE Data::eType GetMathConversionType(const Data::Type& a, const Data::Type& b)
    {
        AZ_Assert
        (
            (a.GetType() == Data::eType::BehaviorContextObject && Data::IsAutoBoxedType(b))
            || 
            (b.GetType() == Data::eType::BehaviorContextObject && Data::IsAutoBoxedType(a))
        , "these types are not convertible, or need no conversion.");

        return a.GetType() == Data::eType::BehaviorContextObject ? b.GetType() : a.GetType();
    }

    AZ_FORCE_INLINE bool ConvertImplicitlyChecked(const Data::Type& sourceType, const void* source, const Data::Type& targetType, AZStd::any& target, const AZ::BehaviorClass* targetClass)
    {
        AZ_Assert(!targetType.IS_A(sourceType), "Bad use of conversion, target type IS-A source type");
        
        if (IsAnyVectorType(sourceType) && IsAnyVectorType(targetType))
        {
            return ConvertImplicitlyCheckedVector(sourceType, source, targetType, target, targetClass);
        } 
        else if (Data::IsConvertible(sourceType, targetType))
        {
            auto conversionType = GetMathConversionType(targetType, sourceType);
            
            switch (conversionType)
            {
            case Data::eType::AABB:
                return ConvertImplicitlyCheckedGeneric<Data::AABBType>(sourceType, source, targetType, target, targetClass);
            case Data::eType::Color:
                return ConvertImplicitlyCheckedGeneric<Data::ColorType>(sourceType, source, targetType, target, targetClass);
            case Data::eType::CRC:
                return ConvertImplicitlyCheckedGeneric<Data::CRCType>(sourceType, source, targetType, target, targetClass);
            case Data::eType::Matrix3x3:
                return ConvertImplicitlyCheckedGeneric<Data::Matrix3x3Type>(sourceType, source, targetType, target, targetClass);
            case Data::eType::Matrix4x4:
                return ConvertImplicitlyCheckedGeneric<Data::Matrix4x4Type>(sourceType, source, targetType, target, targetClass);
            case Data::eType::OBB:
                return ConvertImplicitlyCheckedGeneric<Data::OBBType>(sourceType, source, targetType, target, targetClass);
            case Data::eType::Plane:
                return ConvertImplicitlyCheckedGeneric<Data::AABBType>(sourceType, source, targetType, target, targetClass);
            case Data::eType::Transform:
                return ConvertImplicitlyCheckedGeneric<Data::TransformType>(sourceType, source, targetType, target, targetClass);
            case Data::eType::Quaternion:
                return ConvertImplicitlyCheckedGeneric<Data::QuaternionType>(sourceType, source, targetType, target, targetClass);
            default:
                AZ_Assert(false, "unsupported convertible type added");
            }
        }

        return false;
    }
            
    template<typename t_Value>
    AZ_INLINE bool FromBehaviorContext(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        if (typeID == azrtti_typeid<t_Value>())
        {
            destination = *reinterpret_cast<const t_Value*>(source);
            return true;
        }
        else
        {
            AZ_Error("Script Canvas", false, "FromBehaviorContext generic failed on type match");
        }

        return false;
    }
    
    AZ_INLINE bool FromBehaviorContextAABB(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<Data::AABBType>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextBool(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<bool>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextColor(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<Data::ColorType>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextCRC(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<Data::CRCType>(typeID, source, destination);
    }
    
    AZ_INLINE bool FromBehaviorContextEntityID(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<AZ::EntityId>(typeID, source, destination);
    }

    template<typename t_Value>
    AZ_INLINE bool FromBehaviorContextNumeric(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        if (typeID == azrtti_typeid<t_Value>())
        {
            Data::NumberType number = aznumeric_caster(*reinterpret_cast<const t_Value*>(source));
            destination = number;
            return true;
        }

        return false;
    }

    // special cases NumberType, to prevent compiler errors on unecessary casting
    template<>
    AZ_INLINE bool FromBehaviorContextNumeric<Data::NumberType>(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        if (typeID == azrtti_typeid<Data::NumberType>())
        {
            destination = *reinterpret_cast<const Data::NumberType*>(source);
            return true;
        }

        return false;
    }

    // special cases conversion from vector float, which is non-trivial
    template<>
    AZ_INLINE bool FromBehaviorContextNumeric<AZ::VectorFloat>(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        if (typeID == azrtti_typeid<AZ::VectorFloat>())
        {
            float firstConversion;
            reinterpret_cast<const AZ::VectorFloat*>(source)->StoreToFloat(&firstConversion);
            Data::NumberType secondConversion = aznumeric_caster(firstConversion);
            destination = secondConversion;
            return true;
        }

        return false;
    }

    AZ_INLINE bool FromBehaviorContextNumber(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        AZ_Assert(source, "bad source in FromBehaviorContextNumber");
        return FromBehaviorContextNumeric<char>(typeID, source, destination)
            || FromBehaviorContextNumeric<short>(typeID, source, destination)
            || FromBehaviorContextNumeric<int>(typeID, source, destination)
            || FromBehaviorContextNumeric<long>(typeID, source, destination)
            || FromBehaviorContextNumeric<AZ::s8>(typeID, source, destination)
            || FromBehaviorContextNumeric<AZ::s64>(typeID, source, destination)
            || FromBehaviorContextNumeric<unsigned char>(typeID, source, destination)
            || FromBehaviorContextNumeric<unsigned int>(typeID, source, destination)
            || FromBehaviorContextNumeric<unsigned long>(typeID, source, destination)
            || FromBehaviorContextNumeric<unsigned short>(typeID, source, destination)
            || FromBehaviorContextNumeric<AZ::u64>(typeID, source, destination)
            || FromBehaviorContextNumeric<float>(typeID, source, destination)
            || FromBehaviorContextNumeric<double>(typeID, source, destination)
            || FromBehaviorContextNumeric<AZ::VectorFloat>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextMatrix3x3(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<AZ::Matrix3x3>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextMatrix4x4(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<AZ::Matrix4x4>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextOBB(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<Data::OBBType>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextPlane(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<Data::PlaneType>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextQuaternion(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<ScriptCanvas::Data::QuaternionType>(typeID, source, destination);
    }

    AZ_INLINE bool FromBehaviorContextTransform(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<ScriptCanvas::Data::TransformType>(typeID, source, destination);
    }
    
    AZ_INLINE bool FromBehaviorContextVector2(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        AZ::Vector2* target = AZStd::any_cast<AZ::Vector2>(&destination);
        AZ_Assert(source, "bad source in FromBehaviorContextVector");

        if (typeID == azrtti_typeid<AZ::Vector3>())
        {
            AZ::Vector3 sourceVector3(*reinterpret_cast<const AZ::Vector3*>(source));
            target->SetX(sourceVector3.GetX());
            target->SetY(sourceVector3.GetY());
            return true;
        }
        else if (typeID == azrtti_typeid<AZ::Vector2>())
        {
            *target = *reinterpret_cast<const AZ::Vector2*>(source);
            return true;
        }
        else if (typeID == azrtti_typeid<AZ::Vector4>())
        {
            AZ::Vector4 sourceVector4(*reinterpret_cast<const AZ::Vector4*>(source));
            target->SetX(sourceVector4.GetX());
            target->SetY(sourceVector4.GetY());
            return true;
        }
        else
        {
            return false;
        }
    }

    AZ_INLINE bool FromBehaviorContextVector3(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        AZ::Vector3* target = AZStd::any_cast<AZ::Vector3>(&destination);
        AZ_Assert(source, "bad source in FromBehaviorContextVector");

        if (typeID == azrtti_typeid<AZ::Vector3>())
        {
            *target = *reinterpret_cast<const AZ::Vector3*>(source);
            return true;
        }
        else if (typeID == azrtti_typeid<AZ::Vector2>())
        {
            auto vector2 = reinterpret_cast<const AZ::Vector2*>(source);
            target->Set(vector2->GetX(), vector2->GetY(), AZ::VectorFloat::CreateZero());
            return true;
        }
        else if (typeID == azrtti_typeid<AZ::Vector4>())
        {
            *target = reinterpret_cast<const AZ::Vector4*>(source)->GetAsVector3();
            return true;
        }
        else
        {
            return false;
        }
    }

    AZ_INLINE bool FromBehaviorContextVector4(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        AZ::Vector4* target = AZStd::any_cast<AZ::Vector4>(&destination);
        AZ_Assert(source, "bad source in FromBehaviorContextVector");

        if (typeID == azrtti_typeid<AZ::Vector3>())
        {
            *target = AZ::Vector4::CreateFromVector3(*reinterpret_cast<const AZ::Vector3*>(source));
            return true;
        }
        else if (typeID == azrtti_typeid<AZ::Vector2>())
        {
            auto vector2 = reinterpret_cast<const AZ::Vector2*>(source);
            target->Set(vector2->GetX(), vector2->GetY(), AZ::VectorFloat::CreateZero(), AZ::VectorFloat::CreateZero());
            return true;
        }
        else if (typeID == azrtti_typeid<AZ::Vector4>())
        {
            *target = *reinterpret_cast<const AZ::Vector4*>(source);
            return true;
        }
        else
        {
            return false;
        }
    }

    AZ_INLINE bool FromBehaviorContextString(const AZ::Uuid& typeID, const void* source, AZStd::any& destination)
    {
        return FromBehaviorContext<ScriptCanvas::Data::StringType>(typeID, source, destination);
    }
    
    template<typename t_Value>
    AZ_INLINE bool IsDataEqual(const void* lhs, const void* rhs)
    {
        return *reinterpret_cast<const t_Value*>(lhs) == *reinterpret_cast<const t_Value*>(rhs);
    }

    AZ_INLINE bool IsDataEqual(const Data::Type& type, const void* lhs, const void* rhs)
    {
        switch (type.GetType())
        {
        case Data::eType::AABB:
            return IsDataEqual<Data::AABBType>(lhs, rhs);

        case Data::eType::BehaviorContextObject:
            AZ_Error("ScriptCanvas", false, "BehaviorContextObject passed into IsDataEqual, which is invalid, an attempt must be made to call the behavior method");
            return false;

        case Data::eType::Boolean:
            return IsDataEqual<Data::BooleanType>(lhs, rhs);

        case Data::eType::Color:
            return IsDataEqual<Data::ColorType>(lhs, rhs);

        case Data::eType::CRC:
            return IsDataEqual<Data::CRCType>(lhs, rhs);

        case Data::eType::EntityID:
            return IsDataEqual<Data::EntityIDType>(lhs, rhs);

        case Data::eType::Invalid:
            return false;

        case Data::eType::Matrix3x3:
            return IsDataEqual<Data::Matrix3x3Type>(lhs, rhs);

        case Data::eType::Matrix4x4:
            return IsDataEqual<Data::Matrix4x4Type>(lhs, rhs);

        case Data::eType::Number:
            return IsDataEqual<Data::NumberType>(lhs, rhs);

        case Data::eType::OBB:
            return IsDataEqual<Data::OBBType>(lhs, rhs);

        case Data::eType::Plane:
            return IsDataEqual<Data::PlaneType>(lhs, rhs);

        case Data::eType::Quaternion:
            return IsDataEqual<Data::QuaternionType>(lhs, rhs);

        case Data::eType::String:
            return IsDataEqual<Data::StringType>(lhs, rhs);

        case Data::eType::Transform:
            return IsDataEqual<Data::TransformType>(lhs, rhs);

        case Data::eType::Vector2:
            return IsDataEqual<Data::Vector2Type>(lhs, rhs);

        case Data::eType::Vector3:
            return IsDataEqual<Data::Vector3Type>(lhs, rhs);

        case Data::eType::Vector4:
            return IsDataEqual<Data::Vector4Type>(lhs, rhs);

        default:
            AZ_Assert(false, "unsupported type found in IsDataEqual");
            return false;
        }
    }

    template<typename t_Value>
    AZ_INLINE bool IsDataLess(const void* lhs, const void* rhs)
    {
        return *reinterpret_cast<const t_Value*>(lhs) < *reinterpret_cast<const t_Value*>(rhs);
    }

    AZ_INLINE bool IsDataLess(const Data::Type& type, const void* lhs, const void* rhs)
    {
        switch (type.GetType())
        {
        case Data::eType::BehaviorContextObject:
            AZ_Error("ScriptCanvas", false, "BehaviorContextObject passed into IsDataLess, which is invalid, an attempt must be made to call the behavior method");
            return false;

        case Data::eType::Number:
            return IsDataLess<Data::NumberType>(lhs, rhs);

        case Data::eType::Vector2:
            return reinterpret_cast<const Data::Vector2Type*>(lhs)->IsLessThan(*reinterpret_cast<const Data::Vector2Type*>(rhs));

        case Data::eType::Vector3:
            return reinterpret_cast<const Data::Vector3Type*>(lhs)->IsLessThan(*reinterpret_cast<const Data::Vector3Type*>(rhs));

        case Data::eType::Vector4:
            return reinterpret_cast<const Data::Vector4Type*>(lhs)->IsLessThan(*reinterpret_cast<const Data::Vector4Type*>(rhs));

        case Data::eType::Boolean:
            return IsDataLess<Data::BooleanType>(lhs, rhs);

        case Data::eType::AABB:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::AABBType>::GetName().c_str());
            return false;

        case Data::eType::OBB:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::OBBType>::GetName().c_str());
            return false;

        case Data::eType::Plane:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::PlaneType>::GetName().c_str());
            return false;

        case Data::eType::Quaternion:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::QuaternionType>::GetName().c_str());
            return false;

        case Data::eType::String:
            return IsDataLess<Data::StringType>(lhs, rhs);

        case Data::eType::Transform:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::TransformType>::GetName().c_str());
            return false;

        case Data::eType::Color:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::ColorType>::GetName().c_str());
            return false;

        case Data::eType::CRC:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::CRCType>::GetName().c_str());
            return false;

        case Data::eType::EntityID:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::EntityIDType>::GetName().c_str());
            return false;

        case Data::eType::Matrix3x3:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::Matrix3x3Type>::GetName().c_str());
            return false;

        case Data::eType::Matrix4x4:
            AZ_Error("ScriptCanvas", false, "No Less operator exists for type: %s", Data::Traits<Data::Matrix4x4Type>::GetName().c_str());
            return false;

        case Data::eType::Invalid:
            return false;

        default:
            AZ_Assert(false, "unsupported type found in IsDataLess");
            return false;
        }
    }

    template<typename t_Value>
    AZ_INLINE bool IsDataLessEqual(const void* lhs, const void* rhs)
    {
        return *reinterpret_cast<const t_Value*>(lhs) <= *reinterpret_cast<const t_Value*>(rhs);
    }

    AZ_INLINE bool IsDataLessEqual(const Data::Type& type, const void* lhs, const void* rhs)
    {
        switch (type.GetType())
        {
        case Data::eType::BehaviorContextObject:
            AZ_Error("ScriptCanvas", false, "BehaviorContextObject passed into IsDataLessEqual, which is invalid, an attempt must be made to call the behavior method");
            return false;

        case Data::eType::Number:
            return IsDataLessEqual<Data::NumberType>(lhs, rhs);

        case Data::eType::Vector2:
            return reinterpret_cast<const Data::Vector2Type*>(lhs)->IsLessEqualThan(*reinterpret_cast<const Data::Vector2Type*>(rhs));

        case Data::eType::Vector3:
            return reinterpret_cast<const Data::Vector3Type*>(lhs)->IsLessEqualThan(*reinterpret_cast<const Data::Vector3Type*>(rhs));

        case Data::eType::Vector4:
            return reinterpret_cast<const Data::Vector4Type*>(lhs)->IsLessEqualThan(*reinterpret_cast<const Data::Vector4Type*>(rhs));

        case Data::eType::Boolean:
            return IsDataLessEqual<Data::BooleanType>(lhs, rhs);

        case Data::eType::AABB:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::AABBType>::GetName().c_str());
            return false;

        case Data::eType::OBB:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::OBBType>::GetName().c_str());
            return false;

        case Data::eType::Plane:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::PlaneType>::GetName().c_str());
            return false;

        case Data::eType::Quaternion:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::QuaternionType>::GetName().c_str());
            return false;

        case Data::eType::String:
            return IsDataLessEqual<Data::StringType>(lhs, rhs);

        case Data::eType::Transform:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::TransformType>::GetName().c_str());
            return false;

        case Data::eType::Color:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::ColorType>::GetName().c_str());
            return false;

        case Data::eType::CRC:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::CRCType>::GetName().c_str());
            return false;

        case Data::eType::EntityID:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::EntityIDType>::GetName().c_str());
            return false;

        case Data::eType::Matrix3x3:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::Matrix3x3Type>::GetName().c_str());
            return false;

        case Data::eType::Matrix4x4:
            AZ_Error("ScriptCanvas", false, "No LessEqual operator exists for type: %s", Data::Traits<Data::Matrix4x4Type>::GetName().c_str());
            return false;

        case Data::eType::Invalid:
            return false;

        default:
            AZ_Assert(false, "unsupported type found in IsDataLessEqual");
            return false;
        }
    }

    template<typename t_Value>
    AZ_INLINE bool IsDataGreater(const void* lhs, const void* rhs)
    {
        return *reinterpret_cast<const t_Value*>(lhs) > *reinterpret_cast<const t_Value*>(rhs);
    }

    AZ_INLINE bool IsDataGreater(const Data::Type& type, const void* lhs, const void* rhs)
    {
        switch (type.GetType())
        {
        case Data::eType::BehaviorContextObject:
            AZ_Error("ScriptCanvas", false, "BehaviorContextObject passed into IsDataGreater, which is invalid, an attempt must be made to call the behavior method");
            return false;

        case Data::eType::Number:
            return IsDataGreater<Data::NumberType>(lhs, rhs);

        case Data::eType::Vector2:
            return reinterpret_cast<const Data::Vector2Type*>(lhs)->IsGreaterThan(*reinterpret_cast<const Data::Vector2Type*>(rhs));

        case Data::eType::Vector3:
            return reinterpret_cast<const Data::Vector3Type*>(lhs)->IsGreaterThan(*reinterpret_cast<const Data::Vector3Type*>(rhs));

        case Data::eType::Vector4:
            return reinterpret_cast<const Data::Vector4Type*>(lhs)->IsGreaterThan(*reinterpret_cast<const Data::Vector4Type*>(rhs));

        case Data::eType::Boolean:
            return IsDataGreater<Data::BooleanType>(lhs, rhs);

        case Data::eType::AABB:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::AABBType>::GetName().c_str());
            return false;

        case Data::eType::OBB:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::OBBType>::GetName().c_str());
            return false;

        case Data::eType::Plane:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::PlaneType>::GetName().c_str());
            return false;

        case Data::eType::Quaternion:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::QuaternionType>::GetName().c_str());
            return false;

        case Data::eType::String:
            return IsDataGreater<Data::StringType>(lhs, rhs);

        case Data::eType::Transform:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::TransformType>::GetName().c_str());
            return false;

        case Data::eType::Color:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::ColorType>::GetName().c_str());
            return false;

        case Data::eType::CRC:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::CRCType>::GetName().c_str());
            return false;

        case Data::eType::EntityID:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::EntityIDType>::GetName().c_str());
            return false;

        case Data::eType::Matrix3x3:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::Matrix3x3Type>::GetName().c_str());
            return false;

        case Data::eType::Matrix4x4:
            AZ_Error("ScriptCanvas", false, "No Greater operator exists for type: %s", Data::Traits<Data::Matrix4x4Type>::GetName().c_str());
            return false;

        case Data::eType::Invalid:
            return false;

        default:
            AZ_Assert(false, "unsupported type found in IsDataGreater");
            return false;
        }
    }

    template<typename t_Value>
    AZ_INLINE bool IsDataGreaterEqual(const void* lhs, const void* rhs)
    {
        return *reinterpret_cast<const t_Value*>(lhs) >= *reinterpret_cast<const t_Value*>(rhs);
    }

    template<typename t_Value>
    AZ_INLINE bool IsDataNotEqual(const void* lhs, const void* rhs)
    {
        return *reinterpret_cast<const t_Value*>(lhs) != *reinterpret_cast<const t_Value*>(rhs);
    }

    AZ_INLINE bool IsDataNotEqual(const Data::Type& type, const void* lhs, const void* rhs)
    {
        switch (type.GetType())
        {
        case Data::eType::AABB:
            return IsDataNotEqual<Data::AABBType>(lhs, rhs);

        case Data::eType::BehaviorContextObject:
            return lhs != rhs;

        case Data::eType::Boolean:
            return IsDataNotEqual<Data::BooleanType>(lhs, rhs);

        case Data::eType::Color:
            return IsDataNotEqual<Data::ColorType>(lhs, rhs);

        case Data::eType::CRC:
            return IsDataNotEqual<Data::CRCType>(lhs, rhs);

        case Data::eType::EntityID:
            return IsDataNotEqual<Data::EntityIDType>(lhs, rhs);

        case Data::eType::Invalid:
            return false;

        case Data::eType::Matrix3x3:
            return IsDataNotEqual<Data::Matrix3x3Type>(lhs, rhs);

        case Data::eType::Matrix4x4:
            return IsDataNotEqual<Data::Matrix4x4Type>(lhs, rhs);

        case Data::eType::Number:
            return IsDataNotEqual<Data::NumberType>(lhs, rhs);

        case Data::eType::OBB:
            return IsDataNotEqual<Data::OBBType>(lhs, rhs);

        case Data::eType::Plane:
            return IsDataNotEqual<Data::PlaneType>(lhs, rhs);

        case Data::eType::Quaternion:
            return IsDataNotEqual<Data::QuaternionType>(lhs, rhs);

        case Data::eType::String:
            return IsDataNotEqual<Data::StringType>(lhs, rhs);

        case Data::eType::Transform:
            return IsDataNotEqual<Data::TransformType>(lhs, rhs);

        case Data::eType::Vector2:
            return IsDataNotEqual<Data::Vector2Type>(lhs, rhs);

        case Data::eType::Vector3:
            return IsDataNotEqual<Data::Vector3Type>(lhs, rhs);

        case Data::eType::Vector4:
            return IsDataNotEqual<Data::Vector4Type>(lhs, rhs);

        default:
            AZ_Assert(false, "unsupported type found in IsDataNotEqual");
            return false;
        }
    }
    template<typename t_Value>
    AZ_INLINE bool ToBehaviorContext(AZStd::any& valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        if (typeIDOut == azrtti_typeid<t_Value>())
        {
            valueOut = *reinterpret_cast<const t_Value*>(valueIn);
            return true;
        }

        return false;
    }

    template<typename t_Value>
    AZ_INLINE bool ToBehaviorContextNumeric(AZStd::any& valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        if (typeIDOut == azrtti_typeid<t_Value>())
        {
            const t_Value value = aznumeric_caster(*reinterpret_cast<const Data::NumberType*>(valueIn));
            valueOut = value;
            return true;
        }

        return false;
    }

    template<>
    AZ_INLINE bool ToBehaviorContextNumeric<Data::NumberType>(AZStd::any& valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<Data::NumberType>(valueOut, typeIDOut, valueIn);
    }

    template<>
    AZ_INLINE bool ToBehaviorContextNumeric<AZ::VectorFloat>(AZStd::any& valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        if (typeIDOut == azrtti_typeid<AZ::VectorFloat>())
        {
            float firstConversion = aznumeric_caster(*reinterpret_cast<const Data::NumberType*>(valueIn));
            valueOut = AZ::VectorFloat::CreateFromFloat(&firstConversion);
            return true;
        }

        return false;
    }

    AZ_INLINE bool ToBehaviorContextNumber(AZStd::any& valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return valueIn && (ToBehaviorContextNumeric<char>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<short>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<int>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<long>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::s8>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::s64>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned char>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned int>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned long>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned short>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::u64>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<float>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<double>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::VectorFloat>(valueOut, typeIDOut, valueIn));
    }

    template<typename t_Value>
    AZ_INLINE bool ToBehaviorContext(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        if (typeIDOut == azrtti_typeid<t_Value>())
        {
            *reinterpret_cast<t_Value*>(valueOut) = *reinterpret_cast<const t_Value*>(valueIn);
            return true;
        }

        return false;
    }

    AZ_INLINE bool ToBehaviorContextAABB(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<Data::AABBType>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextBool(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<bool>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextColor(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<Data::ColorType>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextCRC(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<Data::CRCType>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextEntityID(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<AZ::EntityId>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextMatrix3x3(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<AZ::Matrix3x3>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextMatrix4x4(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<AZ::Matrix4x4>(valueOut, typeIDOut, valueIn);
    }

    template<typename t_Value>
    AZ_INLINE bool ToBehaviorContextNumeric(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        if (typeIDOut == azrtti_typeid<t_Value>())
        {
            *reinterpret_cast<t_Value*>(valueOut) = aznumeric_caster(*reinterpret_cast<const Data::NumberType*>(valueIn));
            return true;
        }

        return false;
    }

    template<>
    AZ_INLINE bool ToBehaviorContextNumeric<Data::NumberType>(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<Data::NumberType>(valueOut, typeIDOut, valueIn);
    }

    template<>
    AZ_INLINE bool ToBehaviorContextNumeric<AZ::VectorFloat>(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        if (typeIDOut == azrtti_typeid<AZ::VectorFloat>())
        {
            float firstConversion = aznumeric_caster(*reinterpret_cast<const Data::NumberType*>(valueIn));
            *reinterpret_cast<AZ::VectorFloat*>(valueOut) = AZ::VectorFloat::CreateFromFloat(&firstConversion);
            return true;;
        }

        return false;
    }

    AZ_INLINE bool ToBehaviorContextNumber(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return valueIn && (ToBehaviorContextNumeric<char>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<short>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<int>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<long>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::s8>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::s64>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned char>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned int>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned long>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<unsigned short>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::u64>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<float>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<double>(valueOut, typeIDOut, valueIn)
            || ToBehaviorContextNumeric<AZ::VectorFloat>(valueOut, typeIDOut, valueIn));
    }

    AZ_INLINE bool ToBehaviorContextOBB(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<Data::OBBType>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextObject(const AZ::BehaviorClass* behaviorClass, void* valueOut, const void* valueIn)
    {
        if (behaviorClass && behaviorClass->m_cloner)
        {
            behaviorClass->m_cloner(valueOut, valueIn, nullptr);
            return true;
        }

        return false;
    }

    AZ_INLINE bool ToBehaviorContextPlane(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<Data::PlaneType>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextQuaternion(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<ScriptCanvas::Data::QuaternionType>(valueOut, typeIDOut, valueIn);
    }
    
    AZ_INLINE bool ToBehaviorContextString(AZ::BehaviorValueParameter& destination, const void* valueIn)
    {
        if (Data::IsString(destination.m_typeId))
        {
            return ToBehaviorContext<AZStd::string>(destination.GetValueAddress(), destination.m_typeId, valueIn);
        }
        else
        {
            auto stringValue = reinterpret_cast<const AZStd::string*>(valueIn);
            if (destination.m_typeId == azrtti_typeid<char>() && (destination.m_traits & (AZ::BehaviorParameter::TR_POINTER | AZ::BehaviorParameter::TR_CONST)))
            {
                *reinterpret_cast<const char**>(destination.m_value) = stringValue->c_str();
                return true;
            }
            else if (destination.m_typeId == azrtti_typeid<AZStd::string_view>())
            {
                AZStd::string_view stringView(*stringValue);
                *reinterpret_cast<AZStd::string_view*>(destination.GetValueAddress()) = stringView;
                return true;
            }
        }

        return false;
    }

    AZ_INLINE bool ToBehaviorContextTransform(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        return ToBehaviorContext<ScriptCanvas::Data::TransformType>(valueOut, typeIDOut, valueIn);
    }

    AZ_INLINE bool ToBehaviorContextVector2(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        const AZ::Vector2* vector2in(reinterpret_cast<const AZ::Vector2*>(valueIn));

        if (typeIDOut == azrtti_typeid<AZ::Vector3>())
        {
            AZ::Vector3* vector3out = reinterpret_cast<AZ::Vector3*>(valueOut);
            vector3out->SetX(vector2in->GetX());
            vector3out->SetY(vector2in->GetY());
            return true;
        }
        else if (typeIDOut == azrtti_typeid<AZ::Vector2>())
        {
            *reinterpret_cast<AZ::Vector2*>(valueOut) = *vector2in;
            return true;
        }
        else if (typeIDOut == azrtti_typeid<AZ::Vector4>())
        {
            AZ::Vector4* vector4out = reinterpret_cast<AZ::Vector4*>(valueOut);
            vector4out->SetX(vector2in->GetX());
            vector4out->SetY(vector2in->GetY());
            return true;
        }
        
        return false;
    }

    AZ_INLINE bool ToBehaviorContextVector3(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        const AZ::Vector3* vector3in(reinterpret_cast<const AZ::Vector3*>(valueIn));

        if (typeIDOut == azrtti_typeid<AZ::Vector3>())
        {
            *reinterpret_cast<AZ::Vector3*>(valueOut) = *vector3in;
            return true;
        }
        else if (typeIDOut == azrtti_typeid<AZ::Vector2>())
        {
            reinterpret_cast<AZ::Vector2*>(valueOut)->Set(vector3in->GetX(), vector3in->GetY());
            return true;
        }
        else if (typeIDOut == azrtti_typeid<AZ::Vector4>())
        {
            *reinterpret_cast<AZ::Vector4*>(valueOut) = AZ::Vector4::CreateFromVector3(*vector3in);
            return true;
        }
        
        return false;
    }

    AZ_INLINE bool ToBehaviorContextVector4(void* valueOut, const AZ::Uuid& typeIDOut, const void* valueIn)
    {
        const AZ::Vector4* vector4in(reinterpret_cast<const AZ::Vector4*>(valueIn));

        if (typeIDOut == azrtti_typeid<AZ::Vector3>())
        {
            *reinterpret_cast<AZ::Vector3*>(valueOut) = vector4in->GetAsVector3();
            return true;
        }
        else if (typeIDOut == azrtti_typeid<AZ::Vector2>())
        {
            reinterpret_cast<AZ::Vector2*>(valueOut)->Set(vector4in->GetX(), vector4in->GetY());
            return true;
        }
        else if (typeIDOut == azrtti_typeid<AZ::Vector4>())
        {
            *reinterpret_cast<AZ::Vector4*>(valueOut) = *vector4in;
            return true;
        }
        
        return false;
    }

    bool ToBehaviorContext(const ScriptCanvas::Data::Type& typeIn, const void* valueIn, AZ::BehaviorValueParameter& destination, AZ::BehaviorClass* behaviorClassOut)
    {
        const AZ::Uuid& typeIDOut = destination.m_typeId;
        void* valueOut = destination.GetValueAddress();
        
        if (valueIn)
        {
            switch (typeIn.GetType())
            {
            case ScriptCanvas::Data::eType::AABB:
                return ToBehaviorContextAABB(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::BehaviorContextObject:
                return ToBehaviorContextObject(behaviorClassOut, valueOut, valueIn);

            case ScriptCanvas::Data::eType::Boolean:
                return ToBehaviorContextBool(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Color:
                return ToBehaviorContextColor(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::CRC:
                return ToBehaviorContextCRC(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::EntityID:
                return ToBehaviorContextEntityID(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Matrix3x3:
                return ToBehaviorContextMatrix3x3(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Matrix4x4:
                return ToBehaviorContextMatrix4x4(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Number:
                return ToBehaviorContextNumber(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::OBB:
                return ToBehaviorContextOBB(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Plane:
                return ToBehaviorContextPlane(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Quaternion:
                return ToBehaviorContextQuaternion(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::String:
                return ToBehaviorContextString(destination, valueIn);

            case ScriptCanvas::Data::eType::Transform:
                return ToBehaviorContextTransform(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Vector2:
                return ToBehaviorContextVector2(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Vector3:
                return ToBehaviorContextVector3(valueOut, typeIDOut, valueIn);

            case ScriptCanvas::Data::eType::Vector4:
                return ToBehaviorContextVector4(valueOut, typeIDOut, valueIn);
            }
        }

        AZ_Error("Script Canvas", false, "invalid object going from Script Canvas!");
        return false;
    }

    AZ::BehaviorValueParameter ConvertibleToBehaviorValueParameter(const AZ::BehaviorParameter& description, const AZ::Uuid& azType, AZStd::string_view name, void* value, void*& pointer)
    {
        AZ_Assert(value, "value must be valid");
        AZ::BehaviorValueParameter parameter;
        parameter.m_typeId = description.m_typeId;
        parameter.m_name = name.data();
        
        if (description.m_traits & AZ::BehaviorParameter::TR_POINTER)
        {
            pointer = value;
            parameter.m_value = &pointer;
            parameter.m_traits = AZ::BehaviorParameter::TR_POINTER;
        }
        else
        {
            parameter.m_value = value;
            parameter.m_traits = 0;
        }

        return parameter;
    }

    AZ::Outcome<Data::StringType, AZStd::string> ConvertBehaviorContextString(const AZ::BehaviorParameter& parameterDesc, const void* source)
    {
        if (!source)
        {
            return AZ::Success(AZStd::string());
        }

        if (parameterDesc.m_typeId == azrtti_typeid<char>() && (parameterDesc.m_traits & (AZ::BehaviorParameter::TR_POINTER | AZ::BehaviorParameter::TR_CONST)))
        {
            AZStd::string_view parameterString = *reinterpret_cast<const char* const *>(source);
            return AZ::Success(AZStd::string(parameterString));
        }
        else if (parameterDesc.m_typeId == azrtti_typeid<AZStd::string_view>())
        {
            const AZStd::string_view* parameterString = nullptr;
            if (parameterDesc.m_traits & AZ::BehaviorParameter::TR_POINTER)
            {
                parameterString = *reinterpret_cast<const AZStd::string_view* const *>(source);
            }
            else
            {
                parameterString = reinterpret_cast<const AZStd::string_view*>(source);
            }

            if (parameterString)
            {
                return AZ::Success(AZStd::string(*parameterString));
            }
        }
        return AZ::Failure(AZStd::string("Cannot convert BehaviorContext String type to Script Canvas String"));
    }
} // namespace anonymous

namespace ScriptCanvas
{
    Datum::Datum()
        : m_isOverloadedStorage(true)
        , m_originality(eOriginality::Copy)
        , m_isDefaultConstructed(true)
    {
    }

    Datum::Datum(const Datum& datum)
        : m_isOverloadedStorage(true)
    {
        *this = datum;
        const_cast<bool&>(m_isOverloadedStorage) = datum.m_isOverloadedStorage;
    }

    Datum::Datum(Datum&& datum)
        : m_isOverloadedStorage(true)
    {
        *this = AZStd::move(datum);
        const_cast<bool&>(m_isOverloadedStorage) = AZStd::move(datum.m_isOverloadedStorage);
    }

    Datum::Datum(const Data::Type& type, eOriginality originality)
        : Datum(type, originality, nullptr, AZ::Uuid::CreateNull())
    {
    }

    Datum::Datum(const Data::Type& type, eOriginality originality, const void* source, const AZ::Uuid& sourceTypeID)
    {
        Initialize(type, originality, source, sourceTypeID);
    }
        
    Datum::Datum(const AZStd::string& behaviorClassName, eOriginality originality)
        : Datum(Data::FromAZType(AZ::BehaviorContextHelper::GetClassType(behaviorClassName)), originality, nullptr, AZ::Uuid::CreateNull())
    {
    }

    Datum::Datum(const AZ::BehaviorParameter& parameterDesc, eOriginality originality, const void* source)
    {
        InitializeBehaviorContextParameter(parameterDesc, originality, source);
    }

    Datum::Datum(BehaviorContextResultTag, const AZ::BehaviorParameter& resultType)
    {
        InitializeBehaviorContextMethodResult(resultType);
    }

    Datum::Datum(const AZ::BehaviorValueParameter& value)
        : Datum(value,
            !(value.m_traits & (AZ::BehaviorParameter::TR_POINTER | AZ::BehaviorParameter::TR_REFERENCE)) ? eOriginality::Original : eOriginality::Copy,
            value.m_value)
    {
    }

    void Datum::ReconfigureDatumTo(Datum&& datum)        
    {
        bool isOverloadedStorage = datum.m_isOverloadedStorage;

        const_cast<bool&>(m_isOverloadedStorage) = true;
        (*this) = AZStd::move(datum);
        const_cast<bool&>(m_isOverloadedStorage) = isOverloadedStorage;
    }

    void Datum::ReconfigureDatumTo(const Datum& datum)
    {
        bool isOverloadedStorage = datum.m_isOverloadedStorage;

        const_cast<bool&>(m_isOverloadedStorage) = true;
        (*this) = datum;
        const_cast<bool&>(m_isOverloadedStorage) = isOverloadedStorage;
    }

    ComparisonOutcome Datum::CallComparisonOperator(AZ::Script::Attributes::OperatorType operatorType, const AZ::BehaviorClass& behaviorClass, const Datum& lhs, const Datum& rhs)
    {
        // depending on when this gets called, check for null operands, they could be possible
        for (auto& methodIt : behaviorClass.m_methods)
        {
            auto* method = methodIt.second;

            if (AZ::Attribute* operatorAttr = AZ::FindAttribute(AZ::Script::Attributes::Operator, method->m_attributes))
            {
                AZ::AttributeReader operatorAttrReader(nullptr, operatorAttr);
                AZ::Script::Attributes::OperatorType methodAttribute{};
                
                if (operatorAttrReader.Read<AZ::Script::Attributes::OperatorType>(methodAttribute)
                    && methodAttribute == operatorType
                    && method->HasResult()
                    && method->GetResult()->m_typeId == azrtti_typeid<bool>()
                    && method->GetNumArguments() == 2)
                {
                    bool comparisonResult{};
                    AZ::BehaviorValueParameter result(&comparisonResult);
                    AZStd::array<AZ::BehaviorValueParameter, 2> params;
                    AZ::Outcome<AZ::BehaviorValueParameter, AZStd::string> lhsArgument = lhs.ToBehaviorValueParameter(*method->GetArgument(0));
                    
                    if (lhsArgument.IsSuccess() && lhsArgument.GetValue().m_value)
                    {
                        params[0].Set(lhsArgument.GetValue());
                        AZ::Outcome<AZ::BehaviorValueParameter, AZStd::string> rhsArgument = rhs.ToBehaviorValueParameter(*method->GetArgument(1));
                        
                        if (rhsArgument.IsSuccess() && rhsArgument.GetValue().m_value)
                        {
                            params[1].Set(rhsArgument.GetValue());
                            
                            if (method->Call(params.data(), aznumeric_caster(params.size()), &result))
                            {
                                return AZ::Success(comparisonResult);
                            }
                        }
                    }
                }
            }
        }

        return AZ::Failure(AZStd::string("Invalid Comparison Operator Method"));
    }

    void Datum::Clear()
    {
        m_storage.clear();
        m_class = nullptr;
        m_type = Data::Type::Invalid();
    }

    void Datum::ConvertBehaviorContextMethodResult(const AZ::BehaviorParameter& resultType)
    {
        if (IS_A(Data::Type::Number()))
        {
            if (resultType.m_traits & AZ::BehaviorParameter::TR_POINTER)
            {
                if (m_pointer)
                {
                    ::FromBehaviorContextNumber(resultType.m_typeId, m_pointer, m_storage);
                }
            }
            else
            {
                ::FromBehaviorContextNumber(resultType.m_typeId, reinterpret_cast<const void*>(&m_conversionStorage), m_storage);
            }
        }
        else if (IS_A(Data::Type::String()) && !Data::IsString(resultType.m_typeId) && AZ::BehaviorContextHelper::IsStringParameter(resultType))
        {
            void* storageAddress = (resultType.m_traits & AZ::BehaviorParameter::TR_POINTER) ? reinterpret_cast<void*>(&m_pointer) : AZStd::any_cast<void>(&m_conversionStorage);
            if (auto stringOutcome = ConvertBehaviorContextString(resultType, storageAddress))
            {
                m_storage = stringOutcome.GetValue();
            }
        }
        else if (m_type.GetType() == Data::eType::BehaviorContextObject
            && (resultType.m_traits & (AZ::BehaviorParameter::TR_POINTER | AZ::BehaviorParameter::TR_REFERENCE)))
        {
            if (m_pointer)
            {
                m_storage = BehaviorContextObject::CreateReference(resultType.m_typeId, m_pointer);
            }
        }
    }
    
    bool Datum::FromBehaviorContext(const void* source, const AZ::Uuid& typeID)
    {
        // \todo support polymorphism
        const auto& type = Data::FromAZType(typeID);
        InitializeOverloadedStorage(type, eOriginality::Copy);
        
        if (IS_A(type))
        {
            switch (m_type.GetType())
            {
            case ScriptCanvas::Data::eType::AABB:
                return FromBehaviorContextAABB(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::BehaviorContextObject:
                return FromBehaviorContextObject(m_class, source);
            
            case ScriptCanvas::Data::eType::Boolean:
                return FromBehaviorContextBool(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Color:
                return FromBehaviorContextColor(typeID, source, m_storage);
            
            case ScriptCanvas::Data::eType::CRC:
                return FromBehaviorContextCRC(typeID, source, m_storage);
                
            case ScriptCanvas::Data::eType::EntityID:
                return FromBehaviorContextEntityID(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Matrix3x3:
                return FromBehaviorContextMatrix3x3(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Matrix4x4:
                return FromBehaviorContextMatrix4x4(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Number:
                return ::FromBehaviorContextNumber(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::OBB:
                return FromBehaviorContextOBB(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Plane:
                return FromBehaviorContextPlane(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Quaternion:
                return FromBehaviorContextQuaternion(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::String:
                return FromBehaviorContextString(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Transform:
                return FromBehaviorContextTransform(typeID, source, m_storage);

            case ScriptCanvas::Data::eType::Vector2:
                return ::FromBehaviorContextVector2(typeID, source, m_storage);
            
            case ScriptCanvas::Data::eType::Vector3:
                return ::FromBehaviorContextVector3(typeID, source, m_storage);
            
            case ScriptCanvas::Data::eType::Vector4:
                return ::FromBehaviorContextVector4(typeID, source, m_storage);
            }
        }
        else if (ConvertImplicitlyChecked(type, source, m_type, m_storage, m_class))
        {
            return true;
        }

        AZ_Error("Script Canvas", false, "Invalid type has come into a Script Canvas node");
        return false;
    }

    bool Datum::FromBehaviorContext(const void* source)
    {
        return FromBehaviorContextObject(m_class, source);
    }

    bool Datum::FromBehaviorContextNumber(const void* source, const AZ::Uuid& typeID)
    {
        return ::FromBehaviorContextNumber(typeID, source, m_storage);
    }

    bool Datum::FromBehaviorContextObject(const AZ::BehaviorClass* behaviorClass, const void* source)
    {
        if (behaviorClass)
        {
            m_storage = BehaviorContextObject::CreateReference(behaviorClass->m_typeId, const_cast<void*>(source));
            return true;
        }

        return false;
    }

    const void* Datum::GetValueAddress() const
    {
        return m_type.GetType() != Data::eType::BehaviorContextObject 
             ? AZStd::any_cast<void>(&m_storage) 
             : (*AZStd::any_cast<BehaviorContextObjectPtr>(&m_storage))->Get();
    }

    bool Datum::Initialize(const Data::Type& type, eOriginality originality, const void* source, const AZ::Uuid& sourceTypeID)
    {
        if (m_isOverloadedStorage)
        {
            Clear();
        }

        AZ_Error("ScriptCanvas", Empty(), "double initialized datum");

        m_type = type;
       
        switch (type.GetType())
        {
        case ScriptCanvas::Data::eType::AABB:
            return InitializeAABB(source);

        case ScriptCanvas::Data::eType::BehaviorContextObject:
            return InitializeBehaviorContextObject(originality, source);

        case ScriptCanvas::Data::eType::Boolean:
            return InitializeBool(source);

        case ScriptCanvas::Data::eType::Color:
            return InitializeColor(source);

        case ScriptCanvas::Data::eType::CRC:
            return InitializeCRC(source);

        case ScriptCanvas::Data::eType::EntityID:
            return InitializeEntityID(source);

        case ScriptCanvas::Data::eType::NamedEntityID:
            return InitializeNamedEntityID(source);

        case ScriptCanvas::Data::eType::Matrix3x3:
            return InitializeMatrix3x3(source);

        case ScriptCanvas::Data::eType::Matrix4x4:
            return InitializeMatrix4x4(source);

        case ScriptCanvas::Data::eType::Number:
            return InitializeNumber(source, sourceTypeID);

        case ScriptCanvas::Data::eType::OBB:
            return InitializeOBB(source);

        case ScriptCanvas::Data::eType::Plane:
            return InitializePlane(source);

        case ScriptCanvas::Data::eType::Quaternion:
            return InitializeQuaternion(source);
        
        case ScriptCanvas::Data::eType::String:
            return InitializeString(source, sourceTypeID);
        
        case ScriptCanvas::Data::eType::Transform:
            return InitializeTransform(source);

        case ScriptCanvas::Data::eType::Vector2:
            return InitializeVector2(source, sourceTypeID);

        case ScriptCanvas::Data::eType::Vector3:
            return InitializeVector3(source, sourceTypeID);

        case ScriptCanvas::Data::eType::Vector4:
            return InitializeVector4(source, sourceTypeID);

        default:
            AZ_Error("Script Canvas", false, "Invalid datum type found datum initialization");
        }

        return false;
    }

    bool Datum::InitializeAABB(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::AABBType*>(source) : Data::Traits<Data::AABBType>::GetDefault();
        return true;
    }

    bool Datum::InitializeBehaviorContextParameter(const AZ::BehaviorParameter& parameterDesc, eOriginality originality, const void* source)
    {
        if (AZ::BehaviorContextHelper::IsStringParameter(parameterDesc))
        {
            auto convertOutcome = ConvertBehaviorContextString(parameterDesc, source);
            if (convertOutcome)
            {
                m_type = Data::Type::String();
                return InitializeString(&convertOutcome.GetValue(), azrtti_typeid<Data::StringType>());
            }
        }

        const auto& type = Data::FromAZType(parameterDesc.m_typeId);

        return Initialize(type, originality, source, parameterDesc.m_typeId);
    }

    bool Datum::InitializeBehaviorContextMethodResult(const AZ::BehaviorParameter& description)
    {
        if (AZ::BehaviorContextHelper::IsStringParameter(description))
        {
            auto convertOutcome = ConvertBehaviorContextString(description, nullptr);
            if (convertOutcome)
            {
                m_type = Data::Type::String();
                return InitializeString(&convertOutcome.GetValue(), azrtti_typeid<Data::StringType>());
            }
        }

        const auto& type = Data::FromAZType(description.m_typeId);
        const eOriginality originality 
            = !(description.m_traits & (AZ::BehaviorParameter::TR_POINTER | AZ::BehaviorParameter::TR_REFERENCE))
            ? eOriginality::Original
            : eOriginality::Copy;

        AZ_VerifyError("ScriptCavas", Initialize(type, originality, nullptr, AZ::Uuid::CreateNull()), "Initialization of BehaviorContext Method result failed");
        return true;
    }

    bool Datum::InitializeBehaviorContextObject(eOriginality originality, const void* source)
    {
        AZ_STATIC_ASSERT(sizeof(BehaviorContextObjectPtr) <= AZStd::Internal::ANY_SBO_BUF_SIZE, "BehaviorContextObjectPtr doesn't fit in generic Datum storage");
        AZ::BehaviorContext* behaviorContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
        AZ_Assert(behaviorContext, "Script Canvas can't do anything without a behavior context!");
        AZ_Assert(!IsValueType(m_type), "Can't initialize value types as objects!");
        const auto azType = m_type.GetAZType();

        auto classIter = behaviorContext->m_typeToClassMap.find(azType);
        if (classIter != behaviorContext->m_typeToClassMap.end())
        {
            const AZ::BehaviorClass& behaviorClass = *classIter->second;
            m_class = &behaviorClass;
            m_originality = originality;

            if (m_originality == eOriginality::Original)
            {
                m_storage = BehaviorContextObject::Create(behaviorClass, source);
            }
            else
            {
                m_storage = BehaviorContextObject::CreateReference(behaviorClass.m_typeId, const_cast<void*>(source));
            }

            return true;
        }

        return false;
    }

    bool Datum::InitializeBool(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::BooleanType*>(source) : Data::Traits<Data::BooleanType>::GetDefault();
        return true;
    }

    bool Datum::InitializeColor(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::ColorType*>(source) : Data::Traits<Data::ColorType>::GetDefault();
        return true;
    }

    bool Datum::InitializeCRC(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::CRCType*>(source) : Data::Traits<Data::CRCType>::GetDefault();
        return true;
    }

    bool Datum::InitializeEntityID(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::EntityIDType*>(source) : Data::Traits<Data::EntityIDType>::GetDefault();
        return true;
    }

    bool Datum::InitializeNamedEntityID(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::NamedEntityIDType*>(source) : Data::Traits<Data::NamedEntityIDType>::GetDefault();
        return true;
    }

    bool Datum::InitializeMatrix3x3(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::Matrix3x3Type*>(source) : Data::Traits<Data::Matrix3x3Type>::GetDefault();
        return true;
    }

    bool Datum::InitializeMatrix4x4(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::Matrix4x4Type*>(source) : Data::Traits<Data::Matrix4x4Type>::GetDefault();
        return true;
    }

    bool Datum::InitializeNumber(const void* source, const AZ::Uuid& sourceTypeID)
    {
        m_storage = Data::Traits<Data::NumberType>::GetDefault();
        return (source && ::FromBehaviorContextNumber(sourceTypeID, source, m_storage)) || true;
    }

    bool Datum::InitializeOBB(const void* source)
    {
        m_storage = source 
            ? *reinterpret_cast<const Data::OBBType*>(source) 
            : Data::Traits<Data::OBBType>::GetDefault();
        return true;
    }

    bool Datum::InitializePlane(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::PlaneType*>(source) : Data::Traits<Data::PlaneType>::GetDefault();
        return true;
    }
    bool Datum::InitializeQuaternion(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::QuaternionType*>(source) : Data::Traits<Data::QuaternionType>::GetDefault();
        return true;
    }

    bool Datum::InitializeString(const void* source, const AZ::Uuid& sourceTypeID)
    {
        if (source)
        {
            if (sourceTypeID == azrtti_typeid<AZStd::string_view>())
            {
                m_storage = Data::StringType(*reinterpret_cast<const AZStd::string_view*>(source));
            }
            else if (sourceTypeID == azrtti_typeid<char>())
            {
                m_storage = Data::StringType(reinterpret_cast<const char*>(source));
            }
            else
            {
                m_storage = *reinterpret_cast<const Data::StringType*>(source);
            }
        }
        else
        {
            m_storage = Data::Traits<Data::StringType>::GetDefault();
        }
        return true;
    }

    bool Datum::InitializeTransform(const void* source)
    {
        m_storage = source ? *reinterpret_cast<const Data::TransformType*>(source) : Data::Traits<Data::TransformType>::GetDefault();
        return true;
    }

    bool Datum::InitializeVector2(const void* source, const AZ::Uuid& sourceTypeID)
    {
        m_storage = Data::Traits<Data::Vector2Type>::GetDefault();
        // return a success regardless, but do the initialization first if source is not null
        return (source && FromBehaviorContextVector2(sourceTypeID, source, m_storage)) || true;
    }

    bool Datum::InitializeVector3(const void* source, const AZ::Uuid& sourceTypeID)
    {
        m_storage = Data::Traits<Data::Vector3Type>::GetDefault();
        // return a success regardless, but do the initialization first if source is not null
        return (source && FromBehaviorContextVector3(sourceTypeID, source, m_storage)) || true;
    }

    bool Datum::InitializeVector4(const void* source, const AZ::Uuid& sourceTypeID)
    {
        m_storage = Data::Traits<Data::Vector4Type>::GetDefault();
        // return a success regardless, but do the initialization first if source is not null
        return (source && FromBehaviorContextVector4(sourceTypeID, source, m_storage)) || true;
    }

    void Datum::SetType(const Data::Type& dataType)
    {
        if (!GetType().IsValid() && m_isDefaultConstructed)
        {
            if (dataType.IsValid())
            {
                m_isDefaultConstructed = false;
                                
                Datum tempDatum(dataType, ScriptCanvas::Datum::eOriginality::Original);
                ReconfigureDatumTo(AZStd::move(tempDatum));
            }
            else
            {
                m_isDefaultConstructed = true;
                (*this) = AZStd::move(Datum());                
            }
        }
    }

    bool Datum::IsConvertibleTo(const AZ::BehaviorParameter& parameterDesc) const
    {
        if (AZ::BehaviorContextHelper::IsStringParameter(parameterDesc) && Data::IsString(GetType()))
        {
            return true;
        }

        return IsConvertibleTo(Data::FromAZType(parameterDesc.m_typeId));
    }

    bool Datum::IsDefaultValue() const
    {
        auto& typeIdTraitMap = GetDataRegistry()->m_typeIdTraitMap;
        auto dataTraitIt = typeIdTraitMap.find(m_type.GetType());
        if (dataTraitIt != typeIdTraitMap.end())
        {
            return dataTraitIt->second.m_dataTraits.IsDefault(m_storage, m_type);
        }

        AZ_Error("Script Canvas", m_isOverloadedStorage, "Unsupported ScriptCanvas Data type");
        return true;
    }
         
    void* Datum::ModResultAddress()
    {
        return m_type.GetType() != Data::eType::BehaviorContextObject
            ? AZStd::any_cast<void>(&m_storage)
            : (*AZStd::any_cast<BehaviorContextObjectPtr>(&m_storage))->Mod();
    }

    void* Datum::ModValueAddress() const
    {
        return const_cast<void*>(GetValueAddress());
    }

    Datum& Datum::operator=(Datum&& source)
    {
        if (this != &source)
        {
            if (m_isOverloadedStorage || source.IS_A(m_type))
            {
                m_originality = AZStd::move(source.m_originality);
                InitializeOverloadedStorage(source.m_type, m_originality);
                m_class = AZStd::move(source.m_class);
                m_type = AZStd::move(source.m_type);
                m_storage = AZStd::move(source.m_storage);
                OnDatumChanged();
            }
            else if (ConvertImplicitlyChecked(source.GetType(), source.GetValueAddress(), m_type, m_storage, m_class))
            {
                OnDatumChanged();
            }
            else
            {
                AZ_Error("Script Canvas", false, "Failed to convert from %s to %s", GetName(source.GetType()).c_str(), GetName(m_type).c_str());
            }

            m_notificationId = source.m_notificationId;

            m_conversionStorage = AZStd::move(source.m_conversionStorage);

            m_datumLabel = AZStd::move(source.m_datumLabel);
            m_visibility = AZStd::move(source.m_visibility);
        }

        return *this;
    }

    Datum& Datum::operator=(const Datum& source)
    {
        if (this != &source)
        {
            if (m_isOverloadedStorage || source.IS_A(m_type))
            {
                m_originality = eOriginality::Copy;
                InitializeOverloadedStorage(source.m_type, m_originality);
                m_class = source.m_class;
                m_type = source.m_type;
                m_storage = source.m_storage;
                OnDatumChanged();
            }
            else if (ConvertImplicitlyChecked(source.GetType(), source.GetValueAddress(), m_type, m_storage, m_class))
            {
                OnDatumChanged();
            }
            else
            {
                AZ_Error("Script Canvas", false, "Failed to convert from %s to %s", GetName(source.GetType()).c_str(), GetName(m_type).c_str());
            }

            m_notificationId = source.m_notificationId;

            m_conversionStorage = source.m_conversionStorage;

            m_datumLabel = source.m_datumLabel;
            m_visibility = source.m_visibility;
        }

        return *this;
    }

    ComparisonOutcome Datum::operator==(const Datum& other) const
    {
        if (this == &other)
        {
            return AZ::Success(true);
        }
        else if (m_type.IS_EXACTLY_A(other.GetType()))
        {
            if (m_type.GetType() == Data::eType::BehaviorContextObject)
            {
                return CallComparisonOperator(AZ::Script::Attributes::OperatorType::Equal, *m_class, *this, other);
            }
            else
            {
                return AZ::Success(IsDataEqual(m_type, GetValueAddress(), other.GetValueAddress()));
            }
        }
        else if (m_type.IsConvertibleTo(other.GetType()))
        {
            return AZ::Success(IsDataEqual(m_type.GetType() == Data::eType::BehaviorContextObject ? other.GetType() : m_type, GetValueAddress(), other.GetValueAddress()));
        }
        return AZ::Failure(AZStd::string("Invalid call of Datum::operator=="));
    }

    ComparisonOutcome Datum::operator!=(const Datum& other) const
    {
        if (this == &other)
        {
            return AZ::Success(false);
        }

        ComparisonOutcome isEqualResult = (*this == other);
        if (isEqualResult.IsSuccess())
        {
            return AZ::Success(!isEqualResult.GetValue());
        }
        else
        {
            return AZ::Failure(AZStd::string("Invalid call of Datum::operator!="));
        }
    }

    ComparisonOutcome Datum::operator<(const Datum& other) const
    {
        if (this == &other)
        {
            return AZ::Success(false);
        }
        else if (m_type.IS_EXACTLY_A(other.GetType()))
        {
            if (m_type.GetType() == Data::eType::BehaviorContextObject)
            {
                return CallComparisonOperator(AZ::Script::Attributes::OperatorType::LessThan, *m_class, *this, other);
            }
            else 
            {
                return AZ::Success(IsDataLess(m_type, GetValueAddress(), other.GetValueAddress()));
            }
        }
        else if (m_type.IsConvertibleTo(other.GetType()))
        {
            return AZ::Success(IsDataLess(m_type.GetType() == Data::eType::BehaviorContextObject ? other.GetType() : m_type, GetValueAddress(), other.GetValueAddress()));
        }

        return AZ::Failure(AZStd::string("Invalid call of Datum::operator<"));
    }

    ComparisonOutcome Datum::operator<=(const Datum& other) const
    {
        if (this == &other)
        {
            return AZ::Success(true);
        }
        else if (m_type.IS_EXACTLY_A(other.GetType()))
        {
            if (m_type.GetType() == Data::eType::BehaviorContextObject)
            {
                return CallComparisonOperator(AZ::Script::Attributes::OperatorType::LessEqualThan, *m_class, *this, other);
            }
            else
            {
                return AZ::Success(IsDataLessEqual(m_type, GetValueAddress(), other.GetValueAddress()));
            }
        }
        else if (m_type.IsConvertibleTo(other.GetType()))
        {
            return AZ::Success(IsDataLessEqual(m_type.GetType() == Data::eType::BehaviorContextObject ? other.GetType() : m_type, GetValueAddress(), other.GetValueAddress()));
        }

        return AZ::Failure(AZStd::string("Invalid call of Datum::operator<="));
    }

    ComparisonOutcome Datum::operator>(const Datum& other) const
    {
        if (this == &other)
        {
            return AZ::Success(false);
        }

        ComparisonOutcome isLessEqualResult = (*this <= other);
        
        if (isLessEqualResult.IsSuccess())
        {
            return AZ::Success(!isLessEqualResult.GetValue());
        }
        else
        {
            return AZ::Failure(AZStd::string("Invalid call of Datum::Datum::operator>"));
        }
    }

    ComparisonOutcome Datum::operator>=(const Datum& other) const
    {
        if (this == &other)
        {
            return AZ::Success(true);
        }

        ComparisonOutcome isLessResult = (*this < other);
        
        if (isLessResult.IsSuccess())
        {
            return AZ::Success(!isLessResult.GetValue());
        }
        else
        {
            return AZ::Failure(AZStd::string("Invalid call of Datum::Datum::operator>="));
        }
    }

    void Datum::OnDatumChanged()
    {
        DatumSystemNotificationBus::Broadcast(&DatumSystemNotifications::OnDatumChanged, *this);
        DatumNotificationBus::Event(m_notificationId, &DatumNotifications::OnDatumChanged, this);
    }

    void Datum::OnWriteEnd()
    {
        if (m_type.GetType() == Data::eType::BehaviorContextObject)
        {
            /*
            BehaviorContextObject types require that their behavior context classes are updated, and their type infos are updated
            */
            AZ::BehaviorContext* behaviorContext = nullptr;
            AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
            AZ_Assert(behaviorContext, "Script Canvas can't do anything without a behavior context!");

            auto classIter = behaviorContext->m_typeToClassMap.find(m_type.GetAZType());
            if (classIter != behaviorContext->m_typeToClassMap.end() && classIter->second)
            {
                m_class = classIter->second;
            }
            else
            {
                AZ_Error("Script Canvas", false, "Datum type de-serialized, but no such class found in the behavior context");
            }
        }
    }

    void Datum::Reflect(AZ::ReflectContext* reflection)
    {
        if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection))
        {
            serializeContext->Class<Datum>()
                ->Version(6)
                ->EventHandler<SerializeContextEventHandler>()
                ->Field("m_isUntypedStorage", &Datum::m_isOverloadedStorage)
                ->Field("m_type", &Datum::m_type)
                ->Field("m_originality", &Datum::m_originality)
                ->Field("m_datumStorage", &Datum::m_storage)
                ->Field("m_datumLabel", &Datum::m_datumLabel)
                ;

            if (auto editContext = serializeContext->GetEditContext())
            {
                editContext->Class<Datum>("Datum", "Datum")
                    ->ClassElement(AZ::Edit::ClassElements::EditorData, "Datum")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &Datum::GetVisibility)
                        ->Attribute(AZ::Edit::Attributes::ChildNameLabelOverride, &Datum::GetLabel)
                    ->DataElement(AZ::Edit::UIHandlers::Default, &Datum::m_storage, "Datum", "")
                        ->Attribute(AZ::Edit::Attributes::Visibility, &Datum::GetDatumVisibility)
                        ->Attribute(AZ::Edit::Attributes::AutoExpand, true)
                        ->Attribute(AZ::Edit::Attributes::ContainerCanBeModified, true)
                        ->Attribute(AZ::Edit::Attributes::ChangeNotify, &Datum::OnDatumChanged)
                    ;
            }
        }
    }

    void Datum::ResolveSelfEntityReferences(const AZ::EntityId& graphOwnerId)
    {
        // remap graph unique id to graph entity id
        AZ::SerializeContext* serializeContext{};
        AZ::ComponentApplicationBus::BroadcastResult(serializeContext, &AZ::ComponentApplicationRequests::GetSerializeContext);
        AZStd::unordered_map<AZ::EntityId, AZ::EntityId> uniqueIdMap;
        uniqueIdMap.emplace(ScriptCanvas::GraphOwnerId, graphOwnerId);
        AZ::IdUtils::Remapper<AZ::EntityId>::RemapIds(this, [&uniqueIdMap](AZ::EntityId sourceId, bool, const AZ::IdUtils::Remapper<AZ::EntityId>::IdGenerator&) -> AZ::EntityId
        {
            auto findIt = uniqueIdMap.find(sourceId);
            return findIt != uniqueIdMap.end() ? findIt->second : sourceId;
        }, serializeContext, false);
    }
    
    void Datum::SetToDefaultValueOfType()
    {
        if (m_isOverloadedStorage)
        {
            Clear();
        }
        else
        {
            auto& typeIdTraitMap = GetDataRegistry()->m_typeIdTraitMap;
            auto dataTraitIt = typeIdTraitMap.find(m_type.GetType());
            if (dataTraitIt != typeIdTraitMap.end())
            {
                m_storage = dataTraitIt->second.m_dataTraits.GetDefault(m_type);
            }
            else
            {
                AZ_Error("Script Canvas", false, "Unsupported ScriptCanvas Data type");
            }
        }

        OnDatumChanged();
    }

    void Datum::SetLabel(AZStd::string_view name)
    {
        m_datumLabel = name;
    }

    AZStd::string Datum::GetLabel() const
    {
        return m_datumLabel;
    }

    void Datum::SetVisibility(AZ::Crc32 visibility)
    {
        m_visibility = visibility;
    }

    AZ::Crc32 Datum::GetVisibility() const
    {
        return m_visibility;
    }

    AZ::Crc32 Datum::GetDatumVisibility() const
    {
        if (IS_A<Data::QuaternionType>())
        {
            return AZ::Edit::PropertyVisibility::Hide;
        }
        else
        {
            return AZ::Edit::PropertyVisibility::ShowChildrenOnly;
        }
    }
    
    void Datum::SetNotificationsTarget(AZ::EntityId notificationId)
    {
        m_notificationId = notificationId;
    }

    // pushes this datum to the void* address in destination
    bool Datum::ToBehaviorContext(AZ::BehaviorValueParameter& destination) const
    {
        /// \todo support polymorphism
        AZ::BehaviorContext* behaviorContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
        AZ_Assert(behaviorContext, "Script Canvas can't do anything without a behavior context!");
        AZ::BehaviorClass* destinationBehaviorClass = AZ::BehaviorContextHelper::GetClass(behaviorContext, destination.m_typeId);               
        const auto targetType = Data::FromAZType(destination.m_typeId);
        
        const bool success = 
            ( (IS_A(targetType) || IsConvertibleTo(targetType)) || (IS_A(Data::Type::String()) && AZ::BehaviorContextHelper::IsStringParameter(destination)) )
            && ::ToBehaviorContext(m_type, GetValueAddress(), destination, destinationBehaviorClass);

        AZ_Error("Script Canvas", success, "Cannot push Datum with type %s into BehaviorValueParameter expecting type %s", GetName(m_type).c_str(), GetName(targetType).c_str());

        return success;
    }

    bool Datum::ToBehaviorContextNumber(void* target, const AZ::Uuid& typeID) const
    {
        return ::ToBehaviorContextNumber(target, typeID, GetValueAddress());
    }

    AZ::Outcome<AZ::BehaviorValueParameter, AZStd::string> Datum::ToBehaviorValueParameter(const AZ::BehaviorParameter& description) const
    {
        AZ_Assert(m_isOverloadedStorage || IS_A(Data::FromAZType(description.m_typeId)) || IsConvertibleTo(description), "Mismatched type going to behavior value parameter: %s", description.m_name);
        
        const_cast<Datum*>(this)->InitializeOverloadedStorage(Data::FromAZType(description.m_typeId), eOriginality::Copy);
        
        if (!Data::IsValueType(m_type) && !SatisfiesTraits(description.m_traits))
        {
            return AZ::Failure(AZStd::string("Attempting to convert null value to BehaviorValueParameter that expects reference or value"));
        }

        if (IS_A(Data::Type::Number()))
        {
            return AZ::Success(const_cast<Datum*>(this)->ToBehaviorValueParameterNumber(description));
        }
        else if (IS_A(Data::Type::String()) && AZ::BehaviorContextHelper::IsStringParameter(description))
        {
            return const_cast<Datum*>(this)->ToBehaviorValueParameterString(description);
        }
        else
        {
            AZ::BehaviorValueParameter parameter;
            parameter.m_typeId = description.m_typeId; /// \todo verify there is no polymorphic danger here
            parameter.m_name = m_class ? m_class->m_name.c_str() : Data::GetBehaviorContextName(m_type);
            parameter.m_azRtti = m_class ? m_class->m_azRtti : nullptr;

            if (description.m_traits & AZ::BehaviorParameter::TR_POINTER)
            {
                m_pointer = ModValueAddress();
                if (description.m_traits & AZ::BehaviorParameter::TR_THIS_PTR && !m_pointer)
                {
                    return AZ::Failure(AZStd::string(R"(Cannot invoke behavior context method on nullptr "this" parameter)"));
                }
                parameter.m_value = &m_pointer;
                parameter.m_traits = AZ::BehaviorParameter::TR_POINTER;
            }
            else
            {
                parameter.m_value = ModValueAddress();
                parameter.m_traits = 0;
            }

            return AZ::Success(parameter);
        }
    }
    
    AZ::Outcome<AZ::BehaviorValueParameter, AZStd::string> Datum::ToBehaviorValueParameterResult(const AZ::BehaviorParameter& description)
    {
        AZ_Assert(m_isOverloadedStorage || IS_A(Data::FromAZType(description.m_typeId)) || IsConvertibleTo(description), "Mismatched type going to behavior value parameter: %s", description.m_name);

        InitializeOverloadedStorage(Data::FromAZType(description.m_typeId), eOriginality::Copy);

        if (IS_A(Data::Type::Number()))
        {
            return AZ::Success(const_cast<Datum*>(this)->ToBehaviorValueParameterNumber(description));
        }
        else if (IS_A(Data::Type::String()) && AZ::BehaviorContextHelper::IsStringParameter(description))
        {
            return const_cast<Datum*>(this)->ToBehaviorValueParameterString(description);
        }

        AZ::BehaviorValueParameter parameter;

        if (IsValueType(m_type))
        {
            parameter.m_typeId = description.m_typeId; /// \todo verify there is no polymorphic danger here
            parameter.m_name = m_class ? m_class->m_name.c_str() : Data::GetBehaviorContextName(m_type);
            parameter.m_azRtti = m_class ? m_class->m_azRtti : nullptr;

            if (description.m_traits & AZ::BehaviorParameter::TR_POINTER)
            {
                m_pointer = ModResultAddress();
                
                if (!m_pointer)
                {
                    return AZ::Failure(AZStd::string("nowhere to go for the for behavior context result"));
                }

                parameter.m_value = &m_pointer;
                parameter.m_traits = AZ::BehaviorParameter::TR_POINTER;
            }
            else
            {
                parameter.m_value = ModResultAddress();
                
                if (!parameter.m_value)
                {
                    return AZ::Failure(AZStd::string("nowhere to go for the for behavior context result"));
                }
                
                parameter.m_traits = 0;
            }
        }
        else
        {
            parameter.m_typeId = description.m_typeId; /// \todo verify there is no polymorphic danger here
            parameter.m_name = m_class ? m_class->m_name.c_str() : Data::GetBehaviorContextName(m_type);
            parameter.m_azRtti = m_class ? m_class->m_azRtti : nullptr;
            
            if (description.m_traits & (AZ::BehaviorParameter::TR_POINTER | AZ::BehaviorParameter::TR_REFERENCE))
            {
                parameter.m_value = &m_pointer;
                parameter.m_traits = AZ::BehaviorParameter::TR_POINTER;
            }
            else
            {
                parameter.m_value = ModResultAddress();
                
                if (!parameter.m_value)
                {
                    return AZ::Failure(AZStd::string("nowhere to go for the for behavior context result"));
                }
            }
        }

        return AZ::Success(parameter);
    }

    AZ::BehaviorValueParameter Datum::ToBehaviorValueParameterNumber(const AZ::BehaviorParameter& description)
    {
        AZ_Assert(IS_A(Data::Type::Number()), "ToBehaviorValueParameterNumber is only for numbers");
        // m_conversion isn't a number yet
        // make it a number by initializing it to the proper type
        ::ToBehaviorContextNumber(m_conversionStorage, description.m_typeId, GetValueAddress());
        return ConvertibleToBehaviorValueParameter(description, description.m_typeId, "number", reinterpret_cast<void*>(&m_conversionStorage), m_pointer);
    }

    AZ::Outcome<AZ::BehaviorValueParameter, AZStd::string> Datum::ToBehaviorValueParameterString(const AZ::BehaviorParameter& description)
    {
        AZ_Assert(IS_A(Data::Type::String()), "Cannot created BehaviorValueParameter that contains a string. Datum type must be a string");

        if (!AZ::BehaviorContextHelper::IsStringParameter(description))
        {
            return AZ::Failure(AZStd::string("BehaviorParameter is not a string parameter, a BehaviorValueParameter that references a Script Canvas string cannot be made"));
        }

        AZ::BehaviorContext* behaviorContext = nullptr;
        AZ::ComponentApplicationBus::BroadcastResult(behaviorContext, &AZ::ComponentApplicationRequests::GetBehaviorContext);
        AZ_Assert(behaviorContext, "Script Canvas can't do anything without a behavior context!");

        if (Data::IsString(description.m_typeId))
        {
            return AZ::Success(ConvertibleToBehaviorValueParameter(description, description.m_typeId, "AZStd::string", ModValueAddress(), m_pointer));
        }
        else
        {
            auto stringValue = GetAs<Data::StringType>();
            if (description.m_typeId == azrtti_typeid<char>() && (description.m_traits & (AZ::BehaviorParameter::TR_POINTER | AZ::BehaviorParameter::TR_CONST)))
            {
                return AZ::Success(ConvertibleToBehaviorValueParameter(description, description.m_typeId, "const char*", const_cast<char*>(stringValue->data()), m_pointer));
            }
            else if (description.m_typeId == azrtti_typeid<AZStd::string_view>())
            {
                m_conversionStorage = AZStd::make_any<AZStd::string_view>(*stringValue);
                return AZ::Success(ConvertibleToBehaviorValueParameter(description, description.m_typeId, "AZStd::string_view", AZStd::any_cast<void>(&m_conversionStorage), m_pointer));
            }
        }

        return AZ::Failure(AZStd::string::format("Cannot create a BehaviorValueParameter of type %s", description.m_name));
    }
       
    bool Datum::ToString(Data::StringType& result) const
    {
        switch (GetType().GetType())
        {
        case Data::eType::AABB:
            result = ToStringAABB(*GetAs<Data::AABBType>());
            return true;

        case Data::eType::BehaviorContextObject:
            ToStringBehaviorClassObject(result);
            return true;

        case Data::eType::Boolean:
            result = *GetAs<bool>() ? "true" : "false";
            return true;

        case Data::eType::Color:
            result = ToStringColor(*GetAs<Data::ColorType>());
            return true;

        case Data::eType::CRC:
            result = ToStringCRC(*GetAs<Data::CRCType>());
            return true;

        case Data::eType::EntityID:
            result = GetAs<AZ::EntityId>()->ToString();
            return true;

        case Data::eType::NamedEntityID:
            result = GetAs<AZ::NamedEntityId>()->ToString();
            return true;

        case Data::eType::Invalid:
            result = "Invalid";
            return true;

        case Data::eType::Matrix3x3:
            result = ToStringMatrix3x3(*GetAs<AZ::Matrix3x3>());
            return true;

        case Data::eType::Matrix4x4:
            result = ToStringMatrix4x4(*GetAs<AZ::Matrix4x4>());
            return true;

        case Data::eType::Number:
            result = AZStd::string::format("%f", *GetAs<Data::NumberType>());
            return true;

        case Data::eType::OBB:
            result = ToStringOBB(*GetAs<Data::OBBType>());
            return true;

        case Data::eType::Plane:
            result = ToStringPlane(*GetAs<Data::PlaneType>());
            return true;

        case Data::eType::Quaternion:
            result = ToStringQuaternion(*GetAs<Data::QuaternionType>());
            return true;

        case Data::eType::String:
            result = *GetAs<Data::StringType>();
            return true;

        case Data::eType::Transform:
            result = ToStringTransform(*GetAs<Data::TransformType>());
            return true;

        case Data::eType::Vector2:
            result = ToStringVector2(*GetAs<AZ::Vector2>());
            return true;

        case Data::eType::Vector3:
            result = ToStringVector3(*GetAs<AZ::Vector3>());
            return true;

        case Data::eType::Vector4:
            result = ToStringVector4(*GetAs<AZ::Vector4>());
            return true;

        default:
            AZ_Error("ScriptCanvas", false, "Unsupported type in Datum::ToString()");
            break;
        }

        result = AZStd::string::format("<Datum.ToString() failed for this type: %s >", Data::GetName(m_type).data());
        return false;
    }
    
    AZStd::string Datum::ToStringAABB(const Data::AABBType& aabb) const
    {
        return AZStd::string::format
            ( "(Min: %s, Max: %s)"
            , ToStringVector3(aabb.GetMin()).c_str()
            , ToStringVector3(aabb.GetMax()).c_str());
    }

    AZStd::string Datum::ToStringCRC(const Data::CRCType& source) const
    {
        return AZStd::string::format("0x%08x", static_cast<AZ::u32>(source));
    }

    AZStd::string Datum::ToStringColor(const Data::ColorType& c) const
    {
        return AZStd::string::format("(r=%.7f,g=%.7f,b=%.7f,a=%.7f)", static_cast<float>(c.GetR()), static_cast<float>(c.GetG()), static_cast<float>(c.GetB()), static_cast<float>(c.GetA()));
    }

    bool Datum::ToStringBehaviorClassObject(Data::StringType& stringOut) const
    {
        if (m_class)
        {
            for (auto& methodIt : m_class->m_methods)
            {
                auto* method = methodIt.second;

                if (AZ::Attribute* operatorAttr = AZ::FindAttribute(AZ::Script::Attributes::Operator, method->m_attributes))
                {
                    AZ::AttributeReader operatorAttrReader(nullptr, operatorAttr);
                    AZ::Script::Attributes::OperatorType operatorType{};
                    if (operatorAttrReader.Read<AZ::Script::Attributes::OperatorType>(operatorType) && operatorType == AZ::Script::Attributes::OperatorType::ToString &&
                        method->HasResult() && (method->GetResult()->m_typeId == azrtti_typeid<const char*>() || method->GetResult()->m_typeId == azrtti_typeid<AZStd::string>()))
                    {
                        if (method->GetNumArguments() > 0)
                        {
                            AZ::BehaviorValueParameter result(&stringOut);
                            AZ::Outcome<AZ::BehaviorValueParameter, AZStd::string> argument = ToBehaviorValueParameter(*method->GetArgument(0));
                            return argument.IsSuccess() && argument.GetValue().m_value && method->Call(&argument.GetValue(), 1, &result);
                        }
                    }
                }
            }
        }
        
        stringOut = "<Invalid ToString Method>";
        return false;
    }
    
    AZStd::string Datum::ToStringMatrix3x3(const AZ::Matrix3x3& m) const
    {
        return AZStd::string::format
            ( "(%s, %s, %s)"
            , ToStringVector3(m.GetColumn(0)).c_str()
            , ToStringVector3(m.GetColumn(1)).c_str()
            , ToStringVector3(m.GetColumn(2)).c_str());

    }

    AZStd::string Datum::ToStringMatrix4x4(const AZ::Matrix4x4& m) const
    {
        return AZStd::string::format
            ( "(%s, %s, %s, %s)"
            , ToStringVector4(m.GetColumn(0)).c_str()
            , ToStringVector4(m.GetColumn(1)).c_str()
            , ToStringVector4(m.GetColumn(2)).c_str()
            , ToStringVector4(m.GetColumn(3)).c_str());

    }

    AZStd::string Datum::ToStringOBB(const Data::OBBType& obb) const
    {
        return AZStd::string::format
            ( "(Position: %s, AxisX: %s, AxisY: %s, AxisZ: %s, halfLengthX: %.7f, halfLengthY: %.7f, halfLengthZ: %.7f)"
            , ToStringVector3(obb.GetPosition()).c_str()
            , ToStringVector3(obb.GetAxisX()).c_str()
            , ToStringVector3(obb.GetAxisY()).c_str()
            , ToStringVector3(obb.GetAxisZ()).c_str()
            , obb.GetHalfLengthX()
            , obb.GetHalfLengthY()
            , obb.GetHalfLengthZ());
    }

    AZStd::string Datum::ToStringPlane(const Data::PlaneType& source) const
    {
        return ToStringVector4(source.GetPlaneEquationCoefficients());
    }

    AZStd::string Datum::ToStringQuaternion(const Data::QuaternionType& source) const
    {
        AZ::Vector3 eulerRotation = AZ::ConvertTransformToEulerDegrees(AZ::Transform::CreateFromQuaternion(source));
        return AZStd::string::format
            ( "(Pitch: %5.2f, Roll: %5.2f, Yaw: %5.2f)"
            , static_cast<float>(eulerRotation.GetX())
            , static_cast<float>(eulerRotation.GetY())
            , static_cast<float>(eulerRotation.GetZ()));
    }

    AZStd::string Datum::ToStringTransform(const Data::TransformType& source) const
    {
        Data::TransformType copy(source);
        AZ::Vector3 pos = copy.GetPosition();
        AZ::Vector3 scale = copy.ExtractScale();
        AZ::Vector3 rotation = AZ::ConvertTransformToEulerDegrees(copy);
        return AZStd::string::format
             ( "(Position: X: %f, Y: %f, Z: %f,"
               " Rotation: X: %f, Y: %f, Z: %f,"
               " Scale: X: %f, Y: %f, Z: %f)"
             , static_cast<float>(pos.GetX()), static_cast<float>(pos.GetY()), static_cast<float>(pos.GetZ())
             , static_cast<float>(rotation.GetX()), static_cast<float>(rotation.GetY()), static_cast<float>(rotation.GetZ())
             , static_cast<float>(scale.GetX()), static_cast<float>(scale.GetY()), static_cast<float>(scale.GetZ()));
    }

    AZStd::string Datum::ToStringVector2(const AZ::Vector2& source) const
    {
        return AZStd::string::format
            ( "(X: %f, Y: %f)"
            , source.GetX()
            , source.GetY());
    }
    
    AZStd::string Datum::ToStringVector3(const AZ::Vector3& source) const
    {
        return AZStd::string::format
            ( "(X: %f, Y: %f, Z: %f)"
            , static_cast<float>(source.GetX())
            , static_cast<float>(source.GetY())
            , static_cast<float>(source.GetZ()));
    }
    
    AZStd::string Datum::ToStringVector4(const AZ::Vector4& source) const
    {
        return AZStd::string::format
            ("(X: %f, Y: %f, Z: %f, W: %f)"
            , static_cast<float>(source.GetX())
            , static_cast<float>(source.GetY())
            , static_cast<float>(source.GetZ())
            , static_cast<float>(source.GetW()));
    }

    AZ::Outcome<void, AZStd::string> Datum::CallBehaviorContextMethod(const AZ::BehaviorMethod* method, AZ::BehaviorValueParameter* params, unsigned int numExpectedArgs)
    {
        AZ_Assert(method, "AZ::BehaviorMethod* method == nullptr in Datum");
        if (method->Call(params, numExpectedArgs))
        {
            return AZ::Success();
        }
        else
        {
            return AZ::Failure(AZStd::string::format("Script Canvas call of %s failed", method->m_name.c_str()));
        }
    }

    AZ::Outcome<Datum, AZStd::string> Datum::CallBehaviorContextMethodResult(const AZ::BehaviorMethod* method, const AZ::BehaviorParameter* resultType, AZ::BehaviorValueParameter* params, unsigned int numExpectedArgs)
    {
        AZ_Assert(resultType, "const AZ::BehaviorParameter* resultType == nullptr in Datum");
        AZ_Assert(method, "AZ::BehaviorMethod* method == nullptr in Datum");
        // create storage for the destination of the results in the function call...
        Datum resultDatum(s_behaviorContextResultTag, *resultType);
        //...and initialize a AZ::BehaviorValueParameter to wrap the storage...
        AZ::Outcome<AZ::BehaviorValueParameter, AZStd::string> parameter = resultDatum.ToBehaviorValueParameterResult(*resultType);
        if (parameter.IsSuccess())
        {
            // ...the result of Call here will write back to it...
            if (method->Call(params, numExpectedArgs, &parameter.GetValue()))
            {
                // ...convert the storage, in case the function call result was one of many AZ:::RTTI types mapped to one SC::Type
                resultDatum.ConvertBehaviorContextMethodResult(*resultType);
                return AZ::Success(resultDatum);
            }
            else
            {
                return AZ::Failure(AZStd::string::format("Script Canvas call of %s failed", method->m_name.c_str()));
            }
        }
        else
        {
            // parameter conversion failed
            return AZ::Failure(parameter.GetError());
        }
    }

    bool Datum::IsValidDatum(const Datum* datum)
    {
        return datum != nullptr && !datum->Empty();
    }

} // namespace ScriptCanvas