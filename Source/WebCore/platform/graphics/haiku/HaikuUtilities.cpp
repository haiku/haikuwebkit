/*
 * Copyright (C) 2024 Haiku, inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HaikuUtilities.h"

#if PLATFORM(HAIKU)

#include "Logging.h"

#include <Bitmap.h>
#include <optional>
#include <OS.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>

namespace WebCore{

uintptr_t getBitmapUniqueID(BBitmap* bitmap)
{
    // Gets a unique identifier to a bitmap that is same across processes.
    // This version gets the id from the id attached to the image's area's name
    // by SharedMemory::allocate

    area_id area = bitmap->Area();

    area_info info;
    status_t status = get_area_info(area, &info);

    std::optional<uint64_t> decodedID;
    if (status == B_OK) {
        auto base64Data = WTF::String::fromLatin1(&info.name[7]);
        auto data = WTF::base64Decode(base64Data);
        WTF::Persistence::Decoder decoder({ data->data(), data->size() });
        decoder >> decodedID;
    }

    if (decodedID) {
        // FIXME: This code is untested. This can be remove once tested.
        LOG(Compositing, "Found image ID %ld", *decodedID);
        return *decodedID;
    } else {
        LOG(Compositing, "Failed to find image id for BBitmap. Falling back to 0");
        return 0;
    }
}

} // namespace WebCore

#endif // PLATFORM(HAIKU)
