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


#include <AzCore/PlatformDef.h>

#include <GL/glew.h>

#ifdef AZ_PLATFORM_APPLE
    #include <OpenGL/gl.h>
    #include <OpenGL/glu.h>
#else

#ifdef AZ_PLATFORM_WINDOWS
    #include <GL/wglew.h>
#endif
    #include <GL/gl.h>
    #include <GL/glu.h>
#endif
