/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
#import "DDMesh.h"

#import "APIConversions.h"
#import "Instance.h"
#import "TextureView.h"

#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>

#if ENABLE(WEBGPU_SWIFT)
#import "WebGPUSwiftInternal.h"
#endif

namespace WebGPU {

#if ENABLE(WEBGPU_SWIFT)
static NSArray<DDBridgeVertexAttributeFormat*>* convertAttributes(const Vector<WGPUDDVertexAttributeFormat>& vertexAttributes)
{
    NSMutableArray<DDBridgeVertexAttributeFormat*>* result = [NSMutableArray array];
    for (auto& a : vertexAttributes)
        [result addObject:[[DDBridgeVertexAttributeFormat alloc] initWithSemantic:a.semantic format:a.format layoutIndex:a.layoutIndex offset:a.offset]];

    return result;
}
static NSArray<DDBridgeVertexLayout*>* convertLayouts(const Vector<WGPUDDVertexLayout>& layouts)
{
    NSMutableArray<DDBridgeVertexLayout*>* result = [NSMutableArray array];
    for (auto& a : layouts)
        [result addObject:[[DDBridgeVertexLayout alloc] initWithBufferIndex:a.bufferIndex bufferOffset:a.bufferOffset bufferStride:a.bufferStride]];

    return result;
}

static DDBridgeAddMeshRequest* convertDescriptor(const WGPUDDMeshDescriptor& descriptor)
{
    DDBridgeAddMeshRequest* result = [[DDBridgeAddMeshRequest alloc] initWithIndexCapacity:descriptor.indexCapacity
        indexType:descriptor.indexType
        vertexBufferCount:descriptor.vertexBufferCount
        vertexCapacity:descriptor.vertexCapacity
        vertexAttributes:convertAttributes(descriptor.vertexAttributes)
        vertexLayouts:convertLayouts(descriptor.vertexLayouts)
    ];

    return result;
}
#endif

Ref<DDMesh> Instance::createMesh(const WGPUDDMeshDescriptor& descriptor)
{
#if ENABLE(WEBGPU_SWIFT)
    if (![m_ddReceiver addMesh:convertDescriptor(descriptor) identifier:m_ddMeshIdentifier])
        return DDMesh::createInvalid(*this);
    return DDMesh::create(descriptor, *this);
#else
    UNUSED_PARAM(descriptor);
    return DDMesh::createInvalid(*this);
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(DDMesh);

DDMesh::DDMesh(const WGPUDDMeshDescriptor& descriptor, Instance& instance)
    : m_instance(instance)
    , m_descriptor(descriptor)
{
}

DDMesh::DDMesh(Instance& instance)
    : m_instance(instance)
{
}

bool DDMesh::isValid() const
{
    return true;
}

DDMesh::~DDMesh() = default;


void DDMesh::update(WGPUDDUpdateMeshDescriptor* desc)
{
    if (desc)
        m_instance->updateMesh(*this, *desc);
}

}

#pragma mark WGPU Stubs

void wgpuDDMeshReference(WGPUDDMesh mesh)
{
    WebGPU::fromAPI(mesh).ref();
}

void wgpuDDMeshRelease(WGPUDDMesh mesh)
{
    WebGPU::fromAPI(mesh).deref();
}

WGPU_EXPORT void wgpuDDMeshUpdate(WGPUDDMesh mesh, WGPUDDUpdateMeshDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->update(desc);
}
