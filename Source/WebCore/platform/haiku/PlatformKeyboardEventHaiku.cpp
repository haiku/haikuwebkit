/*
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2008 Andrea Anzani <andrea.anzani@gmail.com>
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

#include <String.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/HexNumber.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringConcatenate.h>

#include "PlatformKeyboardEvent.h"

#include "NotImplemented.h"
#include "WindowsKeyboardCodes.h"
#include <InterfaceDefs.h>
#include <Message.h>


namespace WebCore {

static String keyIdentifierForHaikuKeyCode(char singleByte, int keyCode)
{
    switch (singleByte) {
    case B_FUNCTION_KEY:

        switch (keyCode) {
        case B_F1_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F1");
        case B_F2_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F2");
        case B_F3_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F3");
        case B_F4_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F4");
        case B_F5_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F5");
        case B_F6_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F6");
        case B_F7_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F7");
        case B_F8_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F8");
        case B_F9_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F9");
        case B_F10_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F10");
        case B_F11_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F11");
        case B_F12_KEY:
            return ASCIILiteral::fromLiteralUnsafe("F12");
        case B_PRINT_KEY:
            return ASCIILiteral::fromLiteralUnsafe("Print");
        case B_PAUSE_KEY:
            return ASCIILiteral::fromLiteralUnsafe("Pause");
        case B_SCROLL_KEY:
            return ASCIILiteral::fromLiteralUnsafe(""); // FIXME
        }
    break;

    case B_BACKSPACE:
        return ASCIILiteral::fromLiteralUnsafe("U+0008");
    case B_LEFT_ARROW:
        return ASCIILiteral::fromLiteralUnsafe("Left");
    case B_RIGHT_ARROW:
        return ASCIILiteral::fromLiteralUnsafe("Right");
    case B_UP_ARROW:
        return ASCIILiteral::fromLiteralUnsafe("Up");
    case B_DOWN_ARROW:
        return ASCIILiteral::fromLiteralUnsafe("Down");
    case B_INSERT:
        return ASCIILiteral::fromLiteralUnsafe("Insert");
    case B_ENTER:
        return ASCIILiteral::fromLiteralUnsafe("Enter");
    case B_DELETE:
        return ASCIILiteral::fromLiteralUnsafe("U+007F");
    case B_HOME:
        return ASCIILiteral::fromLiteralUnsafe("Home");
    case B_END:
        return ASCIILiteral::fromLiteralUnsafe("End");
    case B_PAGE_UP:
        return ASCIILiteral::fromLiteralUnsafe("PageUp");
    case B_PAGE_DOWN:
        return ASCIILiteral::fromLiteralUnsafe("PageDown");
    case B_TAB:
        return ASCIILiteral::fromLiteralUnsafe("U+0009");
    }

	// FIXME will not work for non-ASCII characters
    return makeString("U+", hex(toASCIIUpper(singleByte)), 4);
}

static int windowsKeyCodeForKeyEvent(char singleByte, int keyCode)
{
    switch (singleByte) {
    case B_FUNCTION_KEY:
        switch (keyCode) {
        case B_F1_KEY:
            return VK_F1;
        case B_F2_KEY:
            return VK_F2;
        case B_F3_KEY:
            return VK_F3;
        case B_F4_KEY:
            return VK_F4;
        case B_F5_KEY:
            return VK_F5;
        case B_F6_KEY:
            return VK_F6;
        case B_F7_KEY:
            return VK_F7;
        case B_F8_KEY:
            return VK_F8;
        case B_F9_KEY:
            return VK_F9;
        case B_F10_KEY:
            return VK_F10;
        case B_F11_KEY:
            return VK_F11;
        case B_F12_KEY:
            return VK_F12;
        case B_PRINT_KEY:
            return VK_PRINT;
        case B_PAUSE_KEY:
            return 0; // FIXME
        case B_SCROLL_KEY:
            return 0; // FIXME
        }
        break;

    case B_BACKSPACE:
        return VK_BACK; // (08) BACKSPACE key
    case B_TAB:
        return VK_TAB; // (09) TAB key
    case B_RETURN:
        return VK_RETURN; //(0D) Return key
    case B_ESCAPE:
        return VK_ESCAPE; // (1B) ESC key
    case B_SPACE:
        return VK_SPACE; // (20) SPACEBAR
    case B_PAGE_UP:
        return VK_PRIOR; // (21) PAGE UP key
    case B_PAGE_DOWN:
        return VK_NEXT; // (22) PAGE DOWN key
    case B_END:
        return VK_END; // (23) END key
    case B_HOME:
        return VK_HOME; // (24) HOME key
    case B_LEFT_ARROW:
        return VK_LEFT; // (25) LEFT ARROW key
    case B_UP_ARROW:
        return VK_UP; // (26) UP ARROW key
    case B_RIGHT_ARROW:
        return VK_RIGHT; // (27) RIGHT ARROW key
    case B_DOWN_ARROW:
        return VK_DOWN; // (28) DOWN ARROW key
    case B_INSERT:
        return VK_INSERT; // (2D) INS key
    case B_DELETE:
        return VK_DELETE; // (2E) DEL key

    case '0':
    case ')':
        return VK_0;
    case '1':
    case '!':
        return VK_1;
    case '2':
    case '@':
        return VK_2;
    case '3':
    case '#':
        return VK_3;
    case '4':
    case '$':
        return VK_4;
    case '5':
    case '%':
        return VK_5;
    case '6':
    case '^':
        return VK_6;
    case '7':
    case '&':
        return VK_7;
    case '8':
    case '*':
        return VK_8;
    case '9':
    case '(':
        return VK_9;
    case 'a':
    case 'A':
        return VK_A;
    case 'b':
    case 'B':
        return VK_B;
    case 'c':
    case 'C':
        return VK_C;
    case 'd':
    case 'D':
        return VK_D;
    case 'e':
    case 'E':
        return VK_E;
    case 'f':
    case 'F':
        return VK_F;
    case 'g':
    case 'G':
        return VK_G;
    case 'h':
    case 'H':
        return VK_H;
    case 'i':
    case 'I':
        return VK_I;
    case 'j':
    case 'J':
        return VK_J;
    case 'k':
    case 'K':
        return VK_K;
    case 'l':
    case 'L':
        return VK_L;
    case 'm':
    case 'M':
        return VK_M;
    case 'n':
    case 'N':
        return VK_N;
    case 'o':
    case 'O':
        return VK_O;
    case 'p':
    case 'P':
        return VK_P;
    case 'q':
    case 'Q':
        return VK_Q;
    case 'r':
    case 'R':
        return VK_R;
    case 's':
    case 'S':
        return VK_S;
    case 't':
    case 'T':
        return VK_T;
    case 'u':
    case 'U':
        return VK_U;
    case 'v':
    case 'V':
        return VK_V;
    case 'w':
    case 'W':
        return VK_W;
    case 'x':
    case 'X':
        return VK_X;
    case 'y':
    case 'Y':
        return VK_Y;
    case 'z':
    case 'Z':
        return VK_Z;
    case ';':
    case ':':
        return VK_OEM_1;
    case '+':
    case '=':
        return VK_OEM_PLUS;
    case ',':
    case '<':
        return VK_OEM_COMMA;
    case '-':
    case '_':
        return VK_OEM_MINUS;
    case '.':
    case '>':
        return VK_OEM_PERIOD;
    case '/':
    case '?':
        return VK_OEM_2;
    case '`':
    case '~':
        return VK_OEM_3;
    case '[':
    case '{':
        return VK_OEM_4;
    case '\\':
    case '|':
        return VK_OEM_5;
    case ']':
    case '}':
        return VK_OEM_6;
    case '\'':
    case '"':
        return VK_OEM_7;
    }
    return singleByte;
}

static String KeyValueForKeyEvent(BString bytes, int keyCode)
{
    switch (bytes.ByteAt(0)) {

        case B_FUNCTION_KEY:
            switch (keyCode) {
                case B_F1_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F1");
                case B_F2_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F2");
                case B_F3_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F3");
                case B_F4_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F4");
                case B_F5_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F5");
                case B_F6_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F6");
                case B_F7_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F7");
                case B_F8_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F8");
                case B_F9_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F9");
                case B_F10_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F10");
                case B_F11_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F11");
                case B_F12_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("F12");
                case B_PRINT_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("Print");
                case B_PAUSE_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("Pause");
                case B_SCROLL_KEY:
                    return ASCIILiteral::fromLiteralUnsafe("ScrollLock");
            }
            break;

        case B_BACKSPACE:
            return ASCIILiteral::fromLiteralUnsafe("Backspace");
        case B_LEFT_ARROW:
            return ASCIILiteral::fromLiteralUnsafe("ArrowLeft");
        case B_RIGHT_ARROW:
            return ASCIILiteral::fromLiteralUnsafe("ArrowRight");
        case B_UP_ARROW:
            return ASCIILiteral::fromLiteralUnsafe("ArrowUp");
        case B_DOWN_ARROW:
            return ASCIILiteral::fromLiteralUnsafe("ArrowDown");
        case B_INSERT:
            return ASCIILiteral::fromLiteralUnsafe("Insert");
        case B_ENTER:
            return ASCIILiteral::fromLiteralUnsafe("Enter");
        case B_DELETE:
            return ASCIILiteral::fromLiteralUnsafe("Delete");
        case B_HOME:
            return ASCIILiteral::fromLiteralUnsafe("Home");
        case B_END:
            return ASCIILiteral::fromLiteralUnsafe("End");
        case B_PAGE_UP:
            return ASCIILiteral::fromLiteralUnsafe("PageUp");
        case B_PAGE_DOWN:
            return ASCIILiteral::fromLiteralUnsafe("PageDown");
        case B_TAB:
            return ASCIILiteral::fromLiteralUnsafe("Tab");
        case B_SPACE:
            return ASCIILiteral::fromLiteralUnsafe(" "); // (20) SPACEBAR

        case B_ESCAPE:
            return ASCIILiteral::fromLiteralUnsafe("Escape");

        default: {
            return String::fromUTF8(bytes);
        }
    }
    return ASCIILiteral::fromLiteralUnsafe("Unidentified");
}

static String KeyCodeForKeyEvent(int keyCode)
{
    switch (keyCode) {
        case 0x0001:
            return ASCIILiteral::fromLiteralUnsafe("Escape");
        case 0x0002:
            return ASCIILiteral::fromLiteralUnsafe("F1");
        case 0x0003:
            return ASCIILiteral::fromLiteralUnsafe("F2");
        case 0x0004:
            return ASCIILiteral::fromLiteralUnsafe("F3");
        case 0x0005:
            return ASCIILiteral::fromLiteralUnsafe("F4");
        case 0x0006:
            return ASCIILiteral::fromLiteralUnsafe("F5");
        case 0x0007:
            return ASCIILiteral::fromLiteralUnsafe("F6");
        case 0x0008:
            return ASCIILiteral::fromLiteralUnsafe("F7");
        case 0x0009:
            return ASCIILiteral::fromLiteralUnsafe("F8");
        case 0x000A:
            return ASCIILiteral::fromLiteralUnsafe("F9");
        case 0x000B:
            return ASCIILiteral::fromLiteralUnsafe("F10");
        case 0x000C:
            return ASCIILiteral::fromLiteralUnsafe("F11");
        case 0x000D:
            return ASCIILiteral::fromLiteralUnsafe("F12");

        case 0x000E:
            return ASCIILiteral::fromLiteralUnsafe("PrintScreen");
        case 0x000F:
            return ASCIILiteral::fromLiteralUnsafe("ScrollLock");
        case 0x0010:
            return ASCIILiteral::fromLiteralUnsafe("Pause");

        case 0x0011:
            return ASCIILiteral::fromLiteralUnsafe("Backquote");
        case 0x0012:
            return ASCIILiteral::fromLiteralUnsafe("Digit1");
        case 0x0013:
            return ASCIILiteral::fromLiteralUnsafe("Digit2");
        case 0x0014:
            return ASCIILiteral::fromLiteralUnsafe("Digit3");
        case 0x0015:
            return ASCIILiteral::fromLiteralUnsafe("Digit4");
        case 0x0016:
            return ASCIILiteral::fromLiteralUnsafe("Digit5");
        case 0x0017:
            return ASCIILiteral::fromLiteralUnsafe("Digit6");
        case 0x0018:
            return ASCIILiteral::fromLiteralUnsafe("Digit7");
        case 0x0019:
            return ASCIILiteral::fromLiteralUnsafe("Digit8");
        case 0x001A:
            return ASCIILiteral::fromLiteralUnsafe("Digit9");
        case 0x001B:
            return ASCIILiteral::fromLiteralUnsafe("Digit0");
        case 0x001C:
            return ASCIILiteral::fromLiteralUnsafe("Minus");
        case 0x001D:
            return ASCIILiteral::fromLiteralUnsafe("Equal");
        case 0x001E:
            return ASCIILiteral::fromLiteralUnsafe("Backspace"); // IntYen

        case 0x001F:
            return ASCIILiteral::fromLiteralUnsafe("Insert");
        case 0x0020:
            return ASCIILiteral::fromLiteralUnsafe("Home");
        case 0x0021:
            return ASCIILiteral::fromLiteralUnsafe("PageUp");
            
        case 0x0022:
            return ASCIILiteral::fromLiteralUnsafe("NumLock");
        case 0x0023:
            return ASCIILiteral::fromLiteralUnsafe("NumpadDivide");
        case 0x0024:
            return ASCIILiteral::fromLiteralUnsafe("NumpadMultiply");
        case 0x0025:
            return ASCIILiteral::fromLiteralUnsafe("NumpadSubtract");

        case 0x0026:
            return ASCIILiteral::fromLiteralUnsafe("Tab");
        case 0x0027:
            return ASCIILiteral::fromLiteralUnsafe("KeyQ");
        case 0x0028:
            return ASCIILiteral::fromLiteralUnsafe("KeyW");
        case 0x0029:
            return ASCIILiteral::fromLiteralUnsafe("KeyE");
        case 0x002A:
            return ASCIILiteral::fromLiteralUnsafe("KeyR");
        case 0x002B:
            return ASCIILiteral::fromLiteralUnsafe("KeyT");
        case 0x002C:
            return ASCIILiteral::fromLiteralUnsafe("KeyY");
        case 0x002D:
            return ASCIILiteral::fromLiteralUnsafe("KeyU");
        case 0x002E:
            return ASCIILiteral::fromLiteralUnsafe("KeyI");
        case 0x002F:
            return ASCIILiteral::fromLiteralUnsafe("KeyO");
        case 0x0030:
            return ASCIILiteral::fromLiteralUnsafe("KepP");
        case 0x0031:
            return ASCIILiteral::fromLiteralUnsafe("BracketLeft");
        case 0x0032:
            return ASCIILiteral::fromLiteralUnsafe("BracketRight");
        case 0x0033:
            return ASCIILiteral::fromLiteralUnsafe("Backslash");

        case 0x0034:
            return ASCIILiteral::fromLiteralUnsafe("Delete");
        case 0x0035:
            return ASCIILiteral::fromLiteralUnsafe("End");
        case 0x0036:
            return ASCIILiteral::fromLiteralUnsafe("PageDown");

        case 0x0037:
            return ASCIILiteral::fromLiteralUnsafe("Numpad7");
        case 0x0038:
            return ASCIILiteral::fromLiteralUnsafe("Numpad8");
        case 0x0039:
            return ASCIILiteral::fromLiteralUnsafe("Numpad9");
        case 0x003A:
            return ASCIILiteral::fromLiteralUnsafe("NumpadAdd");

        case 0x003B:
            return ASCIILiteral::fromLiteralUnsafe("CapsLock");
        case 0x003C:
            return ASCIILiteral::fromLiteralUnsafe("KeyA");
        case 0x003D:
            return ASCIILiteral::fromLiteralUnsafe("KeyS");
        case 0x003E:
            return ASCIILiteral::fromLiteralUnsafe("KeyD");
        case 0x003F:
            return ASCIILiteral::fromLiteralUnsafe("KeyF");
        case 0x0040:
            return ASCIILiteral::fromLiteralUnsafe("KeyG");
        case 0x0041:
            return ASCIILiteral::fromLiteralUnsafe("KeyH");
        case 0x0042:
            return ASCIILiteral::fromLiteralUnsafe("KeyJ");
        case 0x0043:
            return ASCIILiteral::fromLiteralUnsafe("KeyK");
        case 0x0044:
            return ASCIILiteral::fromLiteralUnsafe("KeyL");
        case 0x0045:
            return ASCIILiteral::fromLiteralUnsafe("Semicolon");
        case 0x0046:
            return ASCIILiteral::fromLiteralUnsafe("Quote");
        case 0x0047:
            return ASCIILiteral::fromLiteralUnsafe("Return");

        case 0x0048:
            return ASCIILiteral::fromLiteralUnsafe("Numpad4");
        case 0x0049:
            return ASCIILiteral::fromLiteralUnsafe("Numpad5");
        case 0x004A:
            return ASCIILiteral::fromLiteralUnsafe("Numpad6");

        case 0x004B:
            return ASCIILiteral::fromLiteralUnsafe("ShiftLeft");
        case 0x004C:
            return ASCIILiteral::fromLiteralUnsafe("KeyZ");
        case 0x004D:
            return ASCIILiteral::fromLiteralUnsafe("KeyX");
        case 0x004E:
            return ASCIILiteral::fromLiteralUnsafe("KeyC");
        case 0x004F:
            return ASCIILiteral::fromLiteralUnsafe("KeyV");
        case 0x0050:
            return ASCIILiteral::fromLiteralUnsafe("KeyB");
        case 0x0051:
            return ASCIILiteral::fromLiteralUnsafe("KeyN");
        case 0x0052:
            return ASCIILiteral::fromLiteralUnsafe("KeyM");
        case 0x0053:
            return ASCIILiteral::fromLiteralUnsafe("Comma");
        case 0x0054:
            return ASCIILiteral::fromLiteralUnsafe("Period");
        case 0x0055:
            return ASCIILiteral::fromLiteralUnsafe("Slash");
        case 0x0056:
            return ASCIILiteral::fromLiteralUnsafe("ShiftRight");

        case 0x0057:
            return ASCIILiteral::fromLiteralUnsafe("ArrowUp");
        case 0x0058:
            return ASCIILiteral::fromLiteralUnsafe("Digit1");
        case 0x0059:
            return ASCIILiteral::fromLiteralUnsafe("Digit2");
        case 0x005A:
            return ASCIILiteral::fromLiteralUnsafe("Digit3");
        case 0x005B:
            return ASCIILiteral::fromLiteralUnsafe("NumpadEnter");
        case 0x005C:
            return ASCIILiteral::fromLiteralUnsafe("ControlLeft");
        case 0x005D:
            return ASCIILiteral::fromLiteralUnsafe("AltLeft");
        case 0x005E:
            return ASCIILiteral::fromLiteralUnsafe("Space");
        case 0x005F:
            return ASCIILiteral::fromLiteralUnsafe("AltRight");
        case 0x0060:
            return ASCIILiteral::fromLiteralUnsafe("ControlRight");
        case 0x0061:
            return ASCIILiteral::fromLiteralUnsafe("ArrowLeft");
        case 0x0062:
            return ASCIILiteral::fromLiteralUnsafe("ArrowDown");
        case 0x0063:
            return ASCIILiteral::fromLiteralUnsafe("ArrowRight");
        case 0x0064:
            return ASCIILiteral::fromLiteralUnsafe("Numpad0");
        case 0x0065:
            return ASCIILiteral::fromLiteralUnsafe("NumpadDecimal");
        case 0x0066:
            return ASCIILiteral::fromLiteralUnsafe("OSLeft"); // MetaLeft
        case 0x0067:
            return ASCIILiteral::fromLiteralUnsafe("OSRight");
        case 0x0068:
            return ASCIILiteral::fromLiteralUnsafe("ContextMenu");
        case 0x0069:
            return ASCIILiteral::fromLiteralUnsafe("IntlBackslash");
        case 0x006a:
            return ASCIILiteral::fromLiteralUnsafe("NumPadEqual");
    }
    return ASCIILiteral::fromLiteralUnsafe("Unidentified");
}

PlatformKeyboardEvent::PlatformKeyboardEvent(BMessage* message)
    : m_autoRepeat(false)
    , m_isKeypad(false)
{
    BString bytes = message->FindString("bytes");

    int32 nativeVirtualKeyCode = message->FindInt32("key");

    m_text = String::fromUTF8(std::span<const char>(bytes.String(), bytes.Length()));
    m_unmodifiedText = String(std::span<const char>(bytes.String(), bytes.Length()));
    m_keyIdentifier = keyIdentifierForHaikuKeyCode(bytes.ByteAt(0), nativeVirtualKeyCode);

    m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(bytes.ByteAt(0), nativeVirtualKeyCode);
	// TODO m_key should also do something for modifier keys, which cannot be
	// extracted from "bytes"
    m_key = KeyValueForKeyEvent(bytes, nativeVirtualKeyCode);
    m_code = KeyCodeForKeyEvent(nativeVirtualKeyCode);

    if (message->what == B_KEY_UP)
        m_type = PlatformEvent::Type::KeyUp;
    else if (message->what == B_KEY_DOWN)
        m_type = PlatformEvent::Type::KeyDown;

    int32 modifiers = message->FindInt32("modifiers");
    if (modifiers & B_SHIFT_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (modifiers & B_COMMAND_KEY)
        m_modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (modifiers & B_CONTROL_KEY)
        m_modifiers.add(PlatformEvent::Modifier::AltKey);
    if (modifiers & B_OPTION_KEY)
        m_modifiers.add(PlatformEvent::Modifier::MetaKey);
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == PlatformEvent::Type::KeyDown);
    m_type = type;

    if (backwardCompatibilityMode)
        return;

    if (type == PlatformEvent::Type::RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

OptionSet<WebCore::PlatformEvent::Modifier> PlatformKeyboardEvent::currentStateOfModifierKeys()
{
    int32 nativeModifiers = ::modifiers();
    OptionSet<Modifier> modifiers;

    if (nativeModifiers & B_SHIFT_KEY)
        modifiers.add(PlatformEvent::Modifier::ShiftKey);
    if (nativeModifiers & B_COMMAND_KEY)
        modifiers.add(PlatformEvent::Modifier::ControlKey);
    if (nativeModifiers & B_CONTROL_KEY)
        modifiers.add(PlatformEvent::Modifier::AltKey);
    if (nativeModifiers & B_OPTION_KEY)
        modifiers.add(PlatformEvent::Modifier::MetaKey);

    return modifiers;
}


} // namespace WebCore

