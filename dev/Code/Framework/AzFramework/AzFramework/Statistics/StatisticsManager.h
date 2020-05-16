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

#include <AzCore/std/containers/vector.h>
#include <AzCore/std/containers/unordered_map.h>

#include "NamedRunningStatistic.h"

namespace AzFramework
{
    namespace Statistics
    {
        /**
         * @brief A Collection of Running Statistics, addressable by a hashable
         *        class/primitive. e.g. AZ::Crc32, int, AZStd::string, etc.
         *
         */
        template <class StatIdType = AZStd::string>
        class StatisticsManager
        {
        public:
            StatisticsManager() = default;

            StatisticsManager(const StatisticsManager& other)
            {
                m_statistics.reserve(other.m_statistics.size());
                for (auto const& it : other.m_statistics)
                {
                    const StatIdType& statId = it.first;
                    const NamedRunningStatistic* stat = it.second;
                    m_statistics[statId] = new NamedRunningStatistic(*stat);
                }
            }

            virtual ~StatisticsManager()
            {
                Clear();
            }

            bool ContainsStatistic(const StatIdType& statId) const
            {
                auto iterator = m_statistics.find(statId);
                return iterator != m_statistics.end();
            }

            AZ::u32 GetCount() const
            {
                return static_cast<AZ::u32>(m_statistics.size());
            }

            void GetAllStatistics(AZStd::vector<NamedRunningStatistic*>& vector)
            {
                for (auto const& it : m_statistics)
                {
                    NamedRunningStatistic* stat = it.second;
                    vector.push_back(stat);
                }
            }

            //! Helper method to apply units to statistics with empty units string.
            AZ::u32 ApplyUnits(const AZStd::string& units)
            {
                AZ::u32 updatedCount = 0;
                for (auto& it : m_statistics)
                {
                    NamedRunningStatistic* stat = it.second;
                    if (stat->GetUnits().empty())
                    {
                        stat->UpdateUnits(units);
                        updatedCount++;
                    }
                }
                return updatedCount;
            }

            void Clear()
            {
                for (auto& it : m_statistics)
                {
                    NamedRunningStatistic* stat = it.second;
                    delete stat;
                }
                m_statistics.clear();
            }

            /**
             * Returns nullptr if a statistic with such name doesn't exist,
             * otherwise returns a pointer to the statistic.
             */
            NamedRunningStatistic* GetStatistic(const StatIdType& statId)
            {
                auto iterator = m_statistics.find(statId);
                if (iterator == m_statistics.end())
                {
                    return nullptr;
                }
                return iterator->second;
            }

            //! Returns false if a NamedRunningStatistic with such id already exists.
            NamedRunningStatistic* AddStatistic(const StatIdType& statId, const bool failIfExist = true)
            {
                if (failIfExist)
                {
                    NamedRunningStatistic* prevStat = GetStatistic(statId);
                    if (prevStat)
                    {
                        return nullptr;
                    }
                }
                NamedRunningStatistic* stat = new NamedRunningStatistic();
                m_statistics[statId] = stat;
                return stat;
            }

            //! Returns false if a NamedRunningStatistic with such id already exists.
            NamedRunningStatistic* AddStatistic(const StatIdType& statId, const AZStd::string& name, const AZStd::string& units, const bool failIfExist = true)
            {
                if (failIfExist)
                {
                    NamedRunningStatistic* prevStat = GetStatistic(statId);
                    if (prevStat)
                    {
                        return nullptr;
                    }
                }
                NamedRunningStatistic* stat = new NamedRunningStatistic(name, units);
                m_statistics[statId] = stat;
                return stat;
            }

            virtual void RemoveStatistic(const StatIdType& statId)
            {
                auto iterator = m_statistics.find(statId);
                if (iterator == m_statistics.end())
                {
                    return;
                }
                NamedRunningStatistic* prevStat = iterator->second;
                delete prevStat;
                m_statistics.erase(iterator);
            }

            void ResetStatistic(const StatIdType& statId)
            {
                NamedRunningStatistic* stat = GetStatistic(statId);
                if (!stat)
                {
                    return;
                }
                stat->Reset();
            }

            void ResetAllStatistics()
            {
                for (auto& it : m_statistics)
                {
                    NamedRunningStatistic* stat = it.second;
                    stat->Reset();
                }
            }

            void PushSampleForStatistic(const StatIdType& statId, double value)
            {
                NamedRunningStatistic* stat = GetStatistic(statId);
                if (!stat)
                {
                    return;
                }
                stat->PushSample(value);
            }

            //! Expensive function because it does a reverse lookup
            bool GetStatId(NamedRunningStatistic* searchStat, StatIdType& statIdOut) const
            {
                for (auto& it : m_statistics)
                {
                    NamedRunningStatistic* stat = it.second;
                    if (stat == searchStat)
                    {
                        statIdOut = it.first;
                        return true;
                    }
                }
                return false;
            }


        private:
            ///Key: StatIdType, Value: NamedRunningStatistic*
            AZStd::unordered_map<StatIdType, NamedRunningStatistic*> m_statistics;
        };//class StatisticsManager
    }//namespace Statistics
}//namespace AzFramework
