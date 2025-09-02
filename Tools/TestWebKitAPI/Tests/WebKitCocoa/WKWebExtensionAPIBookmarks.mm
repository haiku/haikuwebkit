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

#import "CocoaHelpers.h"
#import "config.h"
#import "HTTPServer.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionPermissionPrivate.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebExtensionControllerDelegatePrivate.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKWebExtensionBookmarks.h>


@interface _MockBookmarkNode : NSObject <_WKWebExtensionBookmark>
- (instancetype)initWithDictionary:(NSDictionary *)dictionary;
@property (nonatomic, strong) NSMutableDictionary *dictionary;
@end

@implementation _MockBookmarkNode
- (instancetype)initWithDictionary:(NSDictionary *)dictionary
{
    self = [super init];
    if (self)
        _dictionary = [dictionary mutableCopy];
    return self;
}

- (NSString *)identifierForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"id"];
}

- (NSString *)parentIdentifierForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"parentId"];
}

- (NSString *)titleForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"title"];
}

- (NSString *)urlStringForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _dictionary[@"url"];
}

- (NSInteger)indexForWebExtensionContext:(WKWebExtensionContext *)context
{
    return [_dictionary[@"index"] integerValue];
}

- (_WKWebExtensionBookmarkType)bookmarkTypeForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSString *typeString = self.dictionary[@"type"];
    if ([typeString isEqualToString:@"folder"])
        return _WKWebExtensionBookmarkTypeFolder;

    NSString *url = self.dictionary[@"url"];
    if (url && url.length > 0)
        return _WKWebExtensionBookmarkTypeBookmark;
    return _WKWebExtensionBookmarkTypeFolder;
}

- (NSArray<id<_WKWebExtensionBookmark>> *)childrenForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSArray *childDictionaries = _dictionary[@"children"];
    if (!childDictionaries)
        return nil;
    NSMutableArray<id<_WKWebExtensionBookmark>> *children = [NSMutableArray array];
    for (NSDictionary *childDict in childDictionaries)
        [children addObject:[[_MockBookmarkNode alloc] initWithDictionary:childDict]];
    return children;
}

- (NSDate *)dateAddedForWebExtensionContext:(WKWebExtensionContext *)context
{
    NSNumber *dateValue = WebKit::objectForKey<NSNumber>(_dictionary, @"dateAdded");
    if (!dateValue)
        return nil;

    double millisecondsSinceEpoch = dateValue.doubleValue;
    NSTimeInterval secondsSinceEpoch = millisecondsSinceEpoch / 1000.0;

    return [NSDate dateWithTimeIntervalSince1970:secondsSinceEpoch];
}

@end


@interface TestBookmarksDelegate : NSObject <WKWebExtensionControllerDelegatePrivate>
@property (nonatomic, strong) NSMutableArray<NSMutableDictionary *> *mockBookmarks;
@property (nonatomic) NSInteger nextMockBookmarkId;
@end

static NSMutableDictionary *findParentInMockTree(NSMutableArray<NSMutableDictionary *> *tree, NSString *parentId)
{
    for (NSMutableDictionary *node in tree) {
        if ([node[@"id"] isEqualToString:parentId])
            return node;
        id children = node[@"children"];
        if (children && [children isKindOfClass:[NSMutableArray class]]) {
            NSMutableDictionary *found = findParentInMockTree(children, parentId);
            if (found)
                return found;
        }
    }
    return nil;
}

static NSMutableDictionary *findBookmarkAndParentArrayInMockTree(NSMutableArray *children, NSString *bookmarkId)
{
    if (!children.count)
        return nil;

    for (NSMutableDictionary *childDict in children) {
        if ([childDict[@"id"] isEqualToString:bookmarkId])
            return [@{ @"bookmark": childDict, @"parentChildren": children } mutableCopy];
        if ([childDict[@"type"] isEqualToString:@"folder"] && [childDict[@"children"] isKindOfClass:[NSMutableArray class]]) {
            NSMutableDictionary *found = findBookmarkAndParentArrayInMockTree(childDict[@"children"], bookmarkId);
            if (found)
                return found;
        }
    }
    return nil;
}


@implementation TestBookmarksDelegate
- (instancetype)init
{
    self = [super init];
    if (self) {
        _mockBookmarks = [NSMutableArray array];
        _nextMockBookmarkId = 100;
    }
    return self;
}

- (void)_webExtensionController:(WKWebExtensionController *)controller bookmarksForExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSArray<NSObject<_WKWebExtensionBookmark> *> *, NSError *))completionHandler
{
    NSMutableArray<NSObject<_WKWebExtensionBookmark> *> *results = [NSMutableArray array];
    for (NSDictionary *bookmarkDict in self.mockBookmarks)
        [results addObject:[[_MockBookmarkNode alloc] initWithDictionary:bookmarkDict]];
    completionHandler(results, nil);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller createBookmarkWithParentIdentifier:(NSString *)parentId index:(NSNumber *)index url:(NSString *)url title:(NSString *)title forExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSObject<_WKWebExtensionBookmark> *, NSError *))completionHandler
{
    NSMutableDictionary *newBookmark = [NSMutableDictionary dictionary];
    newBookmark[@"title"] = title;
    if (url)
        newBookmark[@"url"] = url;
    NSString *newId = [NSString stringWithFormat:@"%ld", (long)self.nextMockBookmarkId];
    _nextMockBookmarkId++;
    newBookmark[@"id"] = newId;

    if (!newBookmark[@"type"]) {
        NSString *url = newBookmark[@"url"];
        newBookmark[@"type"] = (url && url.length > 0) ? @"bookmark" : @"folder";
    }

    if (parentId && ![parentId isEqualToString:@"0"]) {
        NSMutableDictionary *parentDict = findParentInMockTree(self.mockBookmarks, parentId);
        if (parentDict) {
            NSMutableArray *children = [parentDict[@"children"] mutableCopy] ?: [NSMutableArray array];
            newBookmark[@"parentId"] = parentId;
            newBookmark[@"index"] = index ?: @(children.count);
            [children addObject:newBookmark];
            parentDict[@"children"] = children;

            completionHandler([[_MockBookmarkNode alloc] initWithDictionary:newBookmark], nil);
            return;
        }
    }

    newBookmark[@"parentId"] = @"0";
    newBookmark[@"index"] = index ?: @(self.mockBookmarks.count);
    [self.mockBookmarks addObject:newBookmark];

    completionHandler([[_MockBookmarkNode alloc] initWithDictionary:newBookmark], nil);
}
@end

namespace TestWebKitAPI {

#pragma mark - Constants

static constexpr auto *bookmarkOffManifest = @{
    @"manifest_version": @3,
    @"name": @"bookmarkpermission off Test",
    @"description": @"bookmarkpermission off Test",
    @"version": @"1",

    @"permissions": @[],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

static constexpr auto *bookmarkOnManifest = @{
    @"manifest_version": @3,
    @"name": @"bookmarkpermission on Test",
    @"description": @"bookmarkpermission on Test",
    @"version": @"1",

    @"permissions": @[ @"bookmarks" ],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
    @"action": @{
        @"default_title": @"Test Action",
        @"default_popup": @"popup.html",
    },
};

#pragma mark - Test Fixture

class WKWebExtensionAPIBookmarks : public testing::Test {
protected:
    WKWebExtensionAPIBookmarks()
    {
        bookmarkConfig = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
        if (!bookmarkConfig.webViewConfiguration)
            bookmarkConfig.webViewConfiguration = [[WKWebViewConfiguration alloc] init];

        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"WebExtensionBookmarksEnabled"])
                [bookmarkConfig.webViewConfiguration.preferences _setEnabled:YES forFeature:feature];
        }
    }

    RetainPtr<NSMutableArray> uiProcessMockBookmarks;
    NSInteger nextMockBookmarkId;

    void SetUp() override
    {
        testing::Test::SetUp();
        uiProcessMockBookmarks = adoptNS([NSMutableArray new]);
        nextMockBookmarkId = 100;
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script, NSDictionary<NSString *, id> *manifest)
    {
        return getManagerFor(@{ @"background.js" : Util::constructScript(script) }, manifest);
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSDictionary<NSString *, id> *resources, NSDictionary<NSString *, id> *manifest)
    {
        return Util::parseExtension(manifest, resources, bookmarkConfig);
    }

    void configureCreateBookmarkDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.createBookmarkWithParentIdentifier = ^(NSString *parentId, NSNumber *index, NSString *url, NSString *title, void (^completionHandler)(NSObject<_WKWebExtensionBookmark>*, NSError*)) {
            NSMutableDictionary *newBookmarkData = [NSMutableDictionary dictionary];
            newBookmarkData[@"title"] = title;
            if (url)
                newBookmarkData[@"url"] = url;
            double dateAddedInMilliseconds = NSDate.date.timeIntervalSince1970 * 1000.0;
            newBookmarkData[@"dateAdded"] = @(dateAddedInMilliseconds);

            NSString *newId = [NSString stringWithFormat:@"%ld", (long)this->nextMockBookmarkId];
            this->nextMockBookmarkId++;
            newBookmarkData[@"id"] = newId;

            if (!newBookmarkData[@"type"]) {
                NSString *url = newBookmarkData[@"url"];
                newBookmarkData[@"type"] = (url && url.length > 0) ? @"bookmark" : @"folder";
            }

            newBookmarkData[@"parentId"] = parentId;
            if (parentId && ![parentId isEqualToString:@"0"]) {
                NSMutableDictionary *parentDict = findParentInMockTree(this->uiProcessMockBookmarks.get(), parentId);
                if (parentDict) {
                    NSMutableArray *children = [parentDict[@"children"] mutableCopy] ?: [NSMutableArray array];
                    newBookmarkData[@"parentId"] = parentId;
                    newBookmarkData[@"index"] = index ?: @(children.count);
                    [children addObject:newBookmarkData];
                    parentDict[@"children"] = children;
                    completionHandler(adoptNS([[_MockBookmarkNode alloc] initWithDictionary:newBookmarkData]).get(), nil);
                    return;
                }
            }

            newBookmarkData[@"index"] = index ?: @(this->uiProcessMockBookmarks.get().count);
            [this->uiProcessMockBookmarks addObject:newBookmarkData];
            completionHandler(adoptNS([[_MockBookmarkNode alloc] initWithDictionary:newBookmarkData]).get(), nil);
        };
    }

    void configureGetBookmarksDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.bookmarksForExtensionContext = ^(void (^completionHandler)(NSArray<NSObject<_WKWebExtensionBookmark> *>*, NSError*)) {
            NSMutableArray<NSObject<_WKWebExtensionBookmark>*> *results = [NSMutableArray array];
            for (NSDictionary *dict in this->uiProcessMockBookmarks.get())
                [results addObject:[[_MockBookmarkNode alloc] initWithDictionary:dict]];
            completionHandler(results, nil);
        };
    }

    void configureRemoveBookmarksDelegate(TestWebExtensionManager *manager)
    {
        manager.internalDelegate.removeBookmarkWithIdentifier = ^(NSString *bookmarkId, BOOL removeFolderWithChildren, void (^completionHandler)(NSError *)) {
            NSMutableDictionary *foundInfo = findBookmarkAndParentArrayInMockTree(uiProcessMockBookmarks.get(), bookmarkId);

            if (!foundInfo) {
                completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Bookmark with ID '%@' not found.", bookmarkId] }]);
                return;
            }

            NSMutableDictionary *foundBookmarkDict = foundInfo[@"bookmark"];
            NSMutableArray *parentChildrenArray = foundInfo[@"parentChildren"];
            NSString *foundBookmarkType = foundBookmarkDict[@"type"];

            if (!removeFolderWithChildren) {
                if ([foundBookmarkType isEqualToString:@"folder"]) {
                    NSArray *folderChildren = foundBookmarkDict[@"children"];

                    if (folderChildren.count) {
                        completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Bookmark with ID '%@' is a non-empty folder and cannot be removed with bookmarks.remove(). Use bookmarks.removeTree().", bookmarkId] }]);
                        return;
                    }
                }
            } else {
                if ([foundBookmarkType isEqualToString:@"bookmark"]) {
                    completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSFileWriteUnknownError userInfo:@{ NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Can't remove a non-folder item '%@' with bookmarks.removeTree(). Use bookmarks.remove().", bookmarkId] }]);
                    return;
                }
            }

            if (parentChildrenArray) {
                [parentChildrenArray removeObject:foundBookmarkDict];
                completionHandler(nil);
            } else
                completionHandler([NSError errorWithDomain:NSCocoaErrorDomain code:NSExecutableRuntimeMismatchError userInfo:@{ NSLocalizedDescriptionKey: @"Failed to remove bookmark from top level (parent array not found)." }]);
        };
    }

    WKWebExtensionControllerConfiguration *bookmarkConfig;
};

#pragma mark - Common Tests

TEST_F(WKWebExtensionAPIBookmarks, APISUnavailableWhenManifestDoesNotRequest)
{
    auto *script = @[
        @"browser.test.assertDeepEq(browser.bookmarks, undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOffManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

#pragma mark - more Tests

TEST_F(WKWebExtensionAPIBookmarks, APIAvailableWhenManifestRequests)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.bookmarks === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.create === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getChildren === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getRecent === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getSubTree === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.getTree === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.get === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.move === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.remove === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.removeTree === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.search === undefined)",
        @"browser.test.assertFalse(browser.bookmarks.update === undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIDisallowsMissingArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.create())",
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren())",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent())",
        @"browser.test.assertThrows(() => browser.bookmarks.getSubTree())",
        @"browser.test.assertThrows(() => browser.bookmarks.get())",
        @"browser.test.assertThrows(() => browser.bookmarks.move())",
        @"browser.test.assertThrows(() => browser.bookmarks.remove())",
        @"browser.test.assertThrows(() => browser.bookmarks.removeTree())",
        @"browser.test.assertThrows(() => browser.bookmarks.search())",
        @"browser.test.assertThrows(() => browser.bookmarks.update())",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIDisallowedIncorrectArguments)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren(123), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren({}), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.get('test', 'test'), /The 'callback' value is invalid, because a function is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.move(123, {}), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.move('someId', 'not-an-object'), /The 'destination' value is invalid, because an object is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.remove(123), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.search(123, 'test'), /The 'callback' value is invalid, because a function is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.update(123, {}), /The 'id' value is invalid, because a string is expected./i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPICheckgetRecent)
{
    auto *script = @[
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren(123), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.log('workingtest line1')",
        @"browser.test.assertThrows(() => browser.bookmarks.getChildren({}), /The 'id' value is invalid, because a string is expected/i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent('not-a-number'), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.log('workingtest line2')",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent({}), /The 'numberOfItems' value is invalid, because a number is expected./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent(-1), /The 'numberOfItems' value is invalid, because it must be at least 1./i)",
        @"browser.test.assertThrows(() => browser.bookmarks.getRecent(0), /The 'numberOfItems' value is invalid, because it must be at least 1./i)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIGetRecent)
{
    auto *script = @[
        @"let oldBm = await browser.bookmarks.create({title: 'Oldest Bookmark', url: 'http://example.com/1'})",
        @"let midBm = await browser.bookmarks.create({title: 'Middle Bookmark', url: 'http://example.com/2'})",
        @"let newBm = await browser.bookmarks.create({title: 'Newest Bookmark', url: 'http://example.com/3'})",
        @"let recent = await browser.bookmarks.getRecent(2)",
        @"browser.test.assertEq(2, recent.length, 'Should return exactly 2 bookmarks')",
        @"browser.test.assertEq('Newest Bookmark', recent[0].title, 'First result should be the newest bookmark')",
        @"browser.test.assertEq('Middle Bookmark', recent[1].title, 'Second result should be the middle bookmark')",
        @"let newFolder = await browser.bookmarks.create({title: 'Newest Folder'})",
        @"let recent2 = await browser.bookmarks.getRecent(5)",
        @"browser.test.assertEq(3, recent2.length, 'Should adapt and return the max available which is 3')",
        @"browser.test.assertEq('Newest Bookmark', recent2[0].title, 'First result should be the newest bookmark')",
        @"browser.test.assertEq('Middle Bookmark', recent2[1].title, 'Second result should be the middle bookmark')",
        @"browser.test.assertEq('Oldest Bookmark', recent2[2].title, 'Second result should be the middle bookmark')",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPICreate)
{
    auto *script = @[
        @"let createdNode = await browser.bookmarks.create({title: 'My Test Bookmark', url: 'https://example.com/test1'})",
        @"browser.test.assertEq('My Test Bookmark', createdNode.title, 'Title should match');",
        @"browser.test.assertEq('https://example.com/test1', createdNode.url, 'URL should match');",
        @"browser.test.assertEq('bookmark', createdNode.type, 'Type should be bookmark');",
        @"let createdNode2 = await browser.bookmarks.create({title: 'My Test Folder'})",
        @"browser.test.assertEq('My Test Folder', createdNode2.title, 'Title should match');",
        @"browser.test.assertEq('folder', createdNode2.type, 'type should be folder because url is not specified');",
        @"let createdNode3 = await browser.bookmarks.create({title: 'My Children Folder', parentId: createdNode2.id})",
        @"browser.test.assertEq('My Children Folder', createdNode3.title, 'Title should match');",
        @"browser.test.assertEq('folder', createdNode3.type, 'type should be folder because url is not specified');",
        @"browser.test.assertEq(createdNode2.id, createdNode3.parentId, 'parentId should match');",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());

    [manager loadAndRun];

}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIGetTree)
{
    auto *script = @[
        @"let bookmark1 = await browser.bookmarks.create({type: 'bookmark', title: 'Top Bookmark 1', url: 'http://example.com/bm1'});",
        @"let folder1 = await browser.bookmarks.create({type: 'folder', title: 'Top Folder 1'});",
        @"let bookmark2 = await browser.bookmarks.create({title: 'Child Bookmark 2', url: 'http://example.com/bm2', parentId: folder1.id});",
        @"let bookmark3 = await browser.bookmarks.create({title: 'Top Bookmark 3', url: 'http://example.com/bm3'});",
        @"let root = await browser.bookmarks.getTree();",
        @"browser.test.assertTrue(Array.isArray(root), 'Root object should have a children array');",
        @"browser.test.assertTrue(root.length >= 1, 'Root should have at least one child (default folder)');",
        @"let foundBookmark1 = root.find(n => n.title === bookmark1.title);",
        @"browser.test.assertEq('Top Bookmark 1', foundBookmark1.title, 'Bm1 title matches');",
        @"browser.test.assertEq('http://example.com/bm1', foundBookmark1.url, 'Bm1 URL matches');",
        @"browser.test.assertEq('bookmark', foundBookmark1.type, 'Bm1 type is bookmark');",
        @"let foundFolder1 = root.find(n => n.title === folder1.title);",
        @"browser.test.assertEq('Top Folder 1', foundFolder1.title, 'Folder1 title matches');",
        @"browser.test.assertEq('folder', foundFolder1.type, 'Folder1 type is folder');",
        @"browser.test.assertEq(bookmark2.title, foundFolder1.children[0].title, 'Folder1 should have children array');",
        @"let foundBookmark2 = foundFolder1.children.find(n => n.title === bookmark2.title);",
        @"browser.test.assertEq('Child Bookmark 2', foundBookmark2.title, 'Bm2 title matches');",
        @"browser.test.assertEq('http://example.com/bm2', foundBookmark2.url, 'Bm2 URL matches');",
        @"browser.test.assertEq('bookmark', foundBookmark2.type, 'Bm2 type is bookmark');",
        @"browser.test.assertEq(folder1.id, foundBookmark2.parentId, 'Bm2 parentId matches Folder1');",
        @"browser.test.notifyPass()",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPICreateAndGetTree)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({ title: 'Test Folder' });",
        @"browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');",
        @"browser.test.assertTrue(!!folder.id, 'Folder should have an ID');",
        @"let bookmark = await browser.bookmarks.create({ parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });",
        @"browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');",
        @"let rootNode = await browser.bookmarks.getTree();",
        @"browser.test.assertTrue(Array.isArray(rootNode), 'Root object should have a children array');",
        @"browser.test.assertEq(rootNode.length, 1);",
        @"browser.test.assertEq(rootNode[0].title, 'Test Folder');",
        @"browser.test.assertTrue(Array.isArray(rootNode[0].children), 'Folder should have a children array');",
        @"browser.test.assertEq(rootNode[0].children[0].title, 'WebKit.org', 'Child bookmark in tree should have correct title');",
        @"browser.test.assertEq(rootNode[0].children[0].url, 'https://webkit.org/', 'Child bookmark in tree should have correct URL');",
        @"let bookmark2 = await browser.bookmarks.create({ id: 'topLevelBookmark', title: 'Test Top Bookmark', url: 'https://coolbook.com/' });",
        @"browser.test.assertEq(bookmark2.title, 'Test Top Bookmark', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark2.url, 'https://coolbook.com/', 'Bookmark URL should be correct');",
        @"let updatedRootNode = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(updatedRootNode.length, 2);",
        @"browser.test.assertEq(updatedRootNode[0].title, 'Test Folder');",
        @"browser.test.assertEq(updatedRootNode[1].title, 'Test Top Bookmark');",
        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIGetSubTreeChildren)
{
    auto *script = @[
        @"let folder = await browser.bookmarks.create({title: 'Test Folder' });",
        @"browser.test.assertEq(folder.title, 'Test Folder', 'Folder title should be correct');",
        @"browser.test.assertTrue(!!folder.id, 'Folder should have an ID');",
        @"let bookmark = await browser.bookmarks.create({parentId: folder.id, title: 'WebKit.org', url: 'https://webkit.org/' });",
        @"browser.test.assertEq(bookmark.title, 'WebKit.org', 'Bookmark title should be correct');",
        @"browser.test.assertEq(bookmark.url, 'https://webkit.org/', 'Bookmark URL should be correct');",
        @"let folder2 = await browser.bookmarks.create({parentId: folder.id, title: 'Folder 2'});",
        @"browser.test.assertEq(folder2.title, 'Folder 2', 'Bookmark title should be correct');",
        @"browser.test.assertEq(folder2.parentId, folder.id, 'Bookmark URL should be correct');",
        @"let bookmark2 = await browser.bookmarks.create({parentId: folder2.id, title: 'Bookmark 2', url: 'https://bookmark2.org/' });",
        @"browser.test.log(`Starting bookmark test right before...: ${JSON.stringify(folder.id)}`);",
        @"let subtreeFolder1 = await browser.bookmarks.getSubTree(folder.id);",
        @"browser.test.assertTrue(Array.isArray(subtreeFolder1), 'subtreeFolder1 should be an array');",
        @"browser.test.assertEq(2, subtreeFolder1.length, 'subtreeFolder1 array should have 2 elements');",
        @"browser.test.assertEq('Folder 2', subtreeFolder1[1].title, 'childs title should be Folder 2');",
        @"browser.test.assertEq('folder', subtreeFolder1[1].type, 'type should be folder');",
        @"browser.test.assertTrue(Array.isArray(subtreeFolder1[1].children), 'Folder 2 should have children');",
        @"browser.test.assertEq(1, subtreeFolder1[1].children.length, 'folder 2 should have 1 child');",
        @"browser.test.assertEq(bookmark2.title, subtreeFolder1[1].children[0].title, 'bookmark2 should be child of folder2');",
        @"browser.test.assertEq(bookmark.id, subtreeFolder1[0].id, 'Second child is bookmark');",
        @"let subtreeBookmark = await browser.bookmarks.getSubTree(bookmark.id);",
        @"browser.test.assertTrue(Array.isArray(subtreeBookmark), 'subtreeBookmark should be an array');",
        @"browser.test.assertEq(0, subtreeBookmark.length, 'subtreeBookmark array should have 1 element');",
        @"let childrenFolder1 = await browser.bookmarks.getChildren(folder.id);",
        @"browser.test.assertTrue(Array.isArray(childrenFolder1), 'childrenFolder1 should be an array');",
        @"browser.test.assertEq('Folder 2', childrenFolder1[1].title, 'childs title should be Folder 2');",
        @"browser.test.log(`Starting bookmark test right before...: ${JSON.stringify(folder2.id)}`);",
        @"let getFolder1 = await browser.bookmarks.get([folder.id, folder2.id]);",
        @"browser.test.assertTrue(Array.isArray(getFolder1), 'getFolder1 should be an array');",
        @"browser.test.assertEq(2, getFolder1.length, 'getFolder1 array should have 2 elements');",
        @"browser.test.assertEq('Folder 2', getFolder1[1].title, 'childs title should be Folder 2');",
        @"browser.test.assertEq('Test Folder', getFolder1[0].title, 'childs title should be Folder 2');",
        @"browser.test.notifyPass();",
    ];

    auto *resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

TEST_F(WKWebExtensionAPIBookmarks, BookmarksAPIRemoveAndRemoveTree)
{
    auto script = @[
        @"let folder1 = await browser.bookmarks.create({ title: 'Folder One' });",
        @"let bookmarkA = await browser.bookmarks.create({ parentId: folder1.id, title: 'Bookmark A', url: 'http://example.com/a' });",
        @"let folder2 = await browser.bookmarks.create({ parentId: folder1.id, title: 'Folder Two' });",
        @"let bookmarkB = await browser.bookmarks.create({ parentId: folder2.id, title: 'Bookmark B', url: 'http://example.com/b' });",
        @"let topLevelBookmark = await browser.bookmarks.create({ title: 'Top Level Bookmark', url: 'http://example.com/top' });",
        @"let tree1 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(2, tree1[0].children.length, 'folder1 should have 2 children');",
        @"browser.test.assertEq('Top Level Bookmark', tree1[1].title, 'Second top-level is topBookmark');",
        @"browser.test.assertEq('Bookmark A', tree1[0].children[0].title, 'bookmarkA is child of folder1');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.removeTree(bookmarkA.id),",
        @"/Can't remove a non-folder item '.*' with bookmarks.removeTree\\(\\). Use bookmarks.remove\\(\\)./,",
        @"'FAIL-1: removeTree on a bookmark should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.remove(folder2.id),",
        @"/Bookmark with ID '.*' is a non-empty folder and cannot be removed with bookmarks.remove\\(\\). Use bookmarks.removeTree\\(\\)./,",
        @"'FAIL-2: remove on a non-empty folder should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.remove('nonexistent-id-123'),",
        @"/Bookmark with ID 'nonexistent-id-123' not found./,",
        @"'FAIL-3a: remove on a non-existent ID should fail with the correct message');",

        @"await browser.test.assertRejects(",
        @"browser.bookmarks.removeTree('nonexistent-id-456'),",
        @"/Bookmark with ID 'nonexistent-id-456' not found./,",
        @"'FAIL-3b: removeTree on a non-existent ID should fail with the correct message');",

        @"await browser.bookmarks.remove(bookmarkA.id);",
        @"let tree2 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(1, tree2[0].children.length, 'folder1 should now have 1 child after removing bookmarkA');",
        @"browser.test.assertEq('Folder Two', tree2[0].children[0].title, 'folder2 should be the only child of folder1');",
        @"await browser.bookmarks.removeTree(folder2.id);",
        @"let tree3 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(0, tree3[0].children.length, 'folder1 should now have 0 children after removing folder2');",
        @"browser.test.assertEq(2, tree3.length, 'Tree should still have 2 top-level items (folder1 and topBookmark)');",
        @"browser.test.assertEq('Folder One', tree3[0].title, 'folder1 is still present');",
        @"browser.test.assertEq(0, tree3[0].children.length, 'folder1 is still present but has no children');",
        @"await browser.bookmarks.remove(folder1.id);",
        @"let tree4 = await browser.bookmarks.getTree();",
        @"browser.test.assertEq(1, tree4.length, 'the top level now ONLY has the top level bookmark');",
        @"browser.test.notifyPass();",
    ];

    auto resources = @{ @"background.js": Util::constructScript(script) };

    auto manager = getManagerFor(resources, bookmarkOnManifest);

    configureCreateBookmarkDelegate(manager.get());
    configureGetBookmarksDelegate(manager.get());
    configureRemoveBookmarksDelegate(manager.get());

    [manager loadAndRun];
}

}
#endif

