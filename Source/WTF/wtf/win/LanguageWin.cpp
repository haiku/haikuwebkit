/*
 * Copyright (C) 2007, 2016, 2017 Apple Inc. All rights reserved.
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
#include <wtf/Language.h>

#include <mutex>
#include <windows.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>
#include <wtf/text/win/WCharStringExtras.h>

namespace WTF {

static Lock platformLanguageMutex;

static String localeInfo(LCTYPE localeType, const String& fallback)
{
    LANGID langID = GetUserDefaultUILanguage();
    int localeChars = GetLocaleInfo(langID, localeType, nullptr, 0);
    if (!localeChars)
        return fallback;
    std::span<char16_t> localeNameBuf;
    String localeName = String::createUninitialized(localeChars, localeNameBuf);
    localeChars = GetLocaleInfo(langID, localeType, wcharFrom(localeNameBuf.data()), localeChars);
    if (!localeChars)
        return fallback;
    if (localeName.isEmpty())
        return fallback;

    return localeName.left(localeName.length() - 1);
}

static String platformLanguage()
{
    Locker locker { platformLanguageMutex };

    static String computedDefaultLanguage;
    if (!computedDefaultLanguage.isEmpty())
        return computedDefaultLanguage.isolatedCopy();

    String languageName = localeInfo(LOCALE_SISO639LANGNAME, "en"_s);
    String countryName = localeInfo(LOCALE_SISO3166CTRYNAME, String());

    if (countryName.isEmpty())
        computedDefaultLanguage = languageName;
    else
        computedDefaultLanguage = makeString(languageName, '-', countryName);

    return computedDefaultLanguage;
}

Vector<String> platformUserPreferredLanguages(ShouldMinimizeLanguages)
{
    return { platformLanguage() };
}

} // namespace WTF
