/*
 * Copyright (C) 2019 Haiku, Inc. All rights reserved.
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
#include "SharedMemory.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"

#include <OS.h>
#include <WebCore/BitmapImage.h>
#include <WebCore/IntSize.h>
#include <Bitmap.h>

using namespace WebCore;
namespace WebKit {

RefPtr<SharedMemory> SharedMemory::allocate(size_t size)
{
	void* baseAddress;

	area_id sharedArea = create_area("WebKit-Shared-Memory", &baseAddress, B_ANY_ADDRESS,
		size, B_NO_LOCK, B_READ_AREA | B_WRITE_AREA);

	if (sharedArea < 0)
		return nullptr;

	RefPtr<SharedMemory> memory = adoptRef(new SharedMemory);
	memory->m_size = size;
	memory->m_data = baseAddress;
	memory->m_areaid = sharedArea;

	return memory;
}

RefPtr<SharedMemory> SharedMemory::map(Handle&& handle, Protection protection)
{
	RefPtr<SharedMemory> memory = adoptRef(new SharedMemory());
	if(!memory)
		return nullptr;

	memory->m_areaid = handle.m_handle;
	memory->m_size = handle.size();
	return memory;
}

SharedMemory::~SharedMemory()
{
	if(!m_areaid)
		return;

	//unset
	delete_area(m_areaid);
	m_areaid = 0;
}

std::optional<SharedMemory::Handle> SharedMemory::createHandle(Protection)
{
    ASSERT(m_areaid);

	return SharedMemory::Handle(WTFMove(m_areaid), m_size);
}

} // namespace WebKit


