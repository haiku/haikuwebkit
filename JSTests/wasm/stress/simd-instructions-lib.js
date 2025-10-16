//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

/**
 * Convert array to v128.const string based on instruction type
 * @param {Array} array - Input array
 * @param {string} instruction - SIMD instruction name
 * @returns {string} - v128.const string
 */
function arrayToV128Const(array, instruction) {
    if (instruction.startsWith('i8x16.')) {
        const hexValues = array.map(val => `0x${val.toString(16).padStart(2, '0').toUpperCase()}`);
        return `(v128.const i8x16 ${hexValues.join(' ')})`;
    } else if (instruction.startsWith('i16x8.')) {
        const hexValues = array.map(val => `0x${val.toString(16).padStart(4, '0').toUpperCase()}`);
        return `(v128.const i16x8 ${hexValues.join(' ')})`;
    } else if (instruction.startsWith('i32x4.')) {
        const hexValues = array.map(val => `0x${val.toString(16).padStart(8, '0').toUpperCase()}`);
        return `(v128.const i32x4 ${hexValues.join(' ')})`;
    } else if (instruction.startsWith('i64x2.')) {
        const hexValues = array.map(val => {
            const bigIntVal = typeof val === 'bigint' ? val : BigInt(val);
            return `0x${bigIntVal.toString(16).padStart(16, '0').toUpperCase()}`;
        });
        return `(v128.const i64x2 ${hexValues.join(' ')})`;
    } else if (instruction.startsWith('f32x4.')) {
        return `(v128.const f32x4 ${array.join(' ')})`;
    } else if (instruction.startsWith('f64x2.')) {
        return `(v128.const f64x2 ${array.join(' ')})`;
    }
    // Default fallback - assume it's already a string
    return array;
}

/**
 * Run SIMD instruction tests with given test data
 * @param {Array} testData - Array of test cases, each containing [instruction, input0, input1, expected]
 * @param {boolean} verbose - Whether to print verbose output
 * @param {string} testType - Description of test type for logging
 */
export async function runSIMDTests(testData, verbose = false, testType = "SIMD") {
    // Generate WebAssembly module
    let wat = `
(module
    (memory (export "memory") 1)
`;

    testData.forEach((test, index) => {
        const [instruction, input0, input1, expected] = test;
        const input0Str = Array.isArray(input0) ? arrayToV128Const(input0, instruction) : input0;
        const input1Str = Array.isArray(input1) ? arrayToV128Const(input1, instruction) : input1;

        wat += `
    (func (export "test_${index}") (param $addr i32)
        (v128.store (local.get $addr)
            (${instruction} ${input0Str} ${input1Str}))
    )
`;
    });

    wat += `
)
`;

    const instance = await instantiate(wat, {}, { simd: true });
    const memory = instance.exports.memory;
    const buffer = memory.buffer;
    const u8 = new Uint8Array(buffer);
    const u16 = new Uint16Array(buffer);
    const u32 = new Uint32Array(buffer);
    const u64 = new BigUint64Array(buffer);

    function clearMemory() {
        u8.fill(0);
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        testData.forEach((test, testIndex) => {
            const [instruction, input0, input1, expected] = test;
            
            if (verbose)
                print(`Testing ${instruction} test ${testIndex}...`);
            
            clearMemory();
            
            // Call the test function
            const testFunc = instance.exports[`test_${testIndex}`];
            testFunc(0);
            
            // Verify the result using appropriate data type
            if (instruction.startsWith('i8x16.')) {
                for (let j = 0; j < 16; j++)
                    assert.eq(u8[j], expected[j]);
            } else if (instruction.startsWith('i16x8.')) {
                for (let j = 0; j < 8; j++)
                    assert.eq(u16[j], expected[j]);
            } else if (instruction.startsWith('i32x4.') || instruction.startsWith('f32x4.')) {
                for (let j = 0; j < 4; j++)
                    assert.eq(u32[j], expected[j]);
            } else if (instruction.startsWith('i64x2.') || instruction.startsWith('f64x2.')) {
                for (let j = 0; j < 2; j++)
                    assert.eq(u64[j], expected[j]);
            }
            
            if (verbose)
                print(`✓ ${instruction} test ${testIndex} passed`);
        });
    }
    
    if (verbose)
        print(`All ${testData.length} ${testType} tests passed!`);
}