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
        .click, .hover, .touch, .wheel, .gesture, .pointer, .keyboard, .focus, .form, .media,
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
        case .gesture:
            "gesture"
        case .pointer:
            "pointer"
        case .keyboard:
            "keyboard"
        case .focus:
            "focus"
        case .form:
            "form"
        case .media:
            "media"
        default:
            "unknown"
        }
    }
}

private func eventListenerTypesAsArray(eventListeners: WKTextExtractionEventListenerTypes) -> [String] {
    WKTextExtractionEventListenerTypes.all.compactMap { eventListeners.contains($0) ? $0.description : nil }
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

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

@_objcImplementation
extension WKTextExtractionSelectItem {
    let selectedValues: [String]

    @nonobjc
    private let backingSupportsMultiple: Bool
    @objc(multiple)
    var supportsMultiple: Bool {
        @objc(supportsMultiple)
        get { backingSupportsMultiple }
    }

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
        self.backingSupportsMultiple = supportsMultiple
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

    #if compiler(<6.0)
    @objc
    deinit {}
    #endif
}

#endif // USE_APPLE_INTERNAL_SDK || (!os(tvOS) && !os(watchOS))
