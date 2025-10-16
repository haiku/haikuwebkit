//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

const verbose = false;

let wat = `
(module
    (memory (export "memory") 1)

    (func (export "test_v128_store") (param $addr i32)
        (v128.store (local.get $addr)
            (v128.const i32x4 0x12345678 0x9ABCDEF0 0x11111111 0x22222222))
    )

    (func (export "test_v128_store_offset") (param $addr i32)
        (v128.store offset=16 (local.get $addr)
            (v128.const i64x2 0x123456789ABCDEF0 0xFEDCBA9876543210))
    )

    (func (export "test_v128_load") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load (local.get $src)))
    )

    (func (export "test_v128_load_offset") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load offset=16 (local.get $src)))
    )

    (func (export "test_v128_load8x8_s") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load8x8_s (local.get $src)))
    )

    (func (export "test_v128_load8x8_u") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load8x8_u (local.get $src)))
    )

    (func (export "test_v128_load16x4_s") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load16x4_s (local.get $src)))
    )

    (func (export "test_v128_load16x4_u") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load16x4_u (local.get $src)))
    )

    (func (export "test_v128_load32x2_s") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32x2_s (local.get $src)))
    )

    (func (export "test_v128_load32x2_u") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32x2_u (local.get $src)))
    )

    (func (export "test_v128_load8_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load8_splat (local.get $src)))
    )

    (func (export "test_v128_load16_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load16_splat (local.get $src)))
    )

    (func (export "test_v128_load32_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load32_splat (local.get $src)))
    )

    (func (export "test_v128_load64_splat") (param $src i32) (param $dst i32)
        (v128.store (local.get $dst)
            (v128.load64_splat (local.get $src)))
    )
)
`

async function test_store() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)
    const i16 = new Int16Array(buffer)

    const {
        test_v128_store,
        test_v128_store_offset,
        test_v128_load,
        test_v128_load_offset,
        test_v128_load8x8_s,
        test_v128_load8x8_u
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    function setupLoad8x8TestData(offset = 0) {
        // Set up test data: mix of values including high bytes that would be negative if signed
        u8[offset + 0] = 0x12    // signed: 18, unsigned: 18
        u8[offset + 1] = 0xFF    // signed: -1, unsigned: 255
        u8[offset + 2] = 0x7F    // signed: 127, unsigned: 127
        u8[offset + 3] = 0x80    // signed: -128, unsigned: 128
        u8[offset + 4] = 0x00    // signed: 0, unsigned: 0
        u8[offset + 5] = 0xCE    // signed: -50, unsigned: 206
        u8[offset + 6] = 0x64    // signed: 100, unsigned: 100
        u8[offset + 7] = 0x9C    // signed: -100, unsigned: 156
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test basic v128.store
        clearMemory()
        test_v128_store(0)
        assert.eq(u32[0], 0x12345678)
        assert.eq(u32[1], 0x9ABCDEF0)
        assert.eq(u32[2], 0x11111111)
        assert.eq(u32[3], 0x22222222)

        // Test v128.store with offset
        clearMemory()
        test_v128_store_offset(0)
        assert.eq(u64[2], 0x123456789ABCDEF0n)  // offset 16 = index 2 in u64 array
        assert.eq(u64[3], 0xFEDCBA9876543210n)

        // Test store at different address
        clearMemory()
        test_v128_store(64)
        assert.eq(u32[16], 0x12345678)  // 64/4 = 16
        assert.eq(u32[17], 0x9ABCDEF0)
        assert.eq(u32[18], 0x11111111)
        assert.eq(u32[19], 0x22222222)

        // Verify other memory locations are still zero
        assert.eq(u32[0], 0)
        assert.eq(u32[15], 0)
        assert.eq(u32[20], 0)

        // Test v128.load - store data, then load and store to different location
        clearMemory()
        test_v128_store(0)  // Store test pattern at address 0
        test_v128_load(0, 128)  // Load from address 0, store to address 128

        // Verify the loaded data matches the original
        assert.eq(u32[32], 0x12345678)  // 128/4 = 32
        assert.eq(u32[33], 0x9ABCDEF0)
        assert.eq(u32[34], 0x11111111)
        assert.eq(u32[35], 0x22222222)

        // Test v128.load with offset
        clearMemory()
        test_v128_store_offset(0)  // Store test pattern at offset 16 from address 0
        test_v128_load_offset(0, 192)  // Load from offset 16, store to address 192

        // Verify the loaded data matches the original
        assert.eq(u64[24], 0x123456789ABCDEF0n)  // 192/8 = 24
        assert.eq(u64[25], 0xFEDCBA9876543210n)

        // Test v128.load8x8_s - sign extension from i8 to i16
        clearMemory()
        setupLoad8x8TestData(0)

        test_v128_load8x8_s(0, 256)  // Load 8 bytes from address 0, store result to address 256

        // Verify sign extension
        assert.eq(i16[128], 18)
        assert.eq(i16[129], -1)
        assert.eq(i16[130], 127)
        assert.eq(i16[131], -128)
        assert.eq(i16[132], 0)
        assert.eq(i16[133], -50)
        assert.eq(i16[134], 100)
        assert.eq(i16[135], -100)

        // Test v128.load8x8_u - zero extension from u8 to u16
        clearMemory()
        setupLoad8x8TestData(8)

        test_v128_load8x8_u(8, 320)  // Load 8 bytes from address 8, store result to address 320

        // Verify zero extension
        assert.eq(u16[160], 0x0012)
        assert.eq(u16[161], 0x00FF)
        assert.eq(u16[162], 0x007F)
        assert.eq(u16[163], 0x0080)
        assert.eq(u16[164], 0x0000)
        assert.eq(u16[165], 0x00CE)
        assert.eq(u16[166], 0x0064)
        assert.eq(u16[167], 0x009C)
    }
    if (verbose)
        print("Store tests passed!")
}

async function test_load_extend() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)

    const {
        test_v128_load16x4_s,
        test_v128_load16x4_u,
        test_v128_load32x2_s,
        test_v128_load32x2_u
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    function setupLoad16x4TestData(offset = 0) {
        // Set up test data: mix of 16-bit values including high values that would be negative if signed
        const baseIndex = offset / 2
        u16[baseIndex + 0] = 0x1234    // signed: 4660, unsigned: 4660
        u16[baseIndex + 1] = 0xFFFF    // signed: -1, unsigned: 65535
        u16[baseIndex + 2] = 0x7FFF    // signed: 32767, unsigned: 32767
        u16[baseIndex + 3] = 0x8000    // signed: -32768, unsigned: 32768
    }

    function setupLoad32x2TestData(offset = 0) {
        // Set up test data: mix of 32-bit values including high values that would be negative if signed
        const baseIndex = offset / 4
        u32[baseIndex + 0] = 0x12345678    // signed: 305419896, unsigned: 305419896
        u32[baseIndex + 1] = 0xFFFFFFFF    // signed: -1, unsigned: 4294967295
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test v128.load16x4_s - sign extension from i16 to i32
        clearMemory()
        setupLoad16x4TestData(0)
        test_v128_load16x4_s(0, 384)

        const i32 = new Int32Array(buffer)
        assert.eq(i32[96], 4660)
        assert.eq(i32[97], -1)
        assert.eq(i32[98], 32767)
        assert.eq(i32[99], -32768)

        // Test v128.load16x4_u - zero extension from u16 to u32
        clearMemory()
        setupLoad16x4TestData(8)
        test_v128_load16x4_u(8, 448)

        assert.eq(u32[112], 0x00001234)
        assert.eq(u32[113], 0x0000FFFF)
        assert.eq(u32[114], 0x00007FFF)
        assert.eq(u32[115], 0x00008000)

        // Test v128.load32x2_s - sign extension from i32 to i64
        clearMemory()
        setupLoad32x2TestData(0)
        test_v128_load32x2_s(0, 512)

        const i64 = new BigInt64Array(buffer)
        assert.eq(i64[64], 0x12345678n)
        assert.eq(i64[65], -1n)

        // Test v128.load32x2_u - zero extension from u32 to u64
        clearMemory()
        setupLoad32x2TestData(8)
        test_v128_load32x2_u(8, 576)

        assert.eq(u64[72], 0x12345678n)
        assert.eq(u64[73], 0xFFFFFFFFn)
    }
    if (verbose)
        print("Load extend tests passed!")
}

async function test_load_splat() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const u8 = new Uint8Array(buffer)
    const u16 = new Uint16Array(buffer)
    const u32 = new Uint32Array(buffer)
    const u64 = new BigUint64Array(buffer)

    const {
        test_v128_load8_splat,
        test_v128_load16_splat,
        test_v128_load32_splat,
        test_v128_load64_splat
    } = instance.exports

    function clearMemory() {
        u8.fill(0)
    }

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        // Test v128.load8_splat - load 1 byte and splat to all 16 lanes
        clearMemory()
        u8[0] = 0xAB
        test_v128_load8_splat(0, 640)

        // Verify all 16 bytes are 0xAB
        for (let j = 0; j < 16; j++)
            assert.eq(u8[640 + j], 0xAB)

        // Test v128.load16_splat - load 1 i16 and splat to all 8 lanes
        clearMemory()
        u16[0] = 0x1234
        test_v128_load16_splat(0, 704)

        // Verify all 8 i16 values are 0x1234
        for (let j = 0; j < 8; j++)
            assert.eq(u16[352 + j], 0x1234)

        // Test v128.load32_splat - load 1 i32 and splat to all 4 lanes
        clearMemory()
        u32[0] = 0x12345678
        test_v128_load32_splat(0, 768)

        // Verify all 4 i32 values are 0x12345678
        for (let j = 0; j < 4; j++)
            assert.eq(u32[192 + j], 0x12345678)

        // Test v128.load64_splat - load 1 i64 and splat to all 2 lanes
        clearMemory()
        u64[0] = 0x123456789ABCDEF0n
        test_v128_load64_splat(0, 832)

        // Verify all 2 i64 values are 0x123456789ABCDEF0n
        for (let j = 0; j < 2; j++)
            assert.eq(u64[104 + j], 0x123456789ABCDEF0n)
    }
    if (verbose)
        print("Load splat tests passed!")
}

async function test_bounds_checking() {
    const instance = await instantiate(wat, {}, { simd: true })
    const memory = instance.exports.memory
    const buffer = memory.buffer
    const memorySize = 65536  // 1 page = 64KB

    const {
        test_v128_load16x4_s,
        test_v128_load16x4_u,
        test_v128_load32x2_s,
        test_v128_load32x2_u,
        test_v128_load8_splat,
        test_v128_load16_splat,
        test_v128_load32_splat,
        test_v128_load64_splat
    } = instance.exports

    for (let i = 0; i < wasmTestLoopCount; ++i) {
        try {
            test_v128_load16x4_s(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load16x4_s should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load16x4_u(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load16x4_u should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load32x2_s(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load32x2_s should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load32x2_u(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load32x2_u should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load8_splat(memorySize, 0)  // Should fail: trying to read at exactly memorySize
            throw new Error("v128.load8_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load16_splat(memorySize - 1, 0)  // Should fail: needs 2 bytes but only 1 available
            throw new Error("v128.load16_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load32_splat(memorySize - 3, 0)  // Should fail: needs 4 bytes but only 3 available
            throw new Error("v128.load32_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        try {
            test_v128_load64_splat(memorySize - 7, 0)  // Should fail: needs 8 bytes but only 7 available
            throw new Error("v128.load64_splat should have thrown on out-of-bounds access")
        } catch (e) {
            if (e.message.includes("should have thrown")) throw e
        }

        // Test valid boundary cases (should succeed)
        test_v128_load16x4_s(memorySize - 8, 0)   // Exactly at boundary: reads 8 bytes
        test_v128_load16x4_u(memorySize - 8, 16)  // Exactly at boundary: reads 8 bytes
        test_v128_load32x2_s(memorySize - 8, 32)  // Exactly at boundary: reads 8 bytes
        test_v128_load32x2_u(memorySize - 8, 48)  // Exactly at boundary: reads 8 bytes
        test_v128_load8_splat(memorySize - 1, 64)  // Exactly at boundary: reads 1 byte
        test_v128_load16_splat(memorySize - 2, 80) // Exactly at boundary: reads 2 bytes
        test_v128_load32_splat(memorySize - 4, 96) // Exactly at boundary: reads 4 bytes
        test_v128_load64_splat(memorySize - 8, 112) // Exactly at boundary: reads 8 bytes
    }
    if (verbose)
        print("Bounds checking tests passed!")
}

await assert.asyncTest(test_store())
await assert.asyncTest(test_load_extend())
await assert.asyncTest(test_load_splat())
await assert.asyncTest(test_bounds_checking())
