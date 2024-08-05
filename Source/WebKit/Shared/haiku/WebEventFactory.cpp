/*
 * Copyright (C) 2019, 2024 Haiku Inc. All rights reserved.
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
#include "WebEventFactory.h"

#include "WebEventModifier.h"
#include "WebMouseEvent.h"
#include <WebCore/IntPoint.h>
#include <wtf/WallTime.h>

#include <AppDefs.h>
#include <InterfaceDefs.h>
#include <Message.h>
#include <View.h>

namespace WebKit {
using namespace WebCore;

int32_t WebEventFactory::currentMouseButtons = 0;
WebMouseEventButton WebEventFactory::currentMouseButton = WebMouseEventButton::None;

WebMouseEvent WebEventFactory::createWebMouseEvent(const BMessage* message)
{
    WebEventType type;
    switch (message->what) {
    case B_MOUSE_DOWN:
        type = WebEventType::MouseDown;
        break;
    case B_MOUSE_UP:
        type = WebEventType::MouseUp;
        break;
    case B_MOUSE_MOVED:
        type = WebEventType::MouseMove;
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    int32_t previousMouseButtons = currentMouseButtons;
    message->FindInt32("buttons", &currentMouseButtons);

    // It doesn't appear that the message we receive indicates which mouse
    // button was pressed to trigger the event, so we need to calculate it
    // manually.
    WebMouseEventButton button = WebMouseEventButton::None;
    if (type != WebEventType::MouseMove) {
        int32_t changedButtons = previousMouseButtons ^ currentMouseButtons;
        if (changedButtons & B_TERTIARY_MOUSE_BUTTON)
            button = WebMouseEventButton::Middle;
        if (changedButtons & B_SECONDARY_MOUSE_BUTTON)
            button = WebMouseEventButton::Right;
        if (changedButtons & B_PRIMARY_MOUSE_BUTTON)
            button = WebMouseEventButton::Left;

        // Store the current button for MouseMove events
        if (type == WebEventType::MouseDown)
            currentMouseButton = button;
        else if (type == WebEventType::MouseUp)
            currentMouseButton = WebMouseEventButton::None;
    } else {
        button = currentMouseButton;
    }

    OptionSet<WebEventModifier> modifiers;
    int32 nativeModifiers;
    message->FindInt32("modifiers", &nativeModifiers);
    if (nativeModifiers & B_SHIFT_KEY)
        modifiers.add(WebEventModifier::ShiftKey);
    if (nativeModifiers & B_COMMAND_KEY)
        modifiers.add(WebEventModifier::ControlKey);
    if (nativeModifiers & B_CONTROL_KEY)
        modifiers.add(WebEventModifier::AltKey);
    if (nativeModifiers & B_OPTION_KEY)
        modifiers.add(WebEventModifier::MetaKey);
    if (nativeModifiers & B_CAPS_LOCK)
        modifiers.add(WebEventModifier::CapsLockKey);

    BPoint globalPosition;
    message->FindPoint("screen_where", &globalPosition);

    BPoint viewPosition;
    message->FindPoint("be:view_where", &viewPosition);

    int32 clickCount;
    message->FindInt32("clicks", &clickCount);

    int32 deltaX;
    int32 deltaY;
    if (message->FindInt32("be:delta_x", &deltaX) != B_OK)
        deltaX = 0;
    if (message->FindInt32("be:delta_y", &deltaY) != B_OK)
        deltaY = 0;

    return WebMouseEvent( { type, modifiers, WallTime::now() }, currentMouseButton, currentMouseButtons,
        IntPoint(viewPosition), IntPoint(globalPosition), deltaX, deltaY, 0, clickCount);
}

}
