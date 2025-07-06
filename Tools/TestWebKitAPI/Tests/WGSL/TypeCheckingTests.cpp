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

#include "config.h"
#include "AST.h"
#include "Lexer.h"
#include "Parser.h"
#include "TestWGSLAPI.h"
#include "WGSLShaderModule.h"

#include <wtf/Assertions.h>
#include <wtf/text/MakeString.h>

namespace TestWGSLAPI {

inline void expectTypeError(const String& wgsl, const String& errorMessage)
{
    auto result = WGSL::staticCheck(wgsl, std::nullopt, { 8 });
    EXPECT_TRUE(std::holds_alternative<WGSL::FailedCheck>(result));
    auto error = std::get<WGSL::FailedCheck>(result);
    EXPECT_EQ(error.errors.size(), 1u);
    EXPECT_EQ(error.errors[0].message(), errorMessage);
}

inline String fn(const String& test)
{
    return makeString("fn testFn"_s, __COUNTER__, "() {\n"_s, test, "\n}"_s);
}

TEST(WGSLMetalGenerationTests, Array)
{
    expectTypeError("var<private> a:array;"_s, "'array' requires at least 1 template argument"_s);
    expectTypeError("@group(0) @binding(0) var<storage, read_write> b: array<u32, (1<<32)>;"_s, "value 4294967296 cannot be represented as 'i32'"_s);

    expectTypeError(fn("let x = array<i32, 0>();"_s), "array count must be greater than 0"_s);
    expectTypeError(fn("let x = array<i32, -1>();"_s), "array count must be greater than 0"_s);

    expectTypeError(fn("let x = array<i32, 2>(0);"_s), "array constructor has too few elements: expected 2, found 1"_s);
    expectTypeError(fn("let x = array<i32, 1>(0, 0);"_s), "array constructor has too many elements: expected 1, found 2"_s);

    expectTypeError(fn("let x = array<i32, 65536>();"_s), "array count (65536) must be less than 65536"_s);

    expectTypeError(fn("let x = array<i32>(0);"_s), "cannot construct a runtime-sized array"_s);

    expectTypeError(fn("let x = array<i32, 1>(0.0);"_s), "'<AbstractFloat>' cannot be used to construct an array of 'i32'"_s);

    expectTypeError(fn("let x = array();"_s), "cannot infer array element type from constructor"_s);

    expectTypeError(fn("let x = array(0, 0.0, 0u);"_s), "cannot infer common array element type from constructor arguments"_s);

    expectTypeError(fn("let x = array<i2, 1>(0.0);"_s), "unresolved type 'i2'"_s);

    expectTypeError("override elementCount = 4;"
        "fn testOverrideElementCount() {"
            "let xl = array<i32, elementCount>(0.0);"
        "}"_s, "array must have constant size in order to be constructed"_s);
}

}
