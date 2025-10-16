// Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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

#if USE_APPLE_INTERNAL_SDK || (!os(tvOS) && !os(watchOS))

import Foundation
#if compiler(>=6.0)
internal import WebKit_Internal
#else
@_implementationOnly import WebKit_Internal
#endif

// FIXME: Adopt `@objc @implementation` when support for macOS Sonoma is no longer needed.
// FIXME: (rdar://110719676) Remove all `@objc deinit`s when support for macOS Sonoma is no longer needed.

extension WKTextExtractionEventListenerTypes {
    static let all: [WKTextExtractionEventListenerTypes] = [
        .click, .hover, .touch, .wheel, .keyboard,
    ]

    fileprivate var description: String {
        switch self {
        case .click:
            "click"
        case .hover:
            "hover"
        case .touch:
            "touch"
        case .wheel:
            "wheel"
        case .keyboard:
            "keyboard"
        default:
            "unknown"
        }
    }
}

private func eventListenerTypesAsArray(eventListeners: WKTextExtractionEventListenerTypes) -> [String] {
    WKTextExtractionEventListenerTypes.all.compactMap { eventListeners.contains($0) ? $0.description : nil }
}

extension String {
    fileprivate var escaped: String {
        self
            .replacingOccurrences(of: "\\", with: "\\\\")
            .replacingOccurrences(of: "\n", with: "\\n")
            .replacingOccurrences(of: "\r", with: "\\r")
            .replacingOccurrences(of: "\t", with: "\\t")
            .replacingOccurrences(of: "'", with: "\\'")
            .replacingOccurrences(of: "\"", with: "\\\"")
            .replacingOccurrences(of: "\0", with: "\\0")
            .replacingOccurrences(of: "\u{08}", with: "\\b")
            .replacingOccurrences(of: "\u{0C}", with: "\\f")
            .replacingOccurrences(of: "\u{0B}", with: "\\v")
    }
}

@_objcImplementation
extension WKTextExtractionItem {
    let rectInWebView: CGRect
    let children: [WKTextExtractionItem]
    let eventListeners: WKTextExtractionEventListenerTypes
    let ariaAttributes: [String: String]
    let accessibilityRole: String
    let nodeIdentifier: String?

    @objc
    fileprivate init(
        with rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.rectInWebView = rectInWebView
        self.children = children
        self.eventListeners = eventListeners
        self.nodeIdentifier = nodeIdentifier
        self.ariaAttributes = ariaAttributes
        self.accessibilityRole = accessibilityRole
    }

    @objc
    fileprivate func textRepresentationRecursive(depth: Int) -> String {
        let indent = String(repeating: "\t", count: depth)

        var result = "\(indent)\(textRepresentationParts.joined(separator: ","))"

        if let text = children.first as? WKTextExtractionTextItem, children.count == 1 {
            result += ",\(text.textRepresentationParts.joined(separator: ","))\n"
        } else if !children.isEmpty {
            result += "\n"
            for child in children {
                result += child.textRepresentationRecursive(depth: depth + 1)
            }
        } else {
            result += "\n"
        }

        return result
    }

    @objc
    fileprivate var textRepresentationParts: [String] {
        var parts: [String] = []

        if let nodeIdentifier, !nodeIdentifier.isEmpty {
            parts.append("uid=\(nodeIdentifier)")
        }

        let origin = rectInWebView.origin
        let size = rectInWebView.size

        if children.isEmpty {
            parts.append("[\(Int(origin.x)),\(Int(origin.y));\(Int(size.width))x\(Int(size.height))]")
        }

        if !accessibilityRole.isEmpty {
            parts.append("role='\(accessibilityRole.escaped)'")
        }

        let listeners = eventListenerTypesAsArray(eventListeners: eventListeners)
        if !listeners.isEmpty {
            parts.append("events=[\(listeners.joined(separator: ","))]")
        }

        for (key, value) in ariaAttributes.sorted(by: { $0.key < $1.key }) {
            parts.append("\(key)='\(value.escaped)'")
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionContainerItem {
    let container: WKTextExtractionContainer

    init(
        container: WKTextExtractionContainer,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.container = container
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        var containerString: String? = nil
        switch container {
        case .root:
            containerString = "root"
        case .viewportConstrained:
            containerString = "overlay"
        case .list:
            containerString = "list"
        case .listItem:
            containerString = "list-item"
        case .blockQuote:
            containerString = "block-quote"
        case .article:
            containerString = "article"
        case .section:
            containerString = "section"
        case .nav:
            containerString = "navigation"
        case .button:
            containerString = "button"
        case .generic:
            break
        @unknown default:
            break
        }

        if let containerString {
            parts.insert(containerString, at: 0)
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionContentEditableItem {
    fileprivate let contentEditableType: WKTextExtractionEditableType

    @nonobjc
    private let backingIsFocused: Bool
    @objc(focused)
    var isFocused: Bool {
        @objc(isFocused)
        get { backingIsFocused }
    }

    init(
        contentEditableType: WKTextExtractionEditableType,
        isFocused: Bool,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.contentEditableType = contentEditableType
        self.backingIsFocused = isFocused
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        parts.insert("contentEditable", at: 0)

        if isFocused {
            parts.append("focused")
        }

        if contentEditableType == .plainTextOnly {
            parts.append("plaintext")
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionTextFormControlItem {
    fileprivate let editable: WKTextExtractionEditable

    @objc(secure)
    var isSecure: Bool {
        editable.isSecure
    }

    @objc(focused)
    var isFocused: Bool {
        editable.isFocused
    }

    @objc
    var label: String {
        editable.label
    }

    @objc
    var placeholder: String {
        editable.placeholder
    }

    let controlType: String
    let autocomplete: String

    @nonobjc
    private let backingIsReadonly: Bool
    @objc(readonly)
    var isReadonly: Bool {
        @objc(isReadonly)
        get { backingIsReadonly }
    }

    @nonobjc
    private let backingIsDisabled: Bool
    @objc(disabled)
    var isDisabled: Bool {
        @objc(isDisabled)
        get { backingIsDisabled }
    }

    @nonobjc
    private let backingIsChecked: Bool
    @objc(checked)
    var isChecked: Bool {
        @objc(isChecked)
        get { backingIsChecked }
    }

    init(
        editable: WKTextExtractionEditable,
        controlType: String,
        autocomplete: String,
        isReadonly: Bool,
        isDisabled: Bool,
        isChecked: Bool,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.editable = editable
        self.controlType = controlType
        self.autocomplete = autocomplete
        self.backingIsReadonly = isReadonly
        self.backingIsDisabled = isDisabled
        self.backingIsChecked = isChecked
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        parts.insert("textFormControl", at: 0)

        if !controlType.isEmpty {
            parts.insert("'\(controlType)'", at: 1)
        }

        if !autocomplete.isEmpty {
            parts.append("autocomplete='\(autocomplete)'")
        }

        if isReadonly {
            parts.append("readonly")
        }

        if isDisabled {
            parts.append("disabled")
        }

        if isChecked {
            parts.append("checked")
        }

        if !editable.label.isEmpty {
            parts.append("label='\(editable.label.escaped)'")
        }

        if !editable.placeholder.isEmpty {
            parts.append("placeholder='\(editable.placeholder.escaped)'")
        }

        if editable.isSecure {
            parts.append("secure")
        }

        if editable.isFocused {
            parts.append("focused")
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionEditable {
    let label: String
    let placeholder: String

    // Properties with a customized getter are incorrectly mapped when using ObjCImplementation.
    @nonobjc
    private let backingIsSecure: Bool
    @objc(secure)
    var isSecure: Bool {
        @objc(isSecure)
        get { backingIsSecure }
    }

    // Properties with a customized getter are incorrectly mapped when using ObjCImplementation.
    @nonobjc
    private let backingIsFocused: Bool
    @objc(focused)
    var isFocused: Bool {
        @objc(isFocused)
        get { backingIsFocused }
    }

    init(label: String, placeholder: String, isSecure: Bool, isFocused: Bool) {
        self.label = label
        self.placeholder = placeholder
        self.backingIsSecure = isSecure
        self.backingIsFocused = isFocused
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionLinkItem {
    let target: String
    @nonobjc
    private let backingURL: NSURL

    var url: URL { backingURL as URL }

    init(
        target: String,
        url: URL,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.target = target
        self.backingURL = url as NSURL
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        parts.insert("link", at: 0)
        parts.append("url='\(url.absoluteString.escaped)'")

        if !target.isEmpty {
            parts.append("target='\(target.escaped)'")
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionLink {
    // Used to workaround the fact that `@_objcImplementation` does not support stored properties whose size can change
    // due to Library Evolution. Do not use this property directly.
    @nonobjc
    private let backingURL: NSURL

    var url: URL { backingURL as URL }

    let range: NSRange

    @objc(initWithURL:range:)
    init(url: URL, range: NSRange) {
        self.backingURL = url as NSURL
        self.range = range
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionTextItem {
    let content: String
    let selectedRange: NSRange
    let links: [WKTextExtractionLink]
    let editable: WKTextExtractionEditable?

    init(
        content: String,
        selectedRange: NSRange,
        links: [WKTextExtractionLink],
        editable: WKTextExtractionEditable?,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.content = content
        self.selectedRange = selectedRange
        self.links = links
        self.editable = editable
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        if !content.isEmpty {
            let trimmedContent = content.trimmingCharacters(in: .newlines)
            parts.insert("'\(trimmedContent.escaped)'", at: 0)

            guard let originalRange = Range(selectedRange, in: content) else {
                return parts
            }

            let leadingNewlineCount = content.prefix(while: \.isNewline).count
            if selectedRange.length > 0 && !trimmedContent.isEmpty {
                let newLocation = max(0, selectedRange.location - leadingNewlineCount)
                let maxLength = trimmedContent.count - newLocation
                let newLength = min(selectedRange.length, max(0, maxLength))
                if newLocation < trimmedContent.count && newLength > 0 {
                    let adjustedNSRange = NSRange(location: newLocation, length: newLength)
                    if let adjustedRange = Range(adjustedNSRange, in: trimmedContent) {
                        let startOffset = trimmedContent.distance(from: trimmedContent.startIndex, to: adjustedRange.lowerBound)
                        let endOffset = trimmedContent.distance(from: trimmedContent.startIndex, to: adjustedRange.upperBound)
                        parts.append("selected=[\(startOffset),\(endOffset)]")
                    }
                } else {
                    parts.append("selected=[0,0]")
                }
            } else if trimmedContent.isEmpty {
                parts.append("selected=[0,0]")
            } else {
                let startOffset = content.distance(from: content.startIndex, to: originalRange.lowerBound)
                let endOffset = content.distance(from: content.startIndex, to: originalRange.upperBound)
                if startOffset >= 0 && endOffset >= startOffset {
                    parts.append("selected=[\(startOffset),\(endOffset)]")
                }
            }
        } else if selectedRange.length > 0 {
            parts.append("selected=[\(selectedRange.location),\(selectedRange.location + selectedRange.length)]")
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionScrollableItem {
    let contentSize: CGSize

    init(
        contentSize: CGSize,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.contentSize = contentSize
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        parts.insert("scrollable", at: 0)
        parts.append("contentSize=[\(contentSize.width)x\(contentSize.height)]")

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionSelectItem {
    let selectedValues: [String]
    let supportsMultiple: Bool

    init(
        selectedValues: [String],
        supportsMultiple: Bool,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.selectedValues = selectedValues
        self.supportsMultiple = supportsMultiple
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        parts.insert("select", at: 0)

        if !selectedValues.isEmpty {
            let escapedValues = selectedValues.map { "'\($0.escaped)'" }
            parts.append("selected=[\(escapedValues.joined(separator: ","))]")
        }

        if supportsMultiple {
            parts.append("multiple")
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionImageItem {
    let name: String
    let altText: String

    init(
        name: String,
        altText: String,
        rectInWebView: CGRect,
        children: [WKTextExtractionItem],
        eventListeners: WKTextExtractionEventListenerTypes,
        ariaAttributes: [String: String],
        accessibilityRole: String,
        nodeIdentifier: String?
    ) {
        self.name = name
        self.altText = altText
        super
            .init(
                with: rectInWebView,
                children: children,
                eventListeners: eventListeners,
                ariaAttributes: ariaAttributes,
                accessibilityRole: accessibilityRole,
                nodeIdentifier: nodeIdentifier
            )
    }

    @objc
    override fileprivate var textRepresentationParts: [String] {
        var parts = super.textRepresentationParts

        parts.insert("image", at: 0)

        if !name.isEmpty {
            parts.append("name='\(name.escaped)'")
        }

        if !altText.isEmpty {
            parts.append("alt='\(altText.escaped)'")
        }

        return parts
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionPopupMenu {
    let itemTitles: [String]

    init(itemTitles: [String]) {
        self.itemTitles = itemTitles
    }

    fileprivate var textRepresentation: String {
        let escapedTitles = itemTitles.map { "'\($0.escaped)'" }
        return "nativePopupMenu,items=[\(escapedTitles.joined(separator: ","))]\n"
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionResult {
    let rootItem: WKTextExtractionItem
    let popupMenu: WKTextExtractionPopupMenu?

    init(rootItem: WKTextExtractionItem, popupMenu: WKTextExtractionPopupMenu?) {
        self.rootItem = rootItem
        self.popupMenu = popupMenu
    }

    @objc
    var textRepresentation: String {
        let popupMenuRepresentation = popupMenu?.textRepresentation ?? ""
        return "\(rootItem.textRepresentationRecursive(depth: 0))\(popupMenuRepresentation)"
    }

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

#endif // USE_APPLE_INTERNAL_SDK || (!os(tvOS) && !os(watchOS))
