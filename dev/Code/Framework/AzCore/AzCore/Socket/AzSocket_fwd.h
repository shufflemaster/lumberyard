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

#include <AzCore/base.h>

#if !AZ_TRAIT_OS_USE_SOCKETS
#   error Platform not supported!
#endif

#define SOCKET_ERROR (-1)
#define AZ_SOCKET_INVALID (-1)

#if defined(AZ_RESTRICTED_PLATFORM)
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/AzSocket_fwd_h_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/AzSocket_fwd_h_provo.inl"
    #endif
#endif
#if defined(AZ_RESTRICTED_SECTION_IMPLEMENTED)
#undef AZ_RESTRICTED_SECTION_IMPLEMENTED
#else
struct sockaddr;
struct sockaddr_in;
#endif

// Type wrappers for sockets
typedef sockaddr    AZSOCKADDR;
typedef sockaddr_in AZSOCKADDR_IN;
typedef AZ::s32     AZSOCKET;

namespace AZ
{
    namespace AzSock
    {
        enum class AzSockError : AZ::s32;
        enum class AZSocketOption : AZ::s32;
        class AzSocketAddress;
    }; // namespace AzSock
}; // namespace AZ
