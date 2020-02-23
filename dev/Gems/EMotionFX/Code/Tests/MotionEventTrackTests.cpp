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

#include "SystemComponentFixture.h"
#include "TestAssetCode/MotionEvent.h"
#include <EMotionFX/Source/EventManager.h>
#include <EMotionFX/Source/EventHandler.h>
#include <EMotionFX/Source/MotionEventTable.h>
#include <EMotionFX/Source/MotionEventTrack.h>
#include <EMotionFX/Source/MotionManager.h>
#include <EMotionFX/Source/SkeletalMotion.h>
#include <EMotionFX/Source/Node.h>
#include <EMotionFX/Source/TwoStringEventData.h>
#include <Integration/System/SystemCommon.h>

#include <AzCore/std/algorithm.h>

namespace EMotionFX
{
    struct ExtractEventsParams
    {
        void (*eventFactory)(MotionEventTrack* track);
        float startTime;
        float endTime;
        EPlayMode playMode;
        std::vector<EventInfo> expectedEvents;
    };

    void PrintTo(const EMotionFX::EventInfo::EventState& state, ::std::ostream* os)
    {
        if (state == EMotionFX::EventInfo::EventState::START)
        {
            *os << "EMotionFX::EventInfo::EventState::START";
        }
        else if (state == EMotionFX::EventInfo::EventState::ACTIVE)
        {
            *os << "EMotionFX::EventInfo::EventState::ACTIVE";
        }
        else if (state == EMotionFX::EventInfo::EventState::END)
        {
            *os << "EMotionFX::EventInfo::EventState::END";
        }
    }

    void PrintTo(const EMotionFX::EventInfo& event, ::std::ostream* os)
    {
        *os << "Time: " << event.mTimeValue
            << " State: "
            ;
        PrintTo(event.m_eventState, os);
    }

    void PrintTo(const ExtractEventsParams& object, ::std::ostream* os)
    {
        if (object.eventFactory == &MakeNoEvents)
        {
            *os << "Events: 0";
        }
        else if (object.eventFactory == &MakeOneEvent)
        {
            *os << "Events: 1";
        }
        else if (object.eventFactory == &MakeTwoEvents)
        {
            *os << "Events: 2";
        }
        else if (object.eventFactory == &MakeThreeEvents)
        {
            *os << "Events: 3";
        }
        else if (object.eventFactory == &MakeThreeRangedEvents)
        {
            *os << "Events: 3 (ranged)";
        }
        else
        {
            *os << "Events: Unknown";
        }
        *os << " Start time: " << object.startTime
            << " End time: " << object.endTime
            << " Play mode: " << ((object.playMode == EPlayMode::PLAYMODE_FORWARD) ? "Forward" : "Backward")
            << " Expected events: ["
            ;
        for (const auto& entry : object.expectedEvents)
        {
            PrintTo(entry, os);
            if (&entry != &(*(object.expectedEvents.end() - 1)))
            {
                *os << ", ";
            }
        }
        *os << ']';
    }

    // This fixture is used for both MotionEventTrack::ProcessEvents and
    // MotionEventTrack::ExtractEvents. Both calls should have similar results,
    // with the exception that ProcessEvents filters out events whose state is
    // ACTIVE.
    class TestExtractProcessEventsFixture
        : public SystemComponentFixture
        , public ::testing::WithParamInterface<ExtractEventsParams>
    {
        // This event handler exists to capture events and put them in a
        // AnimGraphEventBuffer, so that the ExtractEvents and ProcessEvents
        // tests can test the results in a similar fashion
        class TestProcessEventsEventHandler
            : public EMotionFX::EventHandler
        {
        public:
            AZ_CLASS_ALLOCATOR(TestProcessEventsEventHandler, Integration::EMotionFXAllocator, 0);

            TestProcessEventsEventHandler(AnimGraphEventBuffer* buffer)
                : m_buffer(buffer)
            {
            }

            virtual const AZStd::vector<EventTypes> GetHandledEventTypes() const
            {
                return { EVENT_TYPE_ON_EVENT };
            }

            void OnEvent(const EMotionFX::EventInfo& emfxInfo) override
            {
                m_buffer->AddEvent(emfxInfo);
            }

        private:
            AnimGraphEventBuffer* m_buffer;
        };

    public:
        void SetUp() override
        {
            SystemComponentFixture::SetUp();

            m_motion = SkeletalMotion::Create("TestExtractEventsMotion");
            m_motion->SetMaxTime(2.0f);
            m_motion->GetEventTable()->AutoCreateSyncTrack(m_motion);
            m_track = m_motion->GetEventTable()->GetSyncTrack();

            GetParam().eventFactory(m_track);

            m_actor = Actor::Create("testActor");
            Node* rootNode = Node::Create("rootNode", m_actor->GetSkeleton());
            m_actor->AddNode(rootNode);
            m_actor->ResizeTransformData();
            m_actor->PostCreateInit(/*makeGeomLodsCompatibleWithSkeletalLODs=*/false, /*generateOBBs=*/false, /*convertUnitType=*/false);

            m_actorInstance = ActorInstance::Create(m_actor);

            m_motionInstance = MotionInstance::Create(m_motion, m_actorInstance, 0);

            m_buffer = new AnimGraphEventBuffer();

            m_eventHandler = aznew TestProcessEventsEventHandler(m_buffer);
            GetEMotionFX().GetEventManager()->AddEventHandler(m_eventHandler);
        }

        void TearDown() override
        {
            GetEMotionFX().GetEventManager()->RemoveEventHandler(m_eventHandler);
            delete m_buffer;
            delete m_eventHandler;
            m_motionInstance->Destroy();
            m_motion->Destroy();
            m_actorInstance->Destroy();
            m_actor->Destroy();
            SystemComponentFixture::TearDown();
        }

        void TestEvents(AZStd::function<void(float, float, EPlayMode playMode, MotionInstance*)> func)
        {
            const ExtractEventsParams& params = GetParam();

            // Call the function being tested
            func(params.startTime, params.endTime, params.playMode, m_motionInstance);

            // ProcessEvents filters out the ACTIVE events, remove those from our expected results
            AZStd::vector<EventInfo> expectedEvents;
            for (const EventInfo& event : params.expectedEvents)
            {
                if (event.m_eventState != EventInfo::ACTIVE || m_shouldContainActiveEvents)
                {
                    expectedEvents.emplace_back(event);
                }
            }

            EXPECT_EQ(m_buffer->GetNumEvents(), expectedEvents.size()) << "Number of events is incorrect";
            for (uint32 i = 0; i < AZStd::min(m_buffer->GetNumEvents(), static_cast<uint32>(expectedEvents.size())); ++i)
            {
                const EventInfo& gotEvent = m_buffer->GetEvent(i);
                const EventInfo& expectedEvent = expectedEvents[i];
                EXPECT_EQ(gotEvent.mTimeValue, expectedEvent.mTimeValue);
                EXPECT_EQ(gotEvent.m_eventState, expectedEvent.m_eventState);
            }
        }

    protected:
        AnimGraphEventBuffer* m_buffer = nullptr;
        SkeletalMotion* m_motion = nullptr;
        MotionInstance* m_motionInstance = nullptr;
        MotionEventTrack* m_track = nullptr;
        Actor* m_actor = nullptr;
        ActorInstance* m_actorInstance = nullptr;
        TestProcessEventsEventHandler* m_eventHandler = nullptr;

        // ProcessEvents filters out ACTIVE events. For the ProcessEvents
        // tests, this will be set to false.
        bool m_shouldContainActiveEvents;
    };

    TEST_P(TestExtractProcessEventsFixture, TestExtractEvents)
    {
        m_shouldContainActiveEvents = true;
        TestEvents([this](float startTime, float endTime, EPlayMode playMode, MotionInstance* motionInstance)
        {
            motionInstance->SetPlayMode(playMode);
            return this->m_track->ExtractEvents(startTime, endTime, motionInstance, m_buffer);
        });
    }

    TEST_P(TestExtractProcessEventsFixture, TestProcessEvents)
    {
        m_shouldContainActiveEvents = false;
        TestEvents([this](float startTime, float endTime, EPlayMode playMode, MotionInstance* motionInstance)
        {
            motionInstance->SetPlayMode(playMode);
            return this->m_track->ProcessEvents(startTime, endTime, motionInstance);
        });
    }

    std::vector<ExtractEventsParams> extractEventTestData {
        {
            MakeThreeEvents,
            0.0f,
            1.0f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            MakeThreeEvents,
            0.0f,
            1.5f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    1.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Processing from before a ranged event begins to the middle of
            // that event should give a start event and an active event
            MakeThreeRangedEvents,
            0.0f,
            0.3f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Processing from before a ranged event begins to after the end of
            // that event should give a start event and an end event
            MakeThreeRangedEvents,
            0.0f,
            0.6f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.5f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Processing from the middle of a ranged event to after the end of
            // that event should give just an end event
            MakeThreeRangedEvents,
            0.3f,
            0.6f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.5f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Each ranged event processed whose start time is traversed
            // generates 2 event infos
            MakeThreeRangedEvents,
            0.0f,
            0.9f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.5f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        // Now the backwards playback cases
        {
            MakeThreeEvents,
            1.0f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            MakeThreeEvents,
            1.5f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    1.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Processing from the middle of a ranged event to before that
            // event begins should give an end event
            MakeThreeRangedEvents,
            0.3f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Processing from after a ranged event ends to before the
            // beginning of that event should give a start event and an end
            // event
            MakeThreeRangedEvents,
            0.6f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.5f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Processing from after the end of an event to the middle of a
            // ranged event should give a start event and an active event
            MakeThreeRangedEvents,
            0.6f,
            0.3f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.5f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Start in the middle of a ranged event while playing backwards
            MakeThreeRangedEvents,
            0.9f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                },
                EventInfo {
                    0.5f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Loop, but in a way where no events should be triggered.
            MakeTwoEvents,
            1.9f,
            0.1f,
            EPlayMode::PLAYMODE_FORWARD,
            {
            }
        },
        {
            // Loop, but in a way where no events should be triggered, but play backward.
            MakeTwoEvents,
            0.1f,
            1.9f,
            EPlayMode::PLAYMODE_BACKWARD,
            {
            }
        },
        {
            // Loop, forward, and overlap one event.
            MakeTwoEvents,
            1.9f,
            0.5f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Loop, backwards, and overlap one event.
            MakeTwoEvents,
            0.5f,
            1.9f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Loop, forward, and overlap two events.
            MakeTwoEvents,
            1.9f,
            1.0f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Loop, backwards, and overlap two events.
            MakeTwoEvents,
            1.0f,
            1.9f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Start exactly at a given motion event's time value.
            MakeTwoEvents,
            0.25f,
            0.3f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // End exactly at a given motion event's time value.
            MakeTwoEvents,
            0.0f,
            0.25f,
            EPlayMode::PLAYMODE_FORWARD,
            {
            }
        },
        {
            // Double check both cases at the same time.
            MakeTwoEvents,
            0.25f,
            0.75f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Start exactly at a given motion event's time value.
            // Playing backward.
            MakeTwoEvents,
            0.25f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // End exactly at a given motion event's time value.
            // Playing backward.
            MakeTwoEvents,
            0.5f,
            0.25f,
            EPlayMode::PLAYMODE_BACKWARD,
            {
            }
        },
        {
            // Double check both cases at the same time.
            // Playing backward.
            MakeTwoEvents,
            0.75f,
            0.25f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Start exactly at a given motion event's time value.
            MakeOneRangedEvent,
            0.25f,
            0.75f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // End exactly at a given motion event's time value.
            MakeOneRangedEvent,
            0.0f,
            0.25f,
            EPlayMode::PLAYMODE_FORWARD,
            {
            }
        },
        {
            // Double check both cases at the same time.
            MakeOneRangedEvent,
            0.25f,
            0.75f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                }
            }
        },
        {
            // Start exactly at a given motion event's time value.
            // Playing backward.
            MakeOneRangedEvent,
            0.25f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END // Originally the start, but in backward it turns into end.
                }
            }
        },
        {
            // End exactly at a given motion event's time value.
            // Playing backward.
            MakeOneRangedEvent,
            0.5f,
            0.25f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::ACTIVE
                }
            },
        },
        {
            // Double check both cases at the same time.
            // Playing backward.
            MakeOneRangedEvent,
            0.75f,
            0.25f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START // End became start in backward playback.
                }
            }
        },
        {
            // Process the full motion in one go.
            MakeOneRangedEvent,
            0.0f,
            2.0f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Reverse it, processin the whole motion.
            MakeOneRangedEvent,
            2.0f,
            0.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START // Event end became start, because of backward playback.
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END // Start became end, because of backward playback.
                }
            }
        },
        {
            // Use a time delta that is 5x as large as the motion.
            // NOTE: wrapping isn't supported at this time, so we expect it to just act like all events will be emitted once.
            MakeOneRangedEvent,
            0.0f,
            10.0f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Use some negative time delta, fitting the motion 5x, in reverse.
            // NOTE: wrapping isn't supported at this time, so we expect it to just act like all events will be emitted once.
            MakeOneRangedEvent,
            2.0f,
            -10.0f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START // Event end became start, because of backward playback.
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END // Start became end, because of backward playback.
                }
            }
        },
        {
            // Play longer than the motion duration and if wrapping is supported, end up half way in the range event.
            // NOTE: wrapping isn't supported at this time, so we expect it to just act like all events will be emitted once.
            MakeOneRangedEvent,
            0.0f,
            2.5f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START
                },
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Use some negative time delta, fitting the motion 5x, in reverse.
            // NOTE: wrapping isn't supported at this time, so we expect it to just act like all events will be emitted once.
            MakeOneRangedEvent,
            2.0f,
            -1.5f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::START // Event end became start, because of backward playback.
                },
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END // Start became end, because of backward playback.
                }
            }
        },
        {
            // Play longer than the motion duration and if wrapping is supported, end up half way in the range event.
            // NOTE: wrapping isn't supported at this time, so we expect it to just act like all events will be emitted once.
            MakeOneRangedEvent,
            0.5f,
            2.5f,
            EPlayMode::PLAYMODE_FORWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.75f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END
                }
            }
        },
        {
            // Use some negative time delta, fitting the motion 5x, in reverse.
            // NOTE: wrapping isn't supported at this time, so we expect it to just act like all events will be emitted once.
            MakeOneRangedEvent,
            0.5f,
            -1.5f,
            EPlayMode::PLAYMODE_BACKWARD,
            std::vector<EventInfo> {
                EventInfo {
                    0.25f,
                    nullptr, nullptr, nullptr,
                    EMotionFX::EventInfo::EventState::END // Start became end, because of backward playback.
                }
            }
        }
    };

    INSTANTIATE_TEST_CASE_P(TestExtractProcessEvents, TestExtractProcessEventsFixture,
        ::testing::ValuesIn(extractEventTestData));
} // end namespace EMotionFX
