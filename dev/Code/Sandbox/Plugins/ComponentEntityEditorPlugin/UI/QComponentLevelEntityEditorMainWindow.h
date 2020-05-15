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

#include <QMainWindow>

#include <AzCore/Serialization/SerializeContext.h>
#include <Editor/IEditor.h>
#include <AzToolsFramework/Slice/SliceMetadataEntityContextBus.h>

class QObjectPropertyModel;
class PropertyInfo;

namespace AZ
{
    class Entity;
}

namespace AzToolsFramework
{
    class ReflectedPropertyEditor;
    class EntityPropertyEditor;
}

// This is the shell class to interface between Qt and the Sandbox.  All Sandbox implementation is retained in an inherited class.
class QComponentLevelEntityEditorInspectorWindow
    : public QMainWindow
    , public AzToolsFramework::SliceMetadataEntityContextNotificationBus::Handler
    , private IEditorNotifyListener
{
    Q_OBJECT

public:
    explicit QComponentLevelEntityEditorInspectorWindow(QWidget* parent = 0);
    ~QComponentLevelEntityEditorInspectorWindow();

    void Init();

    // you are required to implement this to satisfy the unregister/registerclass requirements on "AzToolsFramework::RegisterViewPane"
    // make sure you pick a unique GUID
    static const GUID& GetClassID()
    {
        //{F539C646-7FC6-4AF4-BB58-F8A161AF6746}
        static const GUID guid =
        {
            0xF539C646, 0x7FC6, 0x4AF4, { 0x8a, 0x9c, 0xf8, 0xa1, 0x61, 0xaf, 0x67, 0x46 }
        };
        return guid;
    }

    AzToolsFramework::EntityPropertyEditor* GetPropertyEditor() { return m_propertyEditor; }

private:
    void OnMetadataEntityAdded(AZ::EntityId /*entityId*/) override;

    void RefreshPropertyEditor();

    AZ::EntityId GetRootMetaDataEntityId() const;

    //////////////////////////////////////////////////////////////////////////
    // IEditorEventListener
    void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

    AzToolsFramework::EntityPropertyEditor* m_propertyEditor;
};
