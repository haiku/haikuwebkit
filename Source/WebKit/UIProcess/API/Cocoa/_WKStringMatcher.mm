/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "_WKStringMatcherInternal.h"

#import "NetworkCacheData.h"
#import "SharedMemoryStringMatcher.h"
#import "WKNSData.h"
#import <WebCore/SharedBuffer.h>
#import <WebCore/SharedMemory.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <WebCore/WebKitStringMatcher.h>
#import <WebCore/WebKitStringMatcherOptions.h>
#import <wtf/FileSystem.h>

@implementation _WKStringMatcher

- (instancetype)initWithData:(NSData *)data
{
    if (!(self = [super init]))
        return nil;

    Ref sharedBuffer = WebCore::SharedBuffer::create(data);
    if (!WebCore::WebKitStringMatcher::statesAndTransitionsFromVersionedData(sharedBuffer->span()))
        return nil;
    API::Object::constructInWrapper<API::StringMatcher>(self, WTFMove(sharedBuffer));

    return self;
}

- (instancetype)initWithDataInFile:(NSURL *)fileURL
{
    if (!(self = [super init]))
        return nil;

    RELEASE_ASSERT(fileURL.isFileURL);
    NSString *path = fileURL.path;
    if (!FileSystem::makeSafeToUseMemoryMapForPath(path))
        return nil;
    auto fileData = WebKit::NetworkCache::mapFile(path);
    if (fileData.isNull())
        return nil;
    RefPtr sharedMemory = fileData.tryCreateSharedMemory();
    if (!sharedMemory)
        return nil;
    if (!WebCore::WebKitStringMatcher::statesAndTransitionsFromVersionedData(sharedMemory->span()))
        return nil;
    API::Object::constructInWrapper<API::StringMatcher>(self, WTFMove(sharedMemory));

    return self;
}

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKStringMatcher.class, self))
        return;
    SUPPRESS_UNRETAINED_ARG _matcher->API::StringMatcher::~StringMatcher();
    [super dealloc];
}

+ (NSData *)matcherDataForStringsAndIdentifiers:(NSDictionary<NSString *, NSNumber *> *)stringsAndIdentifiers
{
    __block Vector<std::pair<String, uint16_t>> vector;
    [stringsAndIdentifiers enumerateKeysAndObjectsUsingBlock:^(NSString *string, NSNumber *identifier, BOOL *stop) {
        vector.append({ string, identifier.unsignedShortValue });
    }];
    auto dfa = WebCore::WebKitStringMatcher::dataForMatchingStrings(vector);
    if (!dfa)
        return nil;
    return wrapper(API::Data::create(spanReinterpretCast<const uint8_t>(dfa->span()))).autorelease();
}

// FIXME: Remove this when it is no longer useful for helping Safari transition away from the injected bundle.
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
- (NSArray *)resultsForMatchingCharacters:(const unichar*)characters length:(size_t)length matchAll:(BOOL)matchAll searchReverse:(BOOL)searchReverse
{
    RefPtr matcher = WebKit::SharedMemoryStringMatcher::create(Ref { *_matcher }->sharedBuffer());
    if (!matcher) {
        ASSERT_NOT_REACHED();
        return @[];
    }

    std::span<const char16_t> characterSpan { reinterpret_cast<const char16_t*>(characters), length };
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    auto matches = matcher->match(characterSpan, WebCore::WebKitStringMatcherOptions {
        !!matchAll,
        !!searchReverse
    });
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:matches.size()];
    for (auto& matchInfo : matches) {
        [result addObject:@{
            @"substring": adoptNS([[NSString alloc] initWithCharacters:characters + matchInfo.substringBeginIndex length:matchInfo.substringEndIndex - matchInfo.substringBeginIndex + 1]).get(),
            @"index": @(matchInfo.substringBeginIndex),
            @"identifier": @(matchInfo.identifier)
        }];
    }
    return result;
}

- (API::Object&)_apiObject
{
    return *_matcher;
}

@end
