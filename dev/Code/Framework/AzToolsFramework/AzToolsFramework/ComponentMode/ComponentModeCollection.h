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
#include <AzToolsFramework/ComponentMode/EditorComponentModeBus.h>

namespace AzToolsFramework
{
    class EditorMetricsEventsBusTraits;

    namespace ComponentModeFramework
    {
        /// Manages all individual ComponentModes for a single instance of Editor wide ComponentMode.
        class ComponentModeCollection
        {
        public:
            AZ_CLASS_ALLOCATOR_DECL

            /// @cond
            ComponentModeCollection() = default;
            ~ComponentModeCollection() = default;
            /// @endcond

            /// Add a ComponentMode for a given Component type on the EntityId specified.
            /// An \ref AZ::EntityComponentIdPair is provided so the individual Component on
            /// the Entity can be addressed if there are more than one of the same type.
            void AddComponentMode(
                const AZ::EntityComponentIdPair& entityComponentIdPair, AZ::Uuid componentType,
                const ComponentModeFactoryFunction& componentModeBuilder);

            /// Refresh (update) all active ComponentModes for the specified Entity and Component Id pair.
            void Refresh(const AZ::EntityComponentIdPair& entityComponentIdPair);

            /// Begin Editor-wide ComponentMode.
            /// Notify other systems a ComponentMode is starting and transition the Editor to the correct state.
            void BeginComponentMode();
            /// End Editor-wide ComponentMode
            /// Leave all active ComponentModes and move the Editor back to its normal state.
            void EndComponentMode();

            /// Ensure entire Entity selection is moved to ComponentMode.
            /// For all other selected entities, if they have a matching component that has just entered
            /// ComponentMode, add them too (duplicates will not be added - handled by AddComponentMode)
            void AddOtherSelectedEntityModes();

            /// Return is the Editor-wide ComponentMode state active.
            bool InComponentMode() const { return m_componentMode; }
            /// Are ComponentModes in the process of being added.
            /// Used to determine if other selected entities with the same Component type should also be added.
            bool ModesAdded() const { return m_adding; }

            /// Return if this Entity and its Component are currently in ComponentMode.
            bool AddedToComponentMode(
                const AZ::EntityComponentIdPair& entityComponentIdPair, const AZ::Uuid& componentType);

            /// Move to the next active ComponentMode so the Actions for that mode become available (it is now 'selected').
            void SelectNextActiveComponentMode();

            /// Move to the previous active ComponentMode so the Actions for that mode become available (it is now 'selected').
            void SelectPreviousActiveComponentMode();

            /// Pick a specific ComponentMode for a Component (by directly selecting a Component in the EntityInspector - is it now 'selected').
            void SelectActiveComponentMode(const AZ::Uuid& componentType);

            /// Refresh Actions (shortcuts) for the 'selected' ComponentMode.
            void RefreshActions();

        private:
            AZ_DISABLE_COPY_MOVE(ComponentModeCollection)

            // Alias for EditorMetricsEventsBusTraits Enter/LeaveComponentMode notification
            using MetricFn = void (EditorMetricsEventsBusTraits::*)(
                const AZStd::vector<AZ::EntityId>&, const AZStd::vector<AZ::Uuid>&);

            // Record which components and entities entered Component Mode.
            void RecordMetrics(MetricFn metricFn);

            // Internal helper used by Select[|Prev|Next]ActiveComponentMode
            void ActiveComponentModeChanged();

            AZStd::vector<AZ::Uuid> m_activeComponentTypes; ///< What types of ComponentMode are currently active.
            AZStd::vector<EntityAndComponentMode> m_entitiesAndComponentModes; ///< The active ComponentModes (one per Entity).
            AZStd::vector<EntityAndComponentModeBuilders> m_entitiesAndComponentModeBuilders; ///< Factory functions to re-create specific modes
                                                                                              ///< tied to a particular Entity (for undo/redo).

            size_t m_selectedComponentModeIndex = 0; ///< Index into the array of active ComponentModes, current index is 'selected' ComponentMode.
            bool m_adding = false; ///< Are we currently adding individual ComponentModes to the Editor wide ComponentMode.
            bool m_componentMode = false; ///< Editor (global) ComponentMode flag - is ComponentMode active or not.
        };
    } // namespace ComponentModeFramework
} // namespace AzToolsFramework