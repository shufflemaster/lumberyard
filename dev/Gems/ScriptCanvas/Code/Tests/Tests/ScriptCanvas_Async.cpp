/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright EntityRef license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

#include "precompiled.h"

#include <AzCore/PlatformDef.h>

#include <Tests/ScriptCanvasTestFixture.h>
#include <Tests/ScriptCanvasTestUtilities.h>

using namespace ScriptCanvasTests;

// Asynchronous ScriptCanvas Behaviors

// TODO #lsempe: Async tests are not available in VS2013 due to some errors with std::promise, enable after problem is solved.
#if AZ_COMPILER_MSVC && AZ_COMPILER_MSVC >= 1900

#pragma warning(disable: 4355) // The this pointer is valid only within nonstatic member functions. It cannot be used in the initializer list for a base class.
#include <future>

class AsyncEvent : public AZ::EBusTraits
{
public:

    //////////////////////////////////////////////////////////////////////////
    // EBusTraits overrides
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::ById;
    using BusIdType = AZ::EntityId;
    //////////////////////////////////////////////////////////////////////////

    virtual void OnAsyncEvent() = 0;
};

using AsyncEventNotificationBus = AZ::EBus<AsyncEvent>;

class LongRunningProcessSimulator3000
{
public:

    static void Run(const AZ::EntityId& listener)
    {
        int duration = 40;
        while (--duration > 0)
        {
            AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(10));
        }

        AsyncEventNotificationBus::Event(listener, &AsyncEvent::OnAsyncEvent);
    }
};

class AsyncNode
    : public ScriptCanvas::Node
    , protected AsyncEventNotificationBus::Handler
    , protected AZ::TickBus::Handler
{
public:
    AZ_COMPONENT(AsyncNode, "{0A7FF6C6-878B-42EC-A8BB-4D29C4039853}", ScriptCanvas::Node);

    bool IsEntryPoint() const { return true; }

    AsyncNode()
        : Node()
    {}

    static void Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<AsyncNode, Node>()
                ->Version(1)
                ;
        }
    }

    void ConfigureSlots() override
    {
        AddSlot("In", "", ScriptCanvas::SlotType::ExecutionIn);
        AddSlot("Out", "", ScriptCanvas::SlotType::LatentOut);
    }

    void OnActivate() override
    {
        AsyncScriptCanvasTestFixture::m_asyncOperationActive = true;
        AZ::TickBus::Handler::BusConnect();
        AsyncEventNotificationBus::Handler::BusConnect(GetEntityId());

        std::packaged_task<void()> task([this]() { LongRunningProcessSimulator3000::Run(GetEntityId()); }); // wrap the function
        std::thread(std::move(task)).detach(); // launch on a thread
    }

    void OnDeactivate() override
    {
        AZ::TickBus::Handler::BusDisconnect();
    }

    virtual void HandleAsyncEvent()
    {
        EXPECT_GT(m_duration, 0.f);

        Shutdown();
    }

    void OnAsyncEvent() override
    {
        HandleAsyncEvent();
    }

    void Shutdown()
    {
        // We've received the event, no longer need the bus connection
        AsyncEventNotificationBus::Handler::BusDisconnect();

        // We're done, kick it out.
        SignalOutput(GetSlotId("Out"));

        // Disconnect from tick bus as well
        AZ::TickBus::Handler::BusDisconnect();

        AsyncScriptCanvasTestFixture::m_asyncOperationActive = false;
    }

    void OnTick(float deltaTime, AZ::ScriptTimePoint) override
    {
        AZ_TracePrintf("Debug", "Awaiting async operation: %.2f\n", m_duration);
        m_duration += deltaTime;
    }

    void Visit(ScriptCanvas::NodeVisitor& visitor) const override { visitor.Visit(*this); }
private:

    double m_duration = 0.f;
};

TEST_F(AsyncScriptCanvasTestFixture, Asynchronous_Behaviors)
{
    RETURN_IF_TEST_BODIES_ARE_DISABLED(TEST_BODY_DEFAULT);

    using namespace ScriptCanvas;

    AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, AsyncNode::CreateDescriptor());

    // Make the graph.
    Graph* graph = nullptr;
    SystemRequestBus::BroadcastResult(graph, &SystemRequests::MakeGraph);
    EXPECT_TRUE(graph != nullptr);

    AZ::Entity* graphEntity = graph->GetEntity();
    graphEntity->Init();

    const AZ::EntityId& graphEntityId = graph->GetEntityId();
    const AZ::EntityId& graphUniqueId = graph->GetUniqueId();

    AZ::EntityId startNodeId;
    Nodes::Core::Start* startNode = CreateTestNode<Nodes::Core::Start>(graphUniqueId, startNodeId);

    AZ::EntityId asyncNodeId;
    AsyncNode* asyncNode = CreateTestNode<AsyncNode>(graphUniqueId, asyncNodeId);

    EXPECT_TRUE(Connect(*graph, startNodeId, "Out", asyncNodeId, "In"));
    
    {
        ScopedOutputSuppression supressOutput;
        graphEntity->Activate();

        // Tick the TickBus while the graph entity is active
        while (AsyncScriptCanvasTestFixture::m_asyncOperationActive)
        {
            AZ::TickBus::ExecuteQueuedEvents();
            AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(100));
            AZ::TickBus::Broadcast(&AZ::TickEvents::OnTick, 0.01f, AZ::ScriptTimePoint(AZStd::chrono::system_clock::now()));
        }
    }

    graphEntity->Deactivate();
    delete graphEntity;

    AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, AsyncNode::CreateDescriptor());
}

///////////////////////////////////////////////////////////////////////////////

namespace
{
    // Fibonacci solver, used to compare against the graph version.
    long ComputeFibonacci(int digits)
    {
        int a = 0;
        int b = 1;
        long sum = 0;
        for (int i = 0; i < digits - 2; ++i)
        {
            sum = a + b;
            a = b;
            b = sum;
        }
        return sum;
    }
}

class AsyncFibonacciComputeNode
    : public AsyncNode
{
public:
    AZ_COMPONENT(AsyncFibonacciComputeNode, "{B198F52D-708C-414B-BB90-DFF0462D7F03}", AsyncNode);

    AsyncFibonacciComputeNode()
        : AsyncNode()
    {}

    bool IsEntryPoint() const { return true; }

    static const int k_numberOfFibonacciDigits = 64;

    static void Reflect(AZ::ReflectContext* reflection)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(reflection);
        if (serializeContext)
        {
            serializeContext->Class<AsyncFibonacciComputeNode, AsyncNode>()
                ->Version(1)
                ;
        }
    }

    void OnActivate() override
    {
        AZ::TickBus::Handler::BusConnect();
        AsyncEventNotificationBus::Handler::BusConnect(GetEntityId());

        int digits = k_numberOfFibonacciDigits;

        std::promise<long> p;
        m_computeFuture = p.get_future();
        std::thread([this, digits](std::promise<long> p)
        {
            p.set_value(ComputeFibonacci(digits));
            AsyncEventNotificationBus::Event(GetEntityId(), &AsyncEvent::OnAsyncEvent);
        }, AZStd::move(p)).detach();
    }

    void OnDeactivate() override
    {
        AZ::TickBus::Handler::BusDisconnect();
    }

    void HandleAsyncEvent() override
    {
        m_result = m_computeFuture.get();

        EXPECT_EQ(m_result, ComputeFibonacci(k_numberOfFibonacciDigits));
    }

    void OnTick(float deltaTime, AZ::ScriptTimePoint) override
    {
        AZ_TracePrintf("Debug", "Awaiting async fib operation: %.2f\n", m_duration);
        m_duration += deltaTime;

        if (m_result != 0)
        {
            Shutdown();
        }
    }

private:

    std::future<long> m_computeFuture;
    long m_result = 0;
    double m_duration = 0.f;
};

TEST_F(AsyncScriptCanvasTestFixture, ComputeFibonacciAsyncGraphTest)
{
    RETURN_IF_TEST_BODIES_ARE_DISABLED(TEST_BODY_DEFAULT);

    using namespace ScriptCanvas;

    AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::RegisterComponentDescriptor, AsyncFibonacciComputeNode::CreateDescriptor());

    // Make the graph.
    Graph* graph = nullptr;
    SystemRequestBus::BroadcastResult(graph, &SystemRequests::MakeGraph);
    EXPECT_TRUE(graph != nullptr);

    AZ::Entity* graphEntity = graph->GetEntity();
    graphEntity->Init();

    const AZ::EntityId& graphEntityId = graph->GetEntityId();
    const AZ::EntityId& graphUniqueId = graph->GetUniqueId();

    AZ::EntityId startNodeId;
    Nodes::Core::Start* startNode = CreateTestNode<Nodes::Core::Start>(graphUniqueId, startNodeId);

    AZ::EntityId asyncNodeId;
    AsyncFibonacciComputeNode* asyncNode = CreateTestNode<AsyncFibonacciComputeNode>(graphUniqueId, asyncNodeId);

    EXPECT_TRUE(Connect(*graph, startNodeId, "Out", asyncNodeId, "In"));

    graphEntity->Activate();

    // Tick the TickBus while the graph entity is active
    while (AsyncScriptCanvasTestFixture::m_asyncOperationActive)
    {
        AZ::TickBus::ExecuteQueuedEvents();
        AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(100));
        AZ::TickBus::Broadcast(&AZ::TickEvents::OnTick, 0.01f, AZ::ScriptTimePoint(AZStd::chrono::system_clock::now()));
    }

    graphEntity->Deactivate();
    delete graphEntity;

    AZ::ComponentApplicationBus::Broadcast(&AZ::ComponentApplicationRequests::UnregisterComponentDescriptor, AsyncFibonacciComputeNode::CreateDescriptor());
}

#endif // AZ_COMPILER_MSVC >= 1900
