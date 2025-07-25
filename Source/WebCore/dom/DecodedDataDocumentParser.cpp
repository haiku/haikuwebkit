/*
 * Copyright (C) 2010 Google, Inc. All rights reserved.
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

#include "config.h"
#include "DecodedDataDocumentParser.h"

#include "Document.h"
#include "DocumentWriter.h"
#include "SegmentedString.h"
#include "TextResourceDecoder.h"

namespace WebCore {

DecodedDataDocumentParser::DecodedDataDocumentParser(Document& document)
    : DocumentParser(document)
{
}

void DecodedDataDocumentParser::appendBytes(DocumentWriter& writer, std::span<const uint8_t> data)
{
    if (data.empty())
        return;

    String decoded = writer.protectedDecoder()->decode(data);
    if (decoded.isEmpty())
        return;

    writer.reportDataReceived();
    append(decoded.releaseImpl());
}

void DecodedDataDocumentParser::flush(DocumentWriter& writer)
{
    String remainingData = writer.protectedDecoder()->flush();
    if (remainingData.isEmpty())
        return;

    writer.reportDataReceived();
    append(remainingData.releaseImpl());
}

};
