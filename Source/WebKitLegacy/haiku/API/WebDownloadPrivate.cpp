/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebDownloadPrivate.h"

#include "WebCore/NetworkingContext.h"
#include "WebCore/NotImplemented.h"
#include "WebCore/ResourceHandle.h"
#include "WebCore/ResourceRequest.h"
#include "WebCore/ResourceResponse.h"
#include "WebCore/SecurityOrigin.h"
#include "WebCore/SharedBuffer.h"
#include "pal/text/TextEncoding.h"
#include "WebDownload.h"
#include "WebPage.h"

#include <Directory.h>
#include <Entry.h>
#include <Message.h>
#include <Messenger.h>
#include <MimeType.h>
#include <NodeInfo.h>
#include <stdio.h>

#include <wtf/text/CString.h>

namespace BPrivate {

static const int kMaxMimeTypeGuessTries	= 5;

WebDownloadPrivate::WebDownloadPrivate(const ResourceRequest& request,
        WebCore::NetworkingContext* context)
    : m_webDownload(0)
    , m_resourceHandle(ResourceHandle::create(context, request, nullptr, false, false,
         WebCore::ContentEncodingSniffingPolicy::Disable, WebCore::SecurityOrigin::createOpaque(), false))
    , m_currentSize(0)
    , m_expectedSize(0)
    , m_url(request.url().string())
    , m_path("/boot/home/Desktop/")
    , m_filename("Download")
    , m_mimeType()
    , m_mimeTypeGuessTries(kMaxMimeTypeGuessTries)
    , m_lastProgressReportTime(0)
{
#if USE(CURL)
	m_download = adoptRef(new WebCore::CurlDownload());
	m_download->init(*this, m_resourceHandle.get(), request, m_response);
#endif
}

#if USE(CURL)
void WebDownloadPrivate::didReceiveResponse(const ResourceResponse& response)
#else
void WebDownloadPrivate::didReceiveResponseAsync(ResourceHandle*, ResourceResponse&& response, WTF::CompletionHandler<void()>&& handler)
#endif
{
    if (!response.isNull()) {
    	if (!response.suggestedFilename().isEmpty())
            m_filename = response.suggestedFilename();
        else {
        	WTF::URL url(response.url());
            url.setQuery(String());
            url.removeFragmentIdentifier();
            m_filename = PAL::decodeURLEscapeSequences(url.lastPathComponent()).utf8().data();
        }
        if (response.mimeType().length()) {
        	// Do some checks, as no mime type yet is always better
        	// than set an invalid one
        	BString mimeType = response.mimeType().utf8().data();
        	BMimeType type(mimeType);
        	BMimeType superType;
        	if (type.IsValid() && type.GetSupertype(&superType) == B_OK
        		&& superType.IsValid()
        		&& strchr(mimeType, '*') == NULL) {
	            m_mimeType = mimeType;
        	}
		}

        m_expectedSize = response.expectedContentLength();
    }

    m_url = response.url().string();

#if USE(CURL)
    // Now that we have the proper filename from the request, rename the downloaded file to that
    // and notify the UI that the download is started
    //
    // findAvailableFilename uses m_path as an input (the downlaod directory) and as an output
    // (the final file name). Since we already called it once when starting the download, restore
    // m_path to be the download path again.
    BPath temp;
    m_path.GetParent(&temp);
    m_path = temp;
    findAvailableFilename();
    m_download->setDestination(WTF::String::fromUTF8(m_path.Path()));
    if (m_progressListener.IsValid()) {
        BMessage message(B_DOWNLOAD_STARTED);
        message.AddString("path", m_path.Path());
        m_progressListener.SendMessage(&message);
    }
#endif
}

#if USE(CURL)
void WebDownloadPrivate::didReceiveDataOfLength(int encodedDataLength)
{
    m_currentSize += encodedDataLength;

    // FIXME: Report total size update, if m_currentSize greater than previous total size
    BMessage message(B_DOWNLOAD_PROGRESS);
    message.AddFloat("progress", m_currentSize * 100.0 / m_expectedSize);
    message.AddInt64("current size", m_currentSize);
    message.AddInt64("expected size", m_expectedSize);
    m_progressListener.SendMessage(&message);
}

void WebDownloadPrivate::didFinish()
{
    handleFinished(m_resourceHandle.get(), B_DOWNLOAD_FINISHED);
}

void WebDownloadPrivate::didFail()
{
    handleFinished(m_resourceHandle.get(), B_DOWNLOAD_FAILED);
}
#else
void WebDownloadPrivate::didReceiveData(ResourceHandle*, const WebCore::SharedBuffer& buffer, int /*encodedDataLength*/)
{
	if (m_file.InitCheck() != B_OK)
		createFile();

    ssize_t bytesWritten = m_file.Write(buffer.data(), buffer.size());
    if (bytesWritten != (ssize_t)buffer.size()) {
        // FIXME: Report error
        return;
    }
    m_currentSize += buffer.size();

    if (m_currentSize > 0 && m_mimeTypeGuessTries > 0) {
    	// Try to guess the MIME type from its actual content
    	BMimeType type;
    	entry_ref ref;
    	BEntry entry(m_path.Path());
    	entry.GetRef(&ref);

		if (BMimeType::GuessMimeType(&ref, &type) == B_OK
			&& type.Type() != B_FILE_MIME_TYPE) {
    		BNodeInfo info(&m_file);
			info.SetType(type.Type());
			m_mimeTypeGuessTries = -1;
    	} else
    		m_mimeTypeGuessTries--;
    }

    // FIXME: Report total size update, if m_currentSize greater than previous total size
    BMessage message(B_DOWNLOAD_PROGRESS);
    message.AddFloat("progress", m_currentSize * 100.0 / m_expectedSize);
    message.AddInt64("current size", m_currentSize);
    message.AddInt64("expected size", m_expectedSize);
    m_progressListener.SendMessage(&message);
}

void WebDownloadPrivate::didFinishLoading(ResourceHandle* handle, const WebCore::NetworkLoadMetrics&)
{
    handleFinished(handle, B_DOWNLOAD_FINISHED);
}

void WebDownloadPrivate::didFail(ResourceHandle* handle, const ResourceError& /*error*/)
{
    handleFinished(handle, B_DOWNLOAD_FAILED);
}

void WebDownloadPrivate::wasBlocked(ResourceHandle* handle)
{
    // FIXME: Implement this when we have the new frame loader signals
    // and error handling.
    handleFinished(handle, B_DOWNLOAD_BLOCKED);
}

void WebDownloadPrivate::cannotShowURL(ResourceHandle* handle)
{
    // FIXME: Implement this when we have the new frame loader signals
    // and error handling.
    handleFinished(handle, B_DOWNLOAD_CANNOT_SHOW_URL);
}
#endif

void WebDownloadPrivate::setDownload(BWebDownload* download)
{
    m_webDownload = download;
}

void WebDownloadPrivate::start(const BPath& path)
{
    if (path.InitCheck() == B_OK)
        m_path = path;

#if USE(CURL)
    // Create the download with the name "Download" in the target directory.
    // After the request is complete, and we have the actual filename, it will be renamed.
    findAvailableFilename();
    m_download->start(WTF::String::fromUTF8(m_path.Path()));
#endif
}

void WebDownloadPrivate::hasMovedTo(const BPath& path)
{
	m_path = path;
}

void WebDownloadPrivate::cancel()
{
#if USE(CURL)
    m_download->cancel();
#else
    m_resourceHandle->cancel();
#endif
}

void WebDownloadPrivate::setProgressListener(const BMessenger& listener)
{
    m_progressListener = listener;
}

// #pragma mark - private

void WebDownloadPrivate::handleFinished(WebCore::ResourceHandle* handle, uint32 /*status*/)
{
#if USE(CURL)
    BNode node(m_path.Path());
    node.WriteAttrString("META:url", &m_url);
#endif

    if (m_mimeTypeGuessTries != -1 && m_mimeType.Length() > 0) {
        // In last resort, use the MIME type provided
        // by the response, which pass our validation
#if USE(CURL)
        BNodeInfo info(&node);
#else
        BNodeInfo info(&m_file);
#endif
        info.SetType(m_mimeType);
    }

    if (m_progressListener.IsValid()) {
        BMessage message(B_DOWNLOAD_REMOVED);
        message.AddPointer("download", m_webDownload);
        // Block until the listener has released the object on it's side...
        BMessage reply;
        m_progressListener.SendMessage(&message, &reply);
    }
    delete m_webDownload;
}

#if !USE(CURL)
void WebDownloadPrivate::createFile()
{
    // Don't overwrite existing files
    findAvailableFilename();

    if (m_file.SetTo(m_path.Path(), B_CREATE_FILE | B_ERASE_FILE | B_WRITE_ONLY) == B_OK)
        m_file.WriteAttrString("META:url", &m_url);

    if (m_progressListener.IsValid()) {
        BMessage message(B_DOWNLOAD_STARTED);
        message.AddString("path", m_path.Path());
        m_progressListener.SendMessage(&message);
    }
}
#endif

void WebDownloadPrivate::findAvailableFilename()
{
    BPath filePath = m_path;
    BString fileName = m_filename;
    filePath.Append(fileName.String());

    // Make sure the parent directory exists.
    BPath parent;
    if (filePath.GetParent(&parent) == B_OK)
        create_directory(parent.Path(), 0755);

    // Find a name that doesn't exists in the directoy yet
    BEntry entry(filePath.Path());
    for (int32 i = 0; entry.InitCheck() == B_OK && entry.Exists(); i++) {
        // Use original file name in each iteration
        BString baseName = m_filename;

        // Separate extension and base file name
        int32 extensionStart = baseName.FindLast('.');
        BString extension;
        if (extensionStart > 0)
            baseName.MoveInto(extension, extensionStart, baseName.CountChars() - extensionStart);

        // Add i to file name before the extension
        char num[12];
        snprintf(num, sizeof(num), "-%" B_PRId32, i);
        baseName.Append(num).Append(extension);
        fileName = baseName;
        filePath = m_path;
        filePath.Append(fileName);
        entry.SetTo(filePath.Path());
    }
    m_filename = fileName;
    m_path = filePath;
}

} // namespace BPrivate

