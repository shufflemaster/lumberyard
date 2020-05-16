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

#include <atomic>
#include <AzCore/Console/ConsoleFunctor.h>
#include <AzCore/std/parallel/scoped_lock.h>


namespace AZ
{
    enum class ThreadSafety
    {
        RequiresLock
    ,   UseStdAtomic
    };


    //! @class ThreadSafeObject
    //! 
    //! Wraps an object in a thread-safe interface.
    template <typename _TYPE>
    class ThreadSafeObject
    {
    public:

        ThreadSafeObject() = default;
        ThreadSafeObject(const _TYPE& rhs);
        ~ThreadSafeObject() = default;

        //! Assigns a new value to the object under lock.
        //! 
        //! @param value the new value to set the object instance to
        void operator =(const _TYPE& value);

        //! Implicit conversion to underlying type.
        //! 
        //! @return a copy of the current value of the object retrieved under lock
        operator _TYPE() const;

    private:

        ThreadSafeObject& operator =(const ThreadSafeObject& rhs) = delete;

        mutable AZStd::mutex m_mutex;
        _TYPE m_object;

    };


    //! @class ConsoleDataContainer
    //! 
    //! Base data wrapper class.
    template <typename BASE_TYPE, ThreadSafety THREAD_SAFETY>
    class ConsoleDataContainer {};

    template <typename BASE_TYPE>
    class ConsoleDataContainer<BASE_TYPE, ThreadSafety::RequiresLock>
    {
    protected:
        ThreadSafeObject<BASE_TYPE> m_value;
    };

    template <typename BASE_TYPE>
    class ConsoleDataContainer<BASE_TYPE, ThreadSafety::UseStdAtomic>
    {
    protected:
        std::atomic<BASE_TYPE> m_value;
    };

    //! @class ConsoleDataWrapper<BASE_TYPE, THREAD_SAFETY>
    //! 
    //! Data wrapper class for console variables.
    template <typename BASE_TYPE, ThreadSafety THREAD_SAFETY>
    class ConsoleDataWrapper
    :   public ConsoleDataContainer<BASE_TYPE, THREAD_SAFETY>
    {
    public:

        using BaseType = BASE_TYPE;
        using SelfType = ConsoleDataWrapper<BASE_TYPE, THREAD_SAFETY>;
        using CallbackFunc = void(*)(const BaseType& value);

        //! Constructor.
        //! 
        //! @param value    the initial value to initialize the wrapped data to
        //! @param callback an optional callback that will be invoked upon any change in value to this variable
        //! @param name     the string name of the variable
        //! @param desc     a string help description of the variable
        //! @param flags    a set of optional flags that mutate the behaviour of the variable
        ConsoleDataWrapper(const BASE_TYPE& value, CallbackFunc callback, const char* name, const char* desc, ConsoleFunctorFlags flags);

        //! Assignment from underlying base type.
        //! 
        //! @param rhs base type value to assign from
        void operator =(const BASE_TYPE& rhs);

        //! Const base type operator
        //! 
        //! @return value in const base type form.
        operator BASE_TYPE() const;

        //! Equality operator, provided for convenience.
        //! The contained value could have changed after the comparison is made and before the result is returned.
        //! 
        //! @param rhs base type value to compare against
        //! 
        //! @return boolean true if this == rhs
        bool operator ==(const BASE_TYPE& rhs) const;

        //! Inequality operator, provided for convenience.
        //! The contained value could have changed after the comparison is made and before the result is returned
        //! 
        //! @param rhs base type value to compare against
        //! 
        //! @return boolean true if this != rhs
        bool operator !=(const BASE_TYPE& rhs) const;

        //! Strictly less than operator, provided for convenience.
        //! The contained value could have changed after the comparison is made and before the result is returned
        //! 
        //! @param rhs base type value to compare against
        //! 
        //! @return boolean true if this < rhs
        bool operator <(const BASE_TYPE& rhs) const;

        //! Less than equal to operator, provided for convenience.
        //! The contained value could have changed after the comparison is made and before the result is returned
        //! 
        //! @param rhs base type value to compare against
        //! 
        //! @return boolean true if this <= rhs
        bool operator <=(const BASE_TYPE& rhs) const;

        //! Strictly greater than operator, provided for convenience.
        //! The contained value could have changed after the comparison is made and before the result is returned
        //! 
        //! @param rhs base type value to compare against
        //! 
        //! @return boolean true if this > rhs
        bool operator >(const BASE_TYPE& rhs) const;

        //! Greater than equal to operator, provided for convenience.
        //! The contained value could have changed after the comparison is made and before the result is returned
        //! 
        //! @param rhs base type value to compare against
        //! 
        //! @return boolean true if this >= rhs
        bool operator >=(const BASE_TYPE& rhs) const;

        //! Reads data contained in arguments to set the console variable value.
        //! 
        //! @param arguments StringSet instance to read new values from
        //! 
        //! @return boolean true on success, false if an error occurred
        bool StringToValue(const StringSet& arguments);

        //! Stringifies the contained variables value and write the result to outString.
        //! 
        //! @param outString output string instance to write the stringified value to
        void ValueToString(AZStd::string& outString) const;

        //! Invokes bound callback on the wrapped BaseType value.
        void InvokeCallback() const;

        //! Cvar functor, reads data contained in arguments to set the console variable value.
        //! 
        //! @param arguments StringSet instance to read new values from
        void CvarFunctor(const StringSet& arguments);

    private:

        ConsoleDataWrapper& operator =(const ConsoleDataWrapper&) = delete;

        CallbackFunc m_callback;
        ConsoleFunctor<SelfType, true> m_functor;

    };
}

#include <AzCore/Console/ConsoleDataWrapper.inl>
