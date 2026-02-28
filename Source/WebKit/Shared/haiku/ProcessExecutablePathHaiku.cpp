/*
 * Copyright (C) 2012 Samsung Electronics
 * Copyright (C) 2014,2019 Haiku, inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessExecutablePath.h"

#include <Path.h>
#include <PathFinder.h>
#include <String.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

// Get the app directory from the currently running binary
static String executableDirectory()
{
    BPath imagePath;

    // B_FIND_PATH_IMAGE_PATH returns the full path of the currently running binary
    // The 'this' pointer serves as a code pointer to identify the current image
    BPathFinder pathFinder(reinterpret_cast<const void*>(&executableDirectory));
    status_t status = pathFinder.FindPath(B_FIND_PATH_IMAGE_PATH, imagePath);
    if (status != B_OK)
        return "./"_s; // fallback to original behavior

    // Go up from the lib dir -> app root dir 
    imagePath.GetParent(&imagePath);
    imagePath.GetParent(&imagePath);
    
    return String::fromUTF8(imagePath.Path());
}

String executablePathOfWebProcess()
{
    return makeString(executableDirectory(), "/bin/WebProcess"_s);
}

String executablePathOfPluginProcess()
{
    return makeString(executableDirectory(), "/bin/PluginProcess"_s);
}

String executablePathOfNetworkProcess()
{
    return makeString(executableDirectory(), "/bin/NetworkProcess"_s);
}

} // namespace WebKit

