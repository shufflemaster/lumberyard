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

#include "TimeDataStatisticsManager.h"

namespace AzFramework
{
    namespace Statistics
    {
        void TimeDataStatisticsManager::PushTimeDataSample(const char * registerName, const AZ::Debug::ProfilerRegister::TimeData& timeData)
        {
            const AZStd::string statName(registerName);
            NamedRunningStatistic* statistic = GetStatistic(statName);
            if (!statistic)
            {
                const AZStd::string units("us");
                AddStatistic(statName, statName, units, false);
                AZ::Debug::ProfilerRegister::TimeData zeroTimeData;
                memset(&zeroTimeData, 0, sizeof(AZ::Debug::ProfilerRegister::TimeData));
                m_previousTimeData[statName] = zeroTimeData;
                statistic = GetStatistic(statName);
                AZ_Assert(statistic != nullptr, "Fatal error adding a new statistic object");
            }

            const AZ::u64 accumulatedTime = timeData.m_time;
            const AZ::s64 totalNumCalls = timeData.m_calls;
            const AZ::u64 previousAccumulatedTime = m_previousTimeData[statName].m_time;
            const AZ::s64 previousTotalNumCalls = m_previousTimeData[statName].m_calls;
            const AZ::u64 deltaTime = accumulatedTime - previousAccumulatedTime;
            const AZ::s64 deltaCalls = totalNumCalls - previousTotalNumCalls;
            
            if (deltaCalls == 0)
            {
                //This is the same old data. Let's skip it
                return;
            }

            double newSample = static_cast<double>(deltaTime) / deltaCalls;

            statistic->PushSample(newSample);
            m_previousTimeData[statName] = timeData;
        }
    } //namespace Statistics
} //namespace AzFramework
