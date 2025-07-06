// Copyright (C) 2025 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.0)

import Testing
@_spi(Testing) import WebKit
import _WebKit_SwiftUI
import UniformTypeIdentifiers
import SwiftUI

@MainActor
struct WebPageTransferableTests {
    @Test
    func transferableContentTypes() async throws {
        let exportedContentTypes = WebPage.exportedContentTypes()
        #expect(exportedContentTypes == [.webArchive, .pdf, .image, .url])

        let importedContentTypes = WebPage.importedContentTypes()
        #expect(importedContentTypes.isEmpty)

        let webPage = WebPage()

        let instanceExportedContentTypes = webPage.exportedContentTypes()
        #expect(instanceExportedContentTypes == [.webArchive, .pdf, .image, .url])

        let instanceImportedContentTypes = webPage.importedContentTypes()
        #expect(instanceImportedContentTypes.isEmpty)
    }

    @Test
    func exportToPDFWithFullContent() async throws {
        let webPage = WebPage()
        webPage.load(
            html: "<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>",
            baseURL: .aboutBlank
        )

        // FIXME: Remove this when possible.
        try await Task.sleep(for: .seconds(10))

        let data = try await webPage.exported(as: .pdf)
        let pdf = TestPDFDocument(from: data)
        #expect(pdf.pageCount == 1)

        let page = try #require(pdf.page(at: 0))

        #expect(page.bounds == .init(x: 0, y: 0, width: 1024, height: 768))
        #expect(page.text == "Hello")
        #expect(page.color(at: .init(x: 400, y: 300)) == .green)
    }

    @Test
    func exportToPDFWithSubRegion() async throws {
        let webPage = WebPage()
        webPage.load(
            html: "<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>",
            baseURL: .aboutBlank
        )

        // FIXME: Remove this when possible.
        try await Task.sleep(for: .seconds(10))

        let region = CGRect(x: 200, y: 150, width: 400, height: 300)
        let data = try await webPage.exported(as: .pdf(region: .rect(region)))
        let pdf = TestPDFDocument(from: data)
        #expect(pdf.pageCount == 1)

        let page = try #require(pdf.page(at: 0))

        #expect(page.bounds == .init(x: 0, y: 0, width: 400, height: 300))
        #expect(page.characterCount == 0)
        #expect(page.color(at: .init(x: 200, y: 150)) == .green)
    }

    @Test
    func exportToPDFWithoutLoadingAnyWebContentFails() async throws {
        let webPage = WebPage()

        // FIXME: This should be `TransferableError.self`, but that type is not API.
        await #expect(throws: (any Error).self) {
            let _ = try await webPage.exported(as: .pdf)
        }
    }

    @Test
    func exportToURL() async throws {
        let webPage = WebPage()
        webPage.load(
            html: "<meta name='viewport' content='width=device-width'><body bgcolor=#00ff00>Hello</body>",
            baseURL: .aboutBlank
        )

        // FIXME: Remove this when possible.
        try await Task.sleep(for: .seconds(10))

        let data = try await webPage.exported(as: .url)
        let actualURL = try await URL(importing: data, contentType: .url)

        #expect(actualURL == .aboutBlank)
    }

    @Test
    func exportToImage() async throws {
        let defaultFrame = CGRect(x: 0, y: 0, width: 1024, height: 768)
        let scaleFactor: CGFloat = 2

        let webPage = WebPage()
        webPage.load(
            html: "<body style='background-color: red;'></body>",
            baseURL: .aboutBlank
        )

        // FIXME: Remove this when possible.
        try await Task.sleep(for: .seconds(10))

        let imageData = try await webPage.exported(as: .image)

        #if os(macOS)
        let image = try await NSImage(importing: imageData, contentType: .image)
        #else
        let image = try #require(UIImage(data: imageData))
        #endif

        #expect(image.size.width == defaultFrame.size.width * scaleFactor)
        #expect(image.size.height == defaultFrame.size.height * scaleFactor)
    }
}

#endif // ENABLE_SWIFTUI && canImport(Testing) && compiler(>=6.0)
