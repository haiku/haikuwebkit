/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#import "config.h"

#import "AST.h"
#import "Lexer.h"
#import "Parser.h"
#import "TestWGSLAPI.h"
#import "WGSLShaderModule.h"
#import <JavaScriptCore/InitializeThreading.h>
#import <JavaScriptCore/RegularExpression.h>
#import <Metal/Metal.h>
#import <wtf/Assertions.h>

namespace TestWGSLAPI {

using Check = Function<unsigned(const String&, unsigned offset)>;

static Check checkLiteral(const String& pattern)
{
    return [&](const String& msl, unsigned offset) -> unsigned {
        auto result = msl.find(pattern, offset);
        EXPECT_TRUE(result != notFound);
        return result;
    };
}

static Check checkNot(const String& pattern)
{
    return [&](const String& msl, unsigned offset) -> unsigned {
        JSC::Yarr::RegularExpression test(pattern);
        auto result = test.match(msl, offset);
        EXPECT_EQ(result, -1);
        return offset;
    };
}

template<typename... Checks>
static void testCompilation(const String& wgsl, Checks&&... checks)
{
    auto result = WGSL::staticCheck(wgsl, std::nullopt, { 8 });
    EXPECT_TRUE(std::holds_alternative<WGSL::SuccessfulCheck>(result));

    auto& shaderModule = std::get<WGSL::SuccessfulCheck>(result).ast;
    HashMap<String, WGSL::PipelineLayout*> pipelineLayouts;
    for (auto& entryPoint : shaderModule->callGraph().entrypoints())
        pipelineLayouts.add(entryPoint.originalName, nullptr);
    auto prepareResult = WGSL::prepare(shaderModule, pipelineLayouts);
    EXPECT_TRUE(std::holds_alternative<WGSL::PrepareResult>(prepareResult));

    HashMap<String, WGSL::ConstantValue> constantValues;
    auto generationResult = WGSL::generate(shaderModule, std::get<WGSL::PrepareResult>(prepareResult), constantValues, { });
    EXPECT_TRUE(std::holds_alternative<String>(generationResult));

    auto msl = std::get<String>(generationResult);
    auto device = MTLCreateSystemDefaultDevice();
    auto options = [MTLCompileOptions new];
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:msl.createNSString().get() options:options error:&error];
    EXPECT_TRUE(error == nil);
    EXPECT_TRUE(library != nil);

    unsigned offset = 0;
    for (const auto& check : std::initializer_list<Check> { std::forward<Checks>(checks)... })
        offset = check(msl, offset);
}

TEST(WGSLMetalCompilationTests, ReturnTypePromotion)
{
    testCompilation(
        "fn testScalarPromotion() -> f32"
        "{"
        "    return 0;"
        "}"

        "fn testVectorPromotion() -> vec3f"
        "{"
        "    return vec3(0);"
        "}"

        "@compute"
        "@workgroup_size(1, 1, 1)"
        "fn main() {"
        "    _ = testScalarPromotion();"
        "    _ = testVectorPromotion();"
        "}"_s);
}

TEST(WGSLMetalCompilationTests, ConstantFunctionNotInOutput)
{
    JSC::initialize();
    testCompilation(
        "@compute @workgroup_size(1)"
        "fn main()"
        "{"
        "  const a = pow(2, 2);"
        "  _ = a;"
        "}"_s,
        checkNot("pow(.*)"_s),
        checkNot("float .* = \\d"_s),
        checkLiteral("(void)(4.)"_s));

}

}
