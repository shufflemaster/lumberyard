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

#include <PhysX_precompiled.h>

#include <AzTest/AzTest.h>
#include <Physics/PhysicsTests.h>
#include <Tests/PhysXTestCommon.h>

#include <AzCore/Math/Random.h>
#include <AzCore/std/parallel/thread.h>

#include <AzFramework/Physics/RigidBodyBus.h>
#include <AzFramework/Physics/ShapeConfiguration.h>

#ifdef PHYSX_ENABLE_MULTI_THREADING

//enable this define to enable some logs for debugging
//#define PHYSX_MT_DEBUG_LOGS
#ifdef PHYSX_MT_DEBUG_LOGS
#define Log_Help(window, message, ...) AZ_Printf(window, message, __VA_ARGS__);
#else
#define Log_Help(window, message, ...) ((void)0);
#endif //PHYSX_MT_DEBUG_LOGS

namespace PhysX
{
    namespace Constants
    {
        static const int NumThreads = 50; //number of threads to create and use for the tests

        //Entities to raycast / shapecast / Overlap against
        static const AZ::Vector3 BoxDimensions = AZ::Vector3::CreateOne();
        static const int NumBoxes = 18;
        AZStd::array<AZ::Vector3, NumBoxes> BoxPositions = {
            AZ::Vector3( 1000.0f,  1000.0f,     0.0f),
            AZ::Vector3(-1000.0f, -1000.0f,     0.0f),
            AZ::Vector3( 1000.0f, -1000.0f,     0.0f),
            AZ::Vector3(-1000.0f,  1000.0f,     0.0f),
            AZ::Vector3( 1000.0f,     0.0f,  1000.0f),
            AZ::Vector3(-1000.0f,     0.0f, -1000.0f),
            AZ::Vector3( 1000.0f,     0.0f, -1000.0f),
            AZ::Vector3(-1000.0f,     0.0f,  1000.0f),
            AZ::Vector3(    0.0f,    10.0f,    10.0f),
            AZ::Vector3(    0.0f,   -10.0f,   -10.0f),
            AZ::Vector3(    0.0f,   -10.0f,    10.0f),
            AZ::Vector3(    0.0f,    10.0f,   -10.0f),
            AZ::Vector3( 100.0f,      0.0f,     0.0f),
            AZ::Vector3(-100.0f,      0.0f,     0.0f),
            AZ::Vector3(  0.0f,     100.0f,     0.0f),
            AZ::Vector3(  0.0f,    -100.0f,     0.0f),
            AZ::Vector3(  0.0f,       0.0f,    10.0f),
            AZ::Vector3(  0.0f,       0.0f,   -10.0f)
        };

        static const float ShpereShapeRadius = 2.0f;
    }

    namespace
    {
        void UpdateDefaultWorldOverTime(int updateTimeLimitMilliseconds)
        {
            const AZStd::chrono::milliseconds updateTimeLimit = AZStd::chrono::milliseconds(updateTimeLimitMilliseconds);
            AZStd::chrono::milliseconds totalTime = AZStd::chrono::milliseconds(0);
            do
            {
                AZStd::chrono::system_clock::time_point startTime = AZStd::chrono::system_clock::now();
                TestUtils::UpdateDefaultWorld(Physics::WorldConfiguration::s_defaultFixedTimeStep, 1);
                AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(1));
                totalTime += AZStd::chrono::system_clock::now() - startTime;
            } while (totalTime < updateTimeLimit);
        }
    }

    class PhysXMultithreadingTest
        : public Physics::GenericPhysicsInterfaceTest
        , public ::testing::WithParamInterface<int>
    {
    public:
        virtual void SetUp() override
        {
            Physics::GenericPhysicsInterfaceTest::SetUpInternal();

            //create some boxes
            for (int i = 0; i < Constants::NumBoxes; i++)
            {
                EntityPtr newEntity = TestUtils::CreateBoxEntity(Constants::BoxPositions[i], Constants::BoxDimensions, false);
                m_boxes.emplace_back(newEntity);

                //disable gravity so they don't move
                Physics::RigidBodyRequestBus::Event(newEntity->GetId(), &Physics::RigidBodyRequestBus::Events::SetGravityEnabled, false);
            }
        }

        virtual void TearDown() override
        {
            Physics::GenericPhysicsInterfaceTest::TearDownInternal();
        }

        AZStd::vector<EntityPtr> m_boxes;
    };

    template<typename RequestType, typename ResultType>
    struct SceneQueryBase
    {
        SceneQueryBase(const AZStd::thread_desc& threadDesc, const RequestType& request)
            : m_threadDesc(threadDesc)
            , m_request(request)
        {

        }

        virtual ~SceneQueryBase() = default;

        void Start(int waitTimeMilliseconds)
        {
            m_waitTimeMilliseconds = waitTimeMilliseconds;
            m_thread = AZStd::thread(AZStd::bind(&SceneQueryBase::Tick, this), &m_threadDesc);
        }

        void Join()
        {
            m_thread.join();
        }

        ResultType m_result;

    private:
        void Tick()
        {
            Log_Help(m_threadDesc.m_name, "Thread %d - sleeping for %dms\n", AZStd::this_thread::get_id(), m_waitTimeMilliseconds);
            AZStd::this_thread::sleep_for(AZStd::chrono::milliseconds(m_waitTimeMilliseconds));
            Log_Help(m_threadDesc.m_name, "Thread %d - running cast\n", AZStd::this_thread::get_id());
            AZStd::chrono::system_clock::time_point startTime = AZStd::chrono::system_clock::now();

            RunRequest();

            AZStd::chrono::microseconds exeTimeUS = AZStd::chrono::system_clock::now() - startTime;
            Log_Help(m_threadDesc.m_name, "Thread %d - complete - time %dus\n", AZStd::this_thread::get_id(), exeTimeUS.count());
        }

    protected:
        virtual void RunRequest() = 0;

        AZStd::thread m_thread;
        AZStd::thread_desc m_threadDesc;
        int m_waitTimeMilliseconds;
        RequestType m_request;
    };

    struct RayCaster
        : public SceneQueryBase<Physics::RayCastRequest, Physics::RayCastHit>
    {
        RayCaster(const AZStd::thread_desc& threadDesc, const Physics::RayCastRequest& request)
            : SceneQueryBase(threadDesc, request)
        {

        }

    private:
        void RunRequest() override
        {
            Physics::WorldRequestBus::BroadcastResult(m_result, &Physics::WorldRequests::RayCast, m_request);
        }
    };

    TEST_P(PhysXMultithreadingTest, RaycastsQueryFromParallelThreads)
    {
        const int seed = GetParam();

        //raycast thread pool
        AZStd::vector<AZStd::unique_ptr<RayCaster>> rayCasters;

        //common request data
        Physics::RayCastRequest request;
        request.m_start = AZ::Vector3::CreateZero();
        request.m_distance = 2000.0f;

        AZStd::thread_desc threadDesc;
        threadDesc.m_name = "RaycastsQueryFromParallelThreads";

        //create the raycasts
        for (int i = 0; i < Constants::NumThreads; i++)
        {
            //pick a box to raycast against
            const int boxTargetIdx = i % m_boxes.size();
            request.m_direction = Constants::BoxPositions[boxTargetIdx].GetNormalizedExact();

            rayCasters.emplace_back(AZStd::make_unique<RayCaster>(threadDesc, request));
        }

        //start all threads
        AZ::SimpleLcgRandom random(seed); //constant seed to have consistency.
        for (auto& caster : rayCasters)
        {
            const int waitTimeMS = aznumeric_cast<int>((random.GetRandomFloat() + 0.25f) * 250.0f); //generate a time between 62.5 - 312.5 ms
            caster->Start(waitTimeMS);
        }

        //update the world
        Log_Help("RaycastsQueryFromParallelThreads", "Start world Update\n");
        UpdateDefaultWorldOverTime(500);
        Log_Help("RaycastsQueryFromParallelThreads", "End world Update\n");

        //each request should have completed, join to be sure. and each request should have a result.
        int i = 0;
        for (auto& caster : rayCasters)
        {
            caster->Join();

            const int boxTargetIdx = i % m_boxes.size();
            EXPECT_TRUE(caster->m_result.m_body != nullptr);
            EXPECT_TRUE(caster->m_result.m_body->GetEntityId() == m_boxes[boxTargetIdx]->GetId());

            caster.release();
            i++;
        }
        rayCasters.clear();
    }

    struct RayCasterMultiple
        : public SceneQueryBase<Physics::RayCastRequest, AZStd::vector<Physics::RayCastHit>>
    {
        RayCasterMultiple(const AZStd::thread_desc& threadDesc, const Physics::RayCastRequest& request)
            : SceneQueryBase(threadDesc, request)
        {

        }

    private:
        void RunRequest() override
        {
            Physics::WorldRequestBus::BroadcastResult(m_result, &Physics::WorldRequests::RayCastMultiple, m_request);
        }
    };

    TEST_P(PhysXMultithreadingTest, RaycastMultiplesQueryFromParallelThreads)
    {
        const int seed = GetParam();

        //raycast thread pool
        AZStd::vector<AZStd::unique_ptr<RayCasterMultiple>> rayCasters;

        //common request data
        Physics::RayCastRequest request;
        request.m_start = AZ::Vector3::CreateZero();
        request.m_distance = 2000.0f;
        
        AZStd::thread_desc threadDesc;
        threadDesc.m_name = "RaycastMultiplesQueryFromParallelThreads";

        //create the raycasts
        for (int i = 0; i < Constants::NumThreads; i++)
        {
            //pick a box to raycast against
            const int boxTargetIdx = i % m_boxes.size();
            request.m_direction = Constants::BoxPositions[boxTargetIdx].GetNormalizedExact();

            rayCasters.emplace_back(AZStd::make_unique<RayCasterMultiple>(threadDesc, request));
        }

        //start all threads
        AZ::SimpleLcgRandom random(seed);
        for (auto& caster : rayCasters)
        {
            const int waitTimeMS = aznumeric_cast<int>((random.GetRandomFloat() + 0.25f) * 250.0f); //generate a time between 62.5 - 312.5 ms
            caster->Start(waitTimeMS);
        }

        //update the world
        Log_Help("RaycastMultiplesQueryFromParallelThreads", "Start world Update\n");
        UpdateDefaultWorldOverTime(500);
        Log_Help("RaycastMultiplesQueryFromParallelThreads", "End world Update\n");

        //each request should have completed, join to be sure. and each request should have a result.
        int i = 0;
        for (auto& caster : rayCasters)
        {
            caster->Join();
            //we should have some results
            EXPECT_TRUE(caster->m_result.size() > 0);

            //check the list of result for the target
            bool targetInList = false;
            const int boxTargetIdx = i % m_boxes.size();
            for (auto& hit : caster->m_result)
            {
                if (hit.m_body)
                {
                    if (hit.m_body->GetEntityId() == m_boxes[boxTargetIdx]->GetId())
                    {
                        targetInList = true;
                        break;
                    }
                }
            }
            EXPECT_TRUE(targetInList);

            caster.release();
            i++;
        }
        rayCasters.clear();
    }

     struct ShapeCaster
         : public SceneQueryBase<Physics::ShapeCastRequest, Physics::RayCastHit>
     {
         ShapeCaster(const AZStd::thread_desc& threadDesc, const Physics::ShapeCastRequest& request)
             : SceneQueryBase(threadDesc, request)
         {
 
         }

     private:
         void RunRequest() override
         {
             Physics::WorldRequestBus::BroadcastResult(m_result, &Physics::WorldRequests::ShapeCast, m_request);
         }
     };
 
     TEST_P(PhysXMultithreadingTest, ShapeCastsQueryFromParallelThreads)
     {
         const int seed = GetParam();

         //shapecast thread pool
         AZStd::vector<AZStd::unique_ptr<ShapeCaster>> shapeCasters;

         //common request data
         Physics::SphereShapeConfiguration shapeConfiguration;
         shapeConfiguration.m_radius = Constants::ShpereShapeRadius;

         Physics::ShapeCastRequest request;
         request.m_start = AZ::Transform::CreateIdentity();
         request.m_distance = 2000.0f;
         request.m_shapeConfiguration = &shapeConfiguration;

         AZStd::thread_desc threadDesc;
         threadDesc.m_name = "ShapeCastsQueryFromParallelThreads";

         //create the raycasts
         for (int i = 0; i < Constants::NumThreads; i++)
         {
             //pick a box to raycast against
             const int boxTargetIdx = i % m_boxes.size();
             request.m_direction = Constants::BoxPositions[boxTargetIdx].GetNormalizedExact();

             shapeCasters.emplace_back(AZStd::make_unique<ShapeCaster>(threadDesc, request));
         }

         //start all threads
         AZ::SimpleLcgRandom random(seed); //constant seed to have consistency.
         for (auto& caster : shapeCasters)
         {
             const int waitTimeMS = aznumeric_cast<int>((random.GetRandomFloat() + 0.25f) * 250.0f); //generate a time between 62.5 - 312.5 ms
             caster->Start(waitTimeMS);
         }

         //update the world
         Log_Help("ShapeCastsQueryFromParallelThreads", "Start world Update\n");
         UpdateDefaultWorldOverTime(500);
         Log_Help("ShapeCastsQueryFromParallelThreads", "End world Update\n");

         //each request should have completed, join to be sure. and each request should have a result.
         int i = 0;
         for (auto& caster : shapeCasters)
         {
             caster->Join();

             const int boxTargetIdx = i % m_boxes.size();
             EXPECT_TRUE(caster->m_result.m_body != nullptr);
             EXPECT_TRUE(caster->m_result.m_body->GetEntityId() == m_boxes[boxTargetIdx]->GetId());

             caster.release();
             i++;
         }
         shapeCasters.clear();
     }

     struct ShapeCasterMultiple
         : public SceneQueryBase<Physics::ShapeCastRequest, AZStd::vector<Physics::RayCastHit>>
     {
         ShapeCasterMultiple(const AZStd::thread_desc& threadDesc, const Physics::ShapeCastRequest& request)
             : SceneQueryBase(threadDesc, request)
         {

         }

     private:
         void RunRequest() override
         {
             Physics::WorldRequestBus::BroadcastResult(m_result, &Physics::WorldRequests::ShapeCastMultiple, m_request);
         }
     };

     TEST_P(PhysXMultithreadingTest, ShapeCastMultiplesQueryFromParallelThreads)
     {
         const int seed = GetParam();

         //shapecast thread pool
         AZStd::vector<AZStd::unique_ptr<ShapeCasterMultiple>> shapeCasters;

         //common request data
         Physics::SphereShapeConfiguration shapeConfiguration;
         shapeConfiguration.m_radius = Constants::ShpereShapeRadius;

         Physics::ShapeCastRequest request;
         request.m_distance = 2000.0f;
         request.m_shapeConfiguration = &shapeConfiguration;

         AZStd::thread_desc threadDesc;
         threadDesc.m_name = "ShapeCastMultiplesQueryFromParallelThreads";

         //create the shape casts
         for (int i = 0; i < Constants::NumThreads; i++)
         {
             //pick a box
             const int boxTargetIdx = i % m_boxes.size();
             request.m_direction = Constants::BoxPositions[boxTargetIdx].GetNormalizedExact();

             shapeCasters.emplace_back(AZStd::make_unique<ShapeCasterMultiple>(threadDesc, request));
         }

         //start all threads
         AZ::SimpleLcgRandom random(seed); //constant seed to have consistency.
         for (auto& caster : shapeCasters)
         {
             const int waitTimeMS = aznumeric_cast<int>((random.GetRandomFloat() + 0.25f) * 250.0f); //generate a time between 62.5 - 312.5 ms
             caster->Start(waitTimeMS);
         }

         //update the world
         Log_Help("ShapeCastMultiplesQueryFromParallelThreads", "Start world Update\n");
         UpdateDefaultWorldOverTime(500);
         Log_Help("ShapeCastMultiplesQueryFromParallelThreads", "End world Update\n");

         //each request should have completed, join to be sure. and each request should have a result.
         int i = 0;
         for (auto& caster : shapeCasters)
         {
             caster->Join();
             //we should have some results
             EXPECT_TRUE(caster->m_result.size() > 0);

             //check the list of result for the target
             bool targetInList = false;
             const int boxTargetIdx = i % m_boxes.size();
             for (auto& hit : caster->m_result)
             {
                 if (hit.m_body)
                 {
                     if (hit.m_body->GetEntityId() == m_boxes[boxTargetIdx]->GetId())
                     {
                         targetInList = true;
                         break;
                     }
                 }
             }
             EXPECT_TRUE(targetInList);

             caster.release();
             i++;
         }
         shapeCasters.clear();
     }

     struct OverlapQuery
         : public SceneQueryBase<Physics::OverlapRequest, AZStd::vector<Physics::OverlapHit>>
     {
         OverlapQuery(const AZStd::thread_desc& threadDesc, const Physics::OverlapRequest& request)
             : SceneQueryBase(threadDesc, request)
         {

         }

     private:
         void RunRequest() override
         {
             Physics::WorldRequestBus::BroadcastResult(m_result, &Physics::WorldRequests::Overlap, m_request);
         }
     };

     TEST_P(PhysXMultithreadingTest, OverlapQueryFromParallelThreads)
     {
         const int seed = GetParam();

         //Overlap thread pool
         AZStd::vector<AZStd::unique_ptr<OverlapQuery>> overlapQuery;

         //common request data
         Physics::SphereShapeConfiguration shapeConfiguration;
         shapeConfiguration.m_radius = Constants::ShpereShapeRadius;

         Physics::OverlapRequest request;
         request.m_shapeConfiguration = &shapeConfiguration;

         AZStd::thread_desc threadDesc;
         threadDesc.m_name = "OverlapQueryFromParallelThreads";

         //create the overlap request
         for (int i = 0; i < Constants::NumThreads; i++)
         {
             //pick a box
             const int boxTargetIdx = i % m_boxes.size();
             request.m_pose = AZ::Transform::CreateTranslation(Constants::BoxPositions[boxTargetIdx]);

             overlapQuery.emplace_back(AZStd::make_unique<OverlapQuery>(threadDesc, request));
         }

         //start all threads
         AZ::SimpleLcgRandom random(seed); //constant seed to have consistency.
         for (auto& caster : overlapQuery)
         {
             const int waitTimeMS = aznumeric_cast<int>((random.GetRandomFloat() + 0.25f) * 250.0f); //generate a time between 62.5 - 312.5 ms
             caster->Start(waitTimeMS);
         }

         //update the world
         Log_Help("OverlapQueryFromParallelThreads", "Start world Update\n");
         UpdateDefaultWorldOverTime(500);
         Log_Help("OverlapQueryFromParallelThreads", "End world Update\n");

         //each request should have completed, join to be sure. and each request should have a result.
         int i = 0;
         for (auto& caster : overlapQuery)
         {
             caster->Join();
             //we should have some results
             EXPECT_TRUE(caster->m_result.size() > 0);

             //check the list of result for the target
             bool targetInList = false;
             const int boxTargetIdx = i % m_boxes.size();
             for (auto& hit : caster->m_result)
             {
                 if (hit.m_body)
                 {
                     if (hit.m_body->GetEntityId() == m_boxes[boxTargetIdx]->GetId())
                     {
                         targetInList = true;
                         break;
                     }
                 }
             }
             EXPECT_TRUE(targetInList);

             caster.release();
             i++;
         }
         overlapQuery.clear();
     }

    INSTANTIATE_TEST_CASE_P(PhysXMultithreading, PhysXMultithreadingTest, ::testing::Values(1, 42, 123, 1337, 1403, 5317, 133987258));
}

#ifdef PHYSX_MT_DEBUG_LOGS
#undef PHYSX_MT_DEBUG_LOGS
#undef Log_Help
#endif //PHYSX_MT_DEBUG_LOGS


#endif //PHYSX_ENABLE_MULTI_THREADING
