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

#include "StdAfx.h"
#include "ATLUtils.h"

namespace Audio
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
    bool AudioDebugDrawFilter(const AZStd::string& objectName, const AZStd::string& filter)
    {
        bool result = false;
        result |= filter.empty();
        result |= (filter.compare("0") == 0);
        result |= (objectName.find(filter) != AZStd::string::npos);
        return result;
    }
#endif //INCLUDE_AUDIO_PRODUCTION_CODE
} // namespace Audio
