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


#include <AzCore/Debug/Timer.h>
#include <AzCore/Debug/TraceMessageBus.h>
#include <AzCore/std/typetraits/typetraits.h>
#include <AzCore/UnitTest/TestTypes.h>

#include <AzTest/AzTest.h>

#include <AzCore/Memory/OSAllocator.h>

#if defined(AZ_TESTS_ENABLED)
DECLARE_AZ_UNIT_TEST_MAIN()
#endif

namespace AZ
{
    inline void* AZMemAlloc(AZStd::size_t byteSize, AZStd::size_t alignment, const char* name = "No name allocation")
    {
        (void)name;
        return AZ_OS_MALLOC(byteSize, alignment);
    }

    inline void AZFree(void* ptr, AZStd::size_t byteSize = 0, AZStd::size_t alignment = 0)
    {
        (void)byteSize;
        (void)alignment;
        AZ_OS_FREE(ptr);
    }
}
// END OF TEMP MEMORY ALLOCATIONS

using namespace AZ;

// Handle asserts
class TraceDrillerHook
    : public AZ::Test::ITestEnvironment
    , public UnitTest::TraceBusRedirector
{
public:
    void SetupEnvironment() override
    {
        AllocatorInstance<OSAllocator>::Create(); // used by the bus

        BusConnect();
    }

    void TeardownEnvironment() override
    {
        BusDisconnect();

        AllocatorInstance<OSAllocator>::Destroy(); // used by the bus
    }
};

AZ_UNIT_TEST_HOOK(new TraceDrillerHook());

#if defined(HAVE_BENCHMARK)
AZ_BENCHMARK_HOOK();
#endif // HAVE_BENCHMARK
