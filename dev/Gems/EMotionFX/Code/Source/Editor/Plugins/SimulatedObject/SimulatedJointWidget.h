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

#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/RTTI/TypeInfo.h>
#include <QScrollArea>
#include <QItemSelection>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QPushButton)

namespace AzQtComponents
{
    class Card;
} // namespace AzQtComponents

namespace EMotionFX
{
    class Actor;
    class Node;
    class ObjectEditor;
    class AddToSimulatedObjectButton;
    class SimulatedObjectWidget;
    class ObjectEditor;
    class SimulatedObjectPropertyNotify;
    class SimulatedObjectColliderWidget;

    class SimulatedJointWidget
        : public QScrollArea
    {
        Q_OBJECT //AUTOMOC

    public:
        explicit SimulatedJointWidget(SimulatedObjectWidget* plugin, QWidget* parent = nullptr);
        ~SimulatedJointWidget() override;

        void UpdateDetailsView(const QItemSelection& selected, const QItemSelection& deselected);

        void RemoveSelectedSimulatedJoint() const;

    private:
        static const int s_jointLabelSpacing;
        static const int s_jointNameSpacing;

        SimulatedObjectWidget* m_plugin;
        QWidget* m_noSelectionWidget;
        QWidget* m_contentsWidget;
        QPushButton* m_removeButton;
        ObjectEditor* m_simulatedObjectEditor;
        ObjectEditor* m_simulatedJointEditor;
        AzQtComponents::Card* m_simulatedObjectEditorCard;
        AzQtComponents::Card* m_simulatedJointEditorCard;
        AZStd::unique_ptr<SimulatedObjectPropertyNotify> m_propertyNotify;
        QWidget* m_colliderWidget = nullptr;
    };
} // namespace EMotionFX
