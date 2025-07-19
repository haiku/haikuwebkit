/*
 * Copyright (C) 2025-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "VMAllocate.h"

namespace bmalloc {

#if BMALLOC_USE_MADV_ZERO
static pthread_once_t madvZeroOnceControl = PTHREAD_ONCE_INIT;
static bool madvZeroSupported = false;

static void zeroFillLatchIfMadvZeroIsSupported()
{
    // See libpas/pas_page_malloc.c for details on this approach
    // The logic is copied in both places, so if we change one we
    // should change the other as well.

    size_t pageSize = vmPageSize();
    void* base = mmap(NULL, pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANON | BMALLOC_NORESERVE, static_cast<int>(VMTag::Malloc), 0);
    BASSERT(base);

    int rc = madvise(base, pageSize, MADV_ZERO);
    if (rc)
        madvZeroSupported = true;
    else
        madvZeroSupported = false;
    munmap(base, pageSize);
}

bool isMadvZeroSupported()
{
    pthread_once(&madvZeroOnceControl, zeroFillLatchIfMadvZeroIsSupported);
    return madvZeroSupported;
}
#endif

} // namespace bmalloc
