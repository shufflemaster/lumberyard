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

#include <AzCore/Component/ComponentBus.h>
#include <LyShine/UiLayoutCellBase.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
class UiLayoutCellDefaultInterface
    : public AZ::ComponentBus
{
public: // member functions

    virtual ~UiLayoutCellDefaultInterface() {}

    //! Get the minimum width
    virtual float GetMinWidth() = 0;

    //! Get the minimum height
    virtual float GetMinHeight() = 0;

    //! Get the target width
    //! \param maxWidth A width that the element will not surpass. LyShine::UiLayoutCellUnspecifiedSize means no max 
    virtual float GetTargetWidth(float maxWidth) = 0;

    //! Get the target height
    //! \param maxHeight A height that the element will not surpass. LyShine::UiLayoutCellUnspecifiedSize means no max
    virtual float GetTargetHeight(float maxHeight) = 0;

    //! Get the extra width ratio
    virtual float GetExtraWidthRatio() = 0;

    //! Get the extra height ratio
    virtual float GetExtraHeightRatio() = 0;

public: // static member data

    //! Multiple components on a entity can implement the events
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Multiple;
};

typedef AZ::EBus<UiLayoutCellDefaultInterface> UiLayoutCellDefaultBus;
