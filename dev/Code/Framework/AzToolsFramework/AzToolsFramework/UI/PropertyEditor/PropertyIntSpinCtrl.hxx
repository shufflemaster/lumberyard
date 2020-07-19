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

#include <AzToolsFramework/UI/PropertyEditor/PropertyIntCtrlCommon.h>


namespace AzQtComponents
{
    class SpinBox;
}

namespace AzToolsFramework
{   
    class PropertyIntSpinCtrl
        : public QWidget
    {
        Q_OBJECT
    public:
        AZ_CLASS_ALLOCATOR(PropertyIntSpinCtrl, AZ::SystemAllocator, 0);

        PropertyIntSpinCtrl(QWidget* parent = NULL);
        virtual ~PropertyIntSpinCtrl();

    public slots:
        void setValue(AZ::s64 val);
        void setMinimum(AZ::s64 val);
        void setMaximum(AZ::s64 val);
        void setStep(AZ::s64 val);
        void setMultiplier(AZ::s64 val);
        void setPrefix(QString val);
        void setSuffix(QString val);

        AZ::s64 value() const;
        AZ::s64 minimum() const;
        AZ::s64 maximum() const;
        AZ::s64 step() const;

        QWidget* GetFirstInTabOrder();
        QWidget* GetLastInTabOrder();
        void UpdateTabOrder();

    protected slots:
        void onChildSpinboxValueChange(int value);
    signals:
        void valueChanged(AZ::s64 newValue);
        void editingFinished();

    private:
        AzQtComponents::SpinBox* m_pSpinBox;
        AZ::s64 m_multiplier;

    protected:
        virtual void focusInEvent(QFocusEvent* e);
    };

    // Base class to allow QObject inheritance and definitions for IntSpinBoxHandlerCommon class template
    class IntSpinBoxHandlerQObject
        : public QObject
    {
        // this is a Qt Object purely so it can connect to slots with context.  This is the only reason its in this header.
        Q_OBJECT
    };

    template <class ValueType>
    class IntSpinBoxHandler
        : public IntWidgetHandler<ValueType, PropertyIntSpinCtrl, IntSpinBoxHandlerQObject>
    {
        using BaseHandler = IntWidgetHandler<ValueType, PropertyIntSpinCtrl, IntSpinBoxHandlerQObject>;
    public:
        AZ_CLASS_ALLOCATOR(IntSpinBoxHandler, AZ::SystemAllocator, 0);
    protected:
        bool IsDefaultHandler() const override;
        AZ::u32 GetHandlerName(void) const override;
        QWidget* CreateGUI(QWidget* pParent) override;
    };

    template <class ValueType>
    bool IntSpinBoxHandler<ValueType>::IsDefaultHandler() const
    {
        return true;
    }

    template <class ValueType>
    AZ::u32 IntSpinBoxHandler<ValueType>::GetHandlerName(void) const
    {
        return AZ::Edit::UIHandlers::SpinBox;
    }

    template <class ValueType>
    QWidget* IntSpinBoxHandler<ValueType>::CreateGUI(QWidget* parent)
    {
        PropertyIntSpinCtrl* newCtrl = static_cast<PropertyIntSpinCtrl*>(BaseHandler::CreateGUI(parent));
        this->connect(newCtrl, &PropertyIntSpinCtrl::valueChanged, [newCtrl]()
        {
            AzToolsFramework::PropertyEditorGUIMessages::Bus::Broadcast(&PropertyEditorGUIMessages::Bus::Handler::RequestWrite, newCtrl);
        });

        return newCtrl;
    }

    void RegisterIntSpinBoxHandlers();
};
