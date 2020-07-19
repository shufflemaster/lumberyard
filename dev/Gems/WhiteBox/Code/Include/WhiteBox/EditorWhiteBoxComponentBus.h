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

#include "EditorWhiteBoxDefaultShapeTypes.h"

#include <AzCore/Component/ComponentBus.h>

namespace WhiteBox
{
    struct WhiteBoxMesh;

    //! Wrapper around WhiteBoxMesh address.
    struct WhiteBoxMeshHandle
    {
        uintptr_t m_whiteBoxMeshAddress; //!< The raw address of the WhiteBoxMesh pointer.
    };

    //! EditorWhiteBoxComponent requests.
    class EditorWhiteBoxComponentRequests : public AZ::EntityComponentBus
    {
    public:
        // EBusTraits overrides ...
        static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;

        //! Return a pointer to the WhiteBoxMesh.
        virtual WhiteBoxMesh* GetWhiteBoxMesh() = 0;

        //! Return a handle wrapping the raw address of the WhiteBoxMesh pointer.
        //! @note This is currently used to address the WhiteBoxMesh via script.
        virtual WhiteBoxMeshHandle GetWhiteBoxMeshHandle();

        //! Serialize the current mesh data.
        virtual void SerializeWhiteBox() = 0;

        //! Set the white box mesh default shape.
        virtual void SetDefaultShape(DefaultShapeType defaultShape) = 0;

    protected:
        ~EditorWhiteBoxComponentRequests() = default;
    };

    inline WhiteBoxMeshHandle EditorWhiteBoxComponentRequests::GetWhiteBoxMeshHandle()
    {
        return WhiteBoxMeshHandle{reinterpret_cast<uintptr_t>(static_cast<void*>(GetWhiteBoxMesh()))};
    }

    using EditorWhiteBoxComponentRequestBus = AZ::EBus<EditorWhiteBoxComponentRequests>;

    //! EditorWhiteBoxComponent notifications.
    class EditorWhiteBoxComponentNotifications : public AZ::EntityComponentBus
    {
    public:
        //! Notify the component the mesh has been modified.
        virtual void OnWhiteBoxMeshModified() {}

        //! Notify listeners when the default shape of the white box mesh changes.
        virtual void OnDefaultShapeTypeChanged([[maybe_unused]] DefaultShapeType defaultShape) {}

    protected:
        ~EditorWhiteBoxComponentNotifications() = default;
    };

    using EditorWhiteBoxComponentNotificationBus = AZ::EBus<EditorWhiteBoxComponentNotifications>;
} // namespace WhiteBox
