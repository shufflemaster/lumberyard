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
#ifndef AZ_UNITY_BUILD

#include <AzCore/Math/Random.h>

#if defined(AZ_RESTRICTED_PLATFORM)
#undef AZ_RESTRICTED_SECTION
#define RANDOM_CPP_SECTION_1 1
#define RANDOM_CPP_SECTION_2 2
#endif

#if defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION RANDOM_CPP_SECTION_1
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/Random_cpp_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/Random_cpp_provo.inl"
    #endif
#elif defined(AZ_PLATFORM_WINDOWS)
#   include <AzCore/PlatformIncl.h>
#   include <Wincrypt.h>
#endif

using namespace AZ;

//=========================================================================
// CreateNull
// [4/10/2012]
//=========================================================================
BetterPseudoRandom::BetterPseudoRandom()
{ 
#if defined(AZ_PLATFORM_WINDOWS)
    if (!CryptAcquireContext(&m_generatorHandle, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
    {
        AZ_Warning("System", false, "CryptAcquireContext failed with 0x%08x\n", GetLastError());
        m_generatorHandle = 0;
    }
#elif AZ_TRAIT_PSUEDO_RANDOM_USE_FILE
    m_generatorHandle = fopen("/dev/urandom", "r");
#endif
}

//=========================================================================
// GetValue
// [4/10/2012]
//=========================================================================
BetterPseudoRandom::~BetterPseudoRandom()
{
#if defined(AZ_PLATFORM_WINDOWS)
    if (m_generatorHandle)
    {
        CryptReleaseContext(m_generatorHandle, 0);
    }
#elif AZ_TRAIT_PSUEDO_RANDOM_USE_FILE
    if (m_generatorHandle)
    {
        fclose(m_generatorHandle);
    }
#endif
}

//=========================================================================
// CreateString
// [4/10/2012]
//=========================================================================
bool BetterPseudoRandom::GetRandom(void* data, size_t dataSize)
{
#if defined(AZ_PLATFORM_WINDOWS)
    if (!m_generatorHandle)
    {
        return false;
    }
    if (!CryptGenRandom(m_generatorHandle, static_cast<DWORD>(dataSize), static_cast<PBYTE>(data)))
    {
        AZ_TracePrintf("System", "Failed to call CryptGenRandom!");
        return false;
    }
#elif AZ_TRAIT_PSUEDO_RANDOM_USE_FILE
    if (!m_generatorHandle)
    {
        return false;
    }

    if (fread(data, 1, dataSize, m_generatorHandle) != dataSize)
    {
        AZ_TracePrintf("System", "Failed to read %d bytes from /dev/urandom!", dataSize);
        fclose(m_generatorHandle);
        m_generatorHandle = nullptr;
        return false;
    }
#elif defined(AZ_RESTRICTED_PLATFORM)
#define AZ_RESTRICTED_SECTION RANDOM_CPP_SECTION_2
    #if defined(AZ_PLATFORM_XENIA)
        #include "Xenia/Random_cpp_xenia.inl"
    #elif defined(AZ_PLATFORM_PROVO)
        #include "Provo/Random_cpp_provo.inl"
    #endif
#endif
    return true;
}
#endif // #ifndef AZ_UNITY_BUILD
