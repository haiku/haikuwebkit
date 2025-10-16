/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(FTL_JIT)

#include "B3Value.h"
#include "DFGArrayMode.h"
#include "FTLAbstractHeap.h"
#include "HasOwnPropertyCache.h"
#include "IndexingType.h"
#include "JSGlobalObject.h"
#include "JSGlobalProxy.h"
#include "JSMap.h"
#include "JSSet.h"
#include "JSWeakMap.h"
#include "NumericStrings.h"
#include "Symbol.h"

namespace JSC { namespace FTL {

#define FOR_EACH_ABSTRACT_HEAP(macro) \
    macro(typedArrayProperties) \
    macro(JSCellHeaderAndNamedProperties) \
    macro(OrderedHashTableData) \

// macro(name, offset, mutability)
#define FOR_EACH_ABSTRACT_FIELD(macro) \
    macro(ArrayBuffer_data, ArrayBuffer::offsetOfData(), B3::Mutability::Mutable) \
    macro(ArrayStorage_numValuesInVector, ArrayStorage::numValuesInVectorOffset(), B3::Mutability::Mutable) \
    macro(Butterfly_arrayBuffer, Butterfly::offsetOfArrayBuffer(), B3::Mutability::Mutable) \
    macro(Butterfly_publicLength, Butterfly::offsetOfPublicLength(), B3::Mutability::Mutable) \
    macro(Butterfly_vectorLength, Butterfly::offsetOfVectorLength(), B3::Mutability::Mutable) \
    macro(CallFrame_callerFrame, CallFrame::callerFrameOffset(), B3::Mutability::Mutable) \
    macro(ClassInfo_parentClass, ClassInfo::offsetOfParentClass(), B3::Mutability::Immutable) \
    macro(ClonedArguments_callee, ClonedArguments::offsetOfCallee(), B3::Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache0_key, ConcatKeyAtomStringCache::offsetOfQuickCache0() + ConcatKeyAtomStringCache::CacheEntry::offsetOfKey(), B3::Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache0_value, ConcatKeyAtomStringCache::offsetOfQuickCache0() + ConcatKeyAtomStringCache::CacheEntry::offsetOfValue(), B3::Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache1_key, ConcatKeyAtomStringCache::offsetOfQuickCache1() + ConcatKeyAtomStringCache::CacheEntry::offsetOfKey(), B3::Mutability::Mutable) \
    macro(ConcatKeyAtomStringCache_quickCache1_value, ConcatKeyAtomStringCache::offsetOfQuickCache1() + ConcatKeyAtomStringCache::CacheEntry::offsetOfValue(), B3::Mutability::Mutable) \
    macro(DateInstance_internalNumber, DateInstance::offsetOfInternalNumber(), B3::Mutability::Mutable) \
    macro(DateInstance_data, DateInstance::offsetOfData(), B3::Mutability::Mutable) \
    macro(DateInstanceData_gregorianDateTimeCachedForMS, DateInstanceData::offsetOfGregorianDateTimeCachedForMS(), B3::Mutability::Mutable) \
    macro(DateInstanceData_gregorianDateTimeUTCCachedForMS, DateInstanceData::offsetOfGregorianDateTimeUTCCachedForMS(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_year, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfYear(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_year, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfYear(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_month, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMonth(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_month, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMonth(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_monthDay, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMonthDay(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_monthDay, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMonthDay(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_weekDay, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfWeekDay(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_weekDay, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfWeekDay(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_hour, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfHour(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_hour, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfHour(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_minute, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfMinute(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_minute, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfMinute(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_second, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfSecond(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_second, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfSecond(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTime_utcOffsetInMinute, DateInstanceData::offsetOfCachedGregorianDateTime() + GregorianDateTime::offsetOfUTCOffsetInMinute(), B3::Mutability::Mutable) \
    macro(DateInstanceData_cachedGregorianDateTimeUTC_utcOffsetInMinute, DateInstanceData::offsetOfCachedGregorianDateTimeUTC() + GregorianDateTime::offsetOfUTCOffsetInMinute(), B3::Mutability::Mutable) \
    macro(DirectArguments_callee, DirectArguments::offsetOfCallee(), B3::Mutability::Mutable) \
    macro(DirectArguments_length, DirectArguments::offsetOfLength(), B3::Mutability::Mutable) \
    macro(DirectArguments_minCapacity, DirectArguments::offsetOfMinCapacity(), B3::Mutability::Mutable) \
    macro(DirectArguments_mappedArguments, DirectArguments::offsetOfMappedArguments(), B3::Mutability::Mutable) \
    macro(DirectArguments_modifiedArgumentsDescriptor, DirectArguments::offsetOfModifiedArgumentsDescriptor(), B3::Mutability::Mutable) \
    macro(FunctionExecutable_rareData, FunctionExecutable::offsetOfRareData(), B3::Mutability::Mutable) \
    macro(FunctionExecutableRareData_asString, FunctionExecutable::RareData::offsetOfAsString(), B3::Mutability::Mutable) \
    macro(FunctionRareData_allocator, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfAllocator(), B3::Mutability::Mutable) \
    macro(FunctionRareData_structure, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfStructure(), B3::Mutability::Mutable) \
    macro(FunctionRareData_prototype, FunctionRareData::offsetOfObjectAllocationProfile() + ObjectAllocationProfileWithPrototype::offsetOfPrototype(), B3::Mutability::Mutable) \
    macro(FunctionRareData_allocationProfileWatchpointSet, FunctionRareData::offsetOfAllocationProfileWatchpointSet(), B3::Mutability::Mutable) \
    macro(FunctionRareData_executable, FunctionRareData::offsetOfExecutable(), B3::Mutability::Mutable) \
    macro(FunctionRareData_internalFunctionAllocationProfile_structureID, FunctionRareData::offsetOfInternalFunctionAllocationProfile() + InternalFunctionAllocationProfile::offsetOfStructureID(), B3::Mutability::Mutable) \
    macro(GetterSetter_getter, GetterSetter::offsetOfGetter(), B3::Mutability::Mutable) \
    macro(GetterSetter_setter, GetterSetter::offsetOfSetter(), B3::Mutability::Mutable) \
    macro(JSArrayBufferView_byteOffset, JSArrayBufferView::offsetOfByteOffset(), B3::Mutability::Mutable) \
    macro(JSArrayBufferView_length, JSArrayBufferView::offsetOfLength(), B3::Mutability::Mutable) \
    macro(JSArrayBufferView_mode, JSArrayBufferView::offsetOfMode(), B3::Mutability::Mutable) \
    macro(JSArrayBufferView_vector, JSArrayBufferView::offsetOfVector(), B3::Mutability::Mutable) \
    macro(JSBigInt_length, JSBigInt::offsetOfLength(), B3::Mutability::Immutable) \
    macro(JSBoundFunction_targetFunction, JSBoundFunction::offsetOfTargetFunction(), B3::Mutability::Mutable) \
    macro(JSBoundFunction_boundThis, JSBoundFunction::offsetOfBoundThis(), B3::Mutability::Mutable) \
    macro(JSBoundFunction_boundArg0, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 0, B3::Mutability::Mutable) \
    macro(JSBoundFunction_boundArg1, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 1, B3::Mutability::Mutable) \
    macro(JSBoundFunction_boundArg2, JSBoundFunction::offsetOfBoundArgs() + sizeof(WriteBarrier<Unknown>) * 2, B3::Mutability::Mutable) \
    macro(JSBoundFunction_nameMayBeNull, JSBoundFunction::offsetOfNameMayBeNull(), B3::Mutability::Mutable) \
    macro(JSBoundFunction_length, JSBoundFunction::offsetOfLength(), B3::Mutability::Mutable) \
    macro(JSBoundFunction_boundArgsLength, JSBoundFunction::offsetOfBoundArgsLength(), B3::Mutability::Mutable) \
    macro(JSBoundFunction_canConstruct, JSBoundFunction::offsetOfCanConstruct(), B3::Mutability::Mutable) \
    macro(JSCallee_scope, JSCallee::offsetOfScopeChain(), B3::Mutability::Mutable) \
    macro(JSCell_cellState, JSCell::cellStateOffset(), B3::Mutability::Mutable) \
    macro(JSCell_header, 0, B3::Mutability::Mutable) \
    macro(JSCell_indexingTypeAndMisc, JSCell::indexingTypeAndMiscOffset(), B3::Mutability::Mutable) \
    macro(JSCell_structureID, JSCell::structureIDOffset(), B3::Mutability::Mutable) \
    macro(JSCell_typeInfoFlags, JSCell::typeInfoFlagsOffset(), B3::Mutability::Mutable) \
    macro(JSCell_typeInfoType, JSCell::typeInfoTypeOffset(), B3::Mutability::Immutable) \
    macro(JSCell_usefulBytes, JSCell::indexingTypeAndMiscOffset(), B3::Mutability::Mutable) \
    macro(JSFunction_executableOrRareData, JSFunction::offsetOfExecutableOrRareData(), B3::Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_lastRegExp, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfLastRegExp(), B3::Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_lastInput, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfLastInput(), B3::Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_result_start, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, start), B3::Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_result_end, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfResult() + OBJECT_OFFSETOF(MatchResult, end), B3::Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_reified, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfReified(), B3::Mutability::Mutable) \
    macro(JSGlobalObject_regExpGlobalData_cachedResult_oneCharacterMatch, JSGlobalObject::regExpGlobalDataOffset() + RegExpGlobalData::offsetOfCachedResult() + RegExpCachedResult::offsetOfOneCharacterMatch(), B3::Mutability::Mutable) \
    macro(JSGlobalProxy_target, JSGlobalProxy::targetOffset(), B3::Mutability::Mutable) \
    macro(JSObject_butterfly, JSObject::butterflyOffset(), B3::Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_cachedInlineCapacity, JSPropertyNameEnumerator::cachedInlineCapacityOffset(), B3::Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_cachedPropertyNamesVector, JSPropertyNameEnumerator::cachedPropertyNamesVectorOffset(), B3::Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_cachedStructureID, JSPropertyNameEnumerator::cachedStructureIDOffset(), B3::Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_endGenericPropertyIndex, JSPropertyNameEnumerator::endGenericPropertyIndexOffset(), B3::Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_endStructurePropertyIndex, JSPropertyNameEnumerator::endStructurePropertyIndexOffset(), B3::Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_indexLength, JSPropertyNameEnumerator::indexedLengthOffset(), B3::Mutability::Mutable) \
    macro(JSPropertyNameEnumerator_flags, JSPropertyNameEnumerator::flagsOffset(), B3::Mutability::Mutable) \
    macro(JSRopeString_flags, JSRopeString::offsetOfFlags(), B3::Mutability::Mutable) \
    macro(JSRopeString_length, JSRopeString::offsetOfLength(), B3::Mutability::Immutable) \
    macro(JSRopeString_fiber0, JSRopeString::offsetOfFiber0(), B3::Mutability::Mutable) \
    macro(JSRopeString_fiber1, JSRopeString::offsetOfFiber1(), B3::Mutability::Mutable) \
    macro(JSRopeString_fiber2, JSRopeString::offsetOfFiber2(), B3::Mutability::Mutable) \
    macro(JSScope_next, JSScope::offsetOfNext(), B3::Mutability::Immutable) \
    macro(JSSymbolTableObject_symbolTable, JSSymbolTableObject::offsetOfSymbolTable(), B3::Mutability::Mutable) \
    macro(JSWebAssemblyInstance_moduleRecord, JSWebAssemblyInstance::offsetOfModuleRecord(), B3::Mutability::Mutable) \
    macro(NativeExecutable_asString, NativeExecutable::offsetOfAsString(), B3::Mutability::Mutable) \
    macro(RegExpObject_regExpAndFlags, RegExpObject::offsetOfRegExpAndFlags(), B3::Mutability::Mutable) \
    macro(RegExpObject_lastIndex, RegExpObject::offsetOfLastIndex(), B3::Mutability::Mutable) \
    macro(ShadowChicken_Packet_callee, OBJECT_OFFSETOF(ShadowChicken::Packet, callee), B3::Mutability::Mutable) \
    macro(ShadowChicken_Packet_frame, OBJECT_OFFSETOF(ShadowChicken::Packet, frame), B3::Mutability::Mutable) \
    macro(ShadowChicken_Packet_callerFrame, OBJECT_OFFSETOF(ShadowChicken::Packet, callerFrame), B3::Mutability::Mutable) \
    macro(ShadowChicken_Packet_thisValue, OBJECT_OFFSETOF(ShadowChicken::Packet, thisValue), B3::Mutability::Mutable) \
    macro(ShadowChicken_Packet_scope, OBJECT_OFFSETOF(ShadowChicken::Packet, scope), B3::Mutability::Mutable) \
    macro(ShadowChicken_Packet_codeBlock, OBJECT_OFFSETOF(ShadowChicken::Packet, codeBlock), B3::Mutability::Mutable) \
    macro(ShadowChicken_Packet_callSiteIndex, OBJECT_OFFSETOF(ShadowChicken::Packet, callSiteIndex), B3::Mutability::Mutable) \
    macro(ScopedArguments_overrodeThings, ScopedArguments::offsetOfOverrodeThings(), B3::Mutability::Mutable) \
    macro(ScopedArguments_scope, ScopedArguments::offsetOfScope(), B3::Mutability::Mutable) \
    macro(ScopedArguments_storage, ScopedArguments::offsetOfStorage(), B3::Mutability::Mutable) \
    macro(ScopedArguments_table, ScopedArguments::offsetOfTable(), B3::Mutability::Mutable) \
    macro(ScopedArguments_totalLength, ScopedArguments::offsetOfTotalLength(), B3::Mutability::Mutable) \
    macro(ScopedArgumentsTable_arguments, ScopedArgumentsTable::offsetOfArguments(), B3::Mutability::Mutable) \
    macro(ScopedArgumentsTable_length, ScopedArgumentsTable::offsetOfLength(), B3::Mutability::Mutable) \
    macro(StringImpl_data, StringImpl::dataOffset(), B3::Mutability::Immutable) \
    macro(StringImpl_hashAndFlags, StringImpl::flagsOffset(), B3::Mutability::Mutable) \
    macro(StringImpl_length, StringImpl::lengthMemoryOffset(), B3::Mutability::Immutable) \
    macro(Structure_bitField, Structure::bitFieldOffset(), B3::Mutability::Mutable) \
    macro(Structure_classInfo, Structure::classInfoOffset(), B3::Mutability::Immutable) \
    macro(Structure_globalObject, Structure::globalObjectOffset(), B3::Mutability::Immutable) \
    macro(Structure_indexingModeIncludingHistory, Structure::indexingModeIncludingHistoryOffset(), B3::Mutability::Immutable) \
    macro(Structure_inlineCapacity, Structure::inlineCapacityOffset(), B3::Mutability::Immutable) \
    macro(Structure_outOfLineTypeFlags, Structure::outOfLineTypeFlagsOffset(), B3::Mutability::Immutable) \
    macro(Structure_previousOrRareData, Structure::previousOrRareDataOffset(), B3::Mutability::Mutable) \
    macro(Structure_propertyHash, Structure::propertyHashOffset(), B3::Mutability::Mutable) \
    macro(Structure_prototype, Structure::prototypeOffset(), B3::Mutability::Immutable) \
    macro(Structure_seenProperties, Structure::seenPropertiesOffset(), B3::Mutability::Mutable) \
    macro(StructureRareData_cachedEnumerableStrings, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::EnumerableStrings), B3::Mutability::Mutable) \
    macro(StructureRareData_cachedStrings, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::Strings), B3::Mutability::Mutable) \
    macro(StructureRareData_cachedSymbols, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::Symbols), B3::Mutability::Mutable) \
    macro(StructureRareData_cachedStringsAndSymbols, StructureRareData::offsetOfCachedPropertyNames(CachedPropertyNamesKind::StringsAndSymbols), B3::Mutability::Mutable) \
    macro(StructureRareData_cachedPropertyNameEnumeratorAndFlag, StructureRareData::offsetOfCachedPropertyNameEnumeratorAndFlag(), B3::Mutability::Mutable) \
    macro(StructureRareData_specialPropertyCache, StructureRareData::offsetOfSpecialPropertyCache(), B3::Mutability::Mutable) \
    macro(SpecialPropertyCache_cachedToStringTagValue, SpecialPropertyCache::offsetOfCache(CachedSpecialPropertyKey::ToStringTag) + SpecialPropertyCacheEntry::offsetOfValue(), B3::Mutability::Mutable) \
    macro(JSMap_storage, (JSMap::offsetOfStorage()), B3::Mutability::Mutable) \
    macro(JSSet_storage, (JSSet::offsetOfStorage()), B3::Mutability::Mutable) \
    macro(VM_heap_barrierThreshold, VM::offsetOfHeapBarrierThreshold(), B3::Mutability::Mutable) \
    macro(VM_heap_mutatorShouldBeFenced, VM::offsetOfHeapMutatorShouldBeFenced(), B3::Mutability::Mutable) \
    macro(VM_exception, VM::exceptionOffset(), B3::Mutability::Mutable) \
    macro(WatchpointSet_state, WatchpointSet::offsetOfState(), B3::Mutability::Mutable) \
    macro(WeakMapImpl_capacity, WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfCapacity(), B3::Mutability::Mutable) \
    macro(WeakMapImpl_buffer,  WeakMapImpl<WeakMapBucket<WeakMapBucketDataKey>>::offsetOfBuffer(), B3::Mutability::Mutable) \
    macro(WeakMapBucket_value, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfValue(), B3::Mutability::Mutable) \
    macro(WeakMapBucket_key, WeakMapBucket<WeakMapBucketDataKeyValue>::offsetOfKey(), B3::Mutability::Mutable) \
    macro(WebAssemblyModuleRecord_exportsObject, WebAssemblyModuleRecord::offsetOfExportsObject(), B3::Mutability::Mutable) \
    macro(Symbol_symbolImpl, Symbol::offsetOfSymbolImpl(), B3::Mutability::Immutable) \

#define FOR_EACH_INDEXED_ABSTRACT_HEAP(macro) \
    macro(ArrayStorage_vector, ArrayStorage::vectorOffset(), sizeof(WriteBarrier<Unknown>)) \
    macro(CompleteSubspace_allocatorForSizeStep, CompleteSubspace::offsetOfAllocatorForSizeStep(), sizeof(Allocator)) \
    macro(DirectArguments_storage, DirectArguments::storageOffset(), sizeof(EncodedJSValue)) \
    macro(JSLexicalEnvironment_variables, JSLexicalEnvironment::offsetOfVariables(), sizeof(EncodedJSValue)) \
    macro(JSPropertyNameEnumerator_cachedPropertyNamesVectorContents, 0, sizeof(WriteBarrier<JSString>)) \
    macro(JSInternalFieldObjectImpl_internalFields, JSInternalFieldObjectImpl<>::offsetOfInternalFields(), sizeof(WriteBarrier<Unknown>)) \
    macro(ScopedArguments_Storage_storage, 0, sizeof(EncodedJSValue)) \
    macro(WriteBarrierBuffer_bufferContents, 0, sizeof(JSCell*)) \
    macro(characters8, 0, sizeof(LChar)) \
    macro(characters16, 0, sizeof(char16_t)) \
    macro(indexedInt32Properties, 0, sizeof(EncodedJSValue)) \
    macro(indexedDoubleProperties, 0, sizeof(double)) \
    macro(indexedContiguousProperties, 0, sizeof(EncodedJSValue)) \
    macro(scopedArgumentsTableArguments, 0, sizeof(int32_t)) \
    macro(singleCharacterStrings, 0, sizeof(JSString*)) \
    macro(structureTable, 0, sizeof(Structure*)) \
    macro(variables, 0, sizeof(Register)) \
    macro(HasOwnPropertyCache, 0, sizeof(HasOwnPropertyCache::Entry)) \
    macro(SmallIntCache, 0, sizeof(NumericStrings::StringWithJSString)) \
    
#define FOR_EACH_NUMBERED_ABSTRACT_HEAP(macro) \
    macro(properties)
    
// This class is meant to be cacheable between compilations, but it doesn't have to be.
// Doing so saves on creation of nodes. But clearing it will save memory.

class AbstractHeapRepository {
    WTF_MAKE_NONCOPYABLE(AbstractHeapRepository);
public:
    AbstractHeapRepository();
    ~AbstractHeapRepository();
    
    AbstractHeap root;
    
#define ABSTRACT_HEAP_DECLARATION(name) AbstractHeap name;
    FOR_EACH_ABSTRACT_HEAP(ABSTRACT_HEAP_DECLARATION)
#undef ABSTRACT_HEAP_DECLARATION

#define ABSTRACT_FIELD_DECLARATION(name, offset, mutability) AbstractHeap name;
    FOR_EACH_ABSTRACT_FIELD(ABSTRACT_FIELD_DECLARATION)
#undef ABSTRACT_FIELD_DECLARATION
    
    AbstractHeap& JSCell_freeListNext;
    AbstractHeap& ArrayStorage_publicLength;
    AbstractHeap& ArrayStorage_vectorLength;
    
#define INDEXED_ABSTRACT_HEAP_DECLARATION(name, offset, size) IndexedAbstractHeap name;
    FOR_EACH_INDEXED_ABSTRACT_HEAP(INDEXED_ABSTRACT_HEAP_DECLARATION)
#undef INDEXED_ABSTRACT_HEAP_DECLARATION
    
#define NUMBERED_ABSTRACT_HEAP_DECLARATION(name) NumberedAbstractHeap name;
    FOR_EACH_NUMBERED_ABSTRACT_HEAP(NUMBERED_ABSTRACT_HEAP_DECLARATION)
#undef NUMBERED_ABSTRACT_HEAP_DECLARATION

    AbstractHeap& JSString_value;
    AbstractHeap& JSWrapperObject_internalValue;

    AbsoluteAbstractHeap absolute;
    
    IndexedAbstractHeap* forIndexingType(IndexingType indexingType)
    {
        switch (indexingType) {
        case ALL_BLANK_INDEXING_TYPES:
        case ALL_UNDECIDED_INDEXING_TYPES:
            return nullptr;
            
        case ALL_INT32_INDEXING_TYPES:
            return &indexedInt32Properties;
            
        case ALL_DOUBLE_INDEXING_TYPES:
            return &indexedDoubleProperties;
            
        case ALL_CONTIGUOUS_INDEXING_TYPES:
            return &indexedContiguousProperties;
            
        case ALL_ARRAY_STORAGE_INDEXING_TYPES:
            return &ArrayStorage_vector;
            
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return nullptr;
        }
    }
    
    IndexedAbstractHeap& forArrayType(DFG::Array::Type type)
    {
        switch (type) {
        case DFG::Array::Int32:
            return indexedInt32Properties;
        case DFG::Array::Double:
            return indexedDoubleProperties;
        case DFG::Array::Contiguous:
            return indexedContiguousProperties;
        case DFG::Array::ArrayStorage:
        case DFG::Array::SlowPutArrayStorage:
            return ArrayStorage_vector;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return indexedInt32Properties;
        }
    }

    void decorateMemory(const AbstractHeap*, B3::Value*);
    void decorateCCallRead(const AbstractHeap*, B3::Value*);
    void decorateCCallWrite(const AbstractHeap*, B3::Value*);
    void decoratePatchpointRead(const AbstractHeap*, B3::Value*);
    void decoratePatchpointWrite(const AbstractHeap*, B3::Value*);
    void decorateFenceRead(const AbstractHeap*, B3::Value*);
    void decorateFenceWrite(const AbstractHeap*, B3::Value*);
    void decorateFencedAccess(const AbstractHeap*, B3::Value*);

    void computeRangesAndDecorateInstructions();

private:

    struct HeapForValue {
        HeapForValue()
        {
        }

        HeapForValue(const AbstractHeap* heap, B3::Value* value)
            : heap(heap)
            , value(value)
        {
        }
        
        const AbstractHeap* heap { nullptr };
        B3::Value* value { nullptr };
    };

    Vector<HeapForValue> m_heapForMemory;
    Vector<HeapForValue> m_heapForCCallRead;
    Vector<HeapForValue> m_heapForCCallWrite;
    Vector<HeapForValue> m_heapForPatchpointRead;
    Vector<HeapForValue> m_heapForPatchpointWrite;
    Vector<HeapForValue> m_heapForFenceRead;
    Vector<HeapForValue> m_heapForFenceWrite;
    Vector<HeapForValue> m_heapForFencedAccess;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)
