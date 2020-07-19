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
// Original file Copyright Crytek GMBH or its affiliates, used under license.

// Description : implementation file


#include "StdAfx.h"
#include "TimeOfDaySlider.h"

QString TimeOfDaySlider::hoverValueText(int sliderValue) const
{
    return QString::fromLatin1("%1:%2").arg(static_cast<int>(sliderValue / 60)).arg(sliderValue % 60, 2, 10, QLatin1Char('0'));
}

#include <Controls/TimeOfDaySlider.moc>
