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
#ifndef AZCORE_IO_BYTECONTAINERSTREAM_H
#define AZCORE_IO_BYTECONTAINERSTREAM_H

#include <AzCore/IO/GenericStreams.h>
#include <AzCore/Math/MathUtils.h>
#include <AzCore/std/typetraits/is_const.h>
#include <AzCore/std/typetraits/has_member_function.h>

namespace AZ
{
    namespace IO
    {
        /*
         * ByteContainerStream
         * A stream that wraps around a sequence container
         * Currently only AZStd::vector and AZStd::fixed_vector are supported as
         * we use direct memcpy, so underlying storage has to be guaranteed continuous.
         * It can be generalized if necessary, but for now this should do.
         */
        template<bool V>
        struct WritableStreamType  {};
        template<>
        struct WritableStreamType<true>
        {
            typedef int type;
        };

        template<typename ContainerType>
        class ByteContainerStream
            : public GenericStream
        {
        public:
            ByteContainerStream(ContainerType* buffer, size_t maxGrowSize = 5 * 1024 * 1024);
            virtual ~ByteContainerStream() {}

            virtual bool        IsOpen() const                              { return true; }
            virtual bool        CanSeek() const                             { return true; }
            virtual bool        CanRead() const override                    { return true; }
            virtual bool        CanWrite() const override                   { return true; }
            virtual SizeType    GetCurPos() const                           { return m_pos; }
            virtual SizeType    GetLength() const                           { return m_buffer->size(); }
            virtual void        Seek(OffsetType bytes, SeekMode mode);
            virtual SizeType    Read(SizeType bytes, void* oBuffer);
            virtual SizeType    Write(SizeType bytes, const void* iBuffer);
            template<typename T>
            inline SizeType Write(const T* iBuffer)
            {
                return Write(sizeof(T), iBuffer);
            }

            // Truncate the stream to the current position.
            void                Truncate();

            ContainerType*      GetData() const                             { return m_buffer; }

        protected:
            template<typename T>
            SizeType Write(SizeType bytes, const void* iBuffer, typename T::type);
            template<typename T>
            SizeType Write(SizeType, const void*, unsigned int);

            ContainerType*  m_buffer;
            size_t          m_pos;
            size_t          m_maxGrowSize; //< Maximum grow size to avoid the buffer growing too fast when it's working on big files
        };

        namespace Internal
        {
            AZ_HAS_MEMBER(ResizeNoConstruct, resize_no_construct, void, (size_t));

            // As we are about to write bytes to a container, use the fast resize (without constructing elements) if available.
            template<class ContainerType>
            inline void ResizeContainerBuffer(ContainerType* buffer, size_t newLength, const AZStd::true_type& /* HasResizeNoConstruct<ContainerType> */)
            {
                buffer->resize_no_construct(newLength);
            }

            // If resize_no_construct if not supported by the container, just call resize.
            template<class ContainerType>
            inline void ResizeContainerBuffer(ContainerType* buffer, size_t newLength, const AZStd::false_type& /* HasResizeNoConstruct<ContainerType> */)
            {
                buffer->resize(newLength);
            }

            // Set the container capacity to manage better number of allocations (especially when doing small allocation)
            AZ_HAS_MEMBER(SetCapacity, set_capacity, void, (size_t));

            template<class ContainerType>
            inline void AdjustContainerBufferCapacity(ContainerType* buffer, size_t requiredSize, size_t maxCapacityGrowSize, const AZStd::true_type& /* HasSetCapacity<ContainerType> */)
            {
                if (requiredSize > buffer->capacity())
                {
                    // grow by 50%, if we can.
                    size_t newCapacityGrowSize = AZ::GetClamp<size_t>(requiredSize / 2, 4, maxCapacityGrowSize);
                    size_t newCapacity = requiredSize + newCapacityGrowSize;
                    buffer->set_capacity(newCapacity);
                }
            }

            template<class ContainerType>
            inline void AdjustContainerBufferCapacity(ContainerType* buffer, size_t requiredSize, size_t maxCapacityGrowSize, const AZStd::false_type& /* HasSetCapacityt<ContainerType> */)
            {
                (void)buffer; (void)requiredSize; (void)maxCapacityGrowSize;
                // no set capacity support we will need to rely on resize alone
            }
        }

        template<typename ContainerType>
        ByteContainerStream<ContainerType>::ByteContainerStream(ContainerType* buffer, size_t maxGrowSize)
            : m_buffer(buffer)
            , m_pos(0)
            , m_maxGrowSize(maxGrowSize)
        {
            AZ_STATIC_ASSERT(sizeof(typename ContainerType::value_type) == 1, "The buffer is not a container of bytes!");
            AZ_Assert(m_buffer, "You cannot make a ByteContainerStream with buffer = null!");
        }

        template<typename ContainerType>
        void ByteContainerStream<ContainerType>::Seek(OffsetType bytes, SeekMode mode)
        {
            m_pos = static_cast<size_t>(ComputeSeekPosition(bytes, mode));
        }

        template<typename ContainerType>
        SizeType ByteContainerStream<ContainerType>::Read(SizeType bytes, void* oBuffer)
        {
            size_t len = m_buffer->size();
            size_t bytesToRead = 0;
            if (m_pos + static_cast<size_t>(bytes) < len)
            {
                bytesToRead = static_cast<size_t>(bytes);
            }
            else if (len > m_pos)
            {
                bytesToRead = len - m_pos;
            }
            if (bytesToRead > 0)
            {
                AZ_Assert(oBuffer, "Output buffer can't be null!");
                memcpy(oBuffer, m_buffer->data() + m_pos, bytesToRead);
                m_pos += bytesToRead;
            }
            return bytesToRead;
        }

        template<typename ContainerType>
        SizeType ByteContainerStream<ContainerType>::Write(SizeType bytes, const void* iBuffer)
        {
            return Write<WritableStreamType<!AZStd::is_const<ContainerType>::value> >(bytes, iBuffer, 0);
        }

        template<typename ContainerType>
        template<typename T>
        SizeType ByteContainerStream<ContainerType>::Write(SizeType bytes, const void* iBuffer, typename T::type)
        {
            size_t len = m_buffer->size();
            size_t requiredLen = static_cast<size_t>(m_pos + bytes);

            if (requiredLen > len)
            {
                // Make sure we grow the write buffer in a reasonable manner to avoid too many allocations
                Internal::AdjustContainerBufferCapacity(m_buffer, requiredLen, m_maxGrowSize,typename Internal::HasSetCapacity<ContainerType>::type());

                Internal::ResizeContainerBuffer(m_buffer, requiredLen, typename Internal::HasResizeNoConstruct<ContainerType>::type());
            }

            size_t bytesToCopy = static_cast<size_t>(bytes);
            memcpy(m_buffer->data() + m_pos, iBuffer, bytesToCopy);
            m_pos += bytesToCopy;
            return bytes;
        }

        template<typename ContainerType>
        template<typename T>
        SizeType ByteContainerStream<ContainerType>::Write(SizeType, const void*, unsigned int)
        {
            AZ_Assert(false, "This stream is read-only!");
            return 0;
        }

        template<typename ContainerType>
        void ByteContainerStream<ContainerType>::Truncate()
        {
            Internal::ResizeContainerBuffer(m_buffer, m_pos, typename Internal::HasResizeNoConstruct<ContainerType>::type());
        }
    }   // IO
}   // namespace AZ

#endif  // AZCORE_IO_BYTECONTAINERSTREAM_H
#pragma once
