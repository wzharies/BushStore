// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/memtable.h"
#include "db/dbformat.h"
#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"
#include "table/pm_mem_alloc.h"
#include "util/coding.h"
#include "util/global.h"
#include "bplustree/bptree.h"

namespace leveldb {

static uint64_t NowMicros() {
    static constexpr uint64_t kUsecondsPerSecond = 1000000;
    struct ::timeval tv;
    ::gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * kUsecondsPerSecond + tv.tv_usec;
}

MemTable::MemTable(const InternalKeyComparator& comparator)
    : comparator_(comparator), refs_(0), table_(comparator_, &arena_), kvCount_(0) {}

MemTable::~MemTable() { assert(refs_ == 0); }

size_t MemTable::ApproximateMemoryUsage() { return arena_.MemoryUsage(); }

int MemTable::KeyComparator::operator()(const char* aptr,
                                        const char* bptr) const {
  // Internal keys are encoded as length-prefixed strings.
  Slice a = GetLengthPrefixedSlice(aptr);
  Slice b = GetLengthPrefixedSlice(bptr);
  return comparator.Compare(a, b);
}

Iterator* MemTable::NewIterator() { return new MemTableIterator(&table_); }

void MemTable::Add2(SequenceNumber s, ValueType type, const Slice& key,
                   const Slice& value) {
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  tag          : uint64((sequence << 8) | type)
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  size_t val_size = value.size();
  size_t internal_key_size = key_size + 8;
  const size_t encoded_len = VarintLength(internal_key_size) +
                             internal_key_size + VarintLength(val_size) +
                             val_size;
  // char* buf = arena_.Allocate(encoded_len);
  char* buf;
  if(TEST_SKIPLIST_NVM){
    buf = (char*)allocatr_->mallocPage(key_t);
  }else{
    buf = arena_.Allocate(encoded_len);
  }
  char* p = EncodeVarint32(buf, internal_key_size);
  std::memcpy(p, key.data(), key_size);
  p += key_size;
  EncodeFixed64(p, (s << 8) | type);
  p += 8;
  p = EncodeVarint32(p, val_size);
  std::memcpy(p, value.data(), val_size);
  assert(p + val_size == buf + encoded_len);
  table_.Insert(buf);
}

void MemTable::Add(SequenceNumber s, ValueType type, const Slice& key,
                   const Slice& value) {
  if(TEST_SKIPLIST_NVM || TEST_SKIPLIST_DRAM){
    return Add2(s, type, key, value);
  }
  // Format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  assert(key_size == 8);
  size_t val_size = value.size();
  size_t internal_key_size = key_size + 8;
  const size_t encoded_len = VarintLength(internal_key_size) +
                             internal_key_size +
                            //  VarintLength(val_size) +
                            //  val_size;
                             6;
  char* buf;
  if(TEST_SKIPLIST_NVM){
    buf = (char*)allocatr_->mallocPage(key_t);
  }else{
    buf = arena_.Allocate(encoded_len);
  }
  char* p = EncodeVarint32(buf, internal_key_size);
  std::memcpy(p, key.data(), key_size);
  p += key_size;
  EncodeFixed64(p, (s << 8) | type);
  p += 8;
  if(type != kTypeDeletion){
    // auto [pointer, index] = write_.writeValue(key, value);
    auto [pointer, index] = write_.writeValue(GetLengthPrefixedSlice(buf), value);
    assert(index >= 4);
    // auto check = [&](uint16_t index, uint32_t pointer){
    //     vPage* addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer) << 12);
    //     assert(addr->v(index).size() == 4096);
    // };
    // check(index, pointer);
    EncodeFixed32(p, pointer);
    p+=4;
    EncodeFixed16(p, index);
    assert(p + 2 == buf + encoded_len);
  }else{
    EncodeFixed32(p, 0);
    p+=4;
    EncodeFixed16(p, 0);
    assert(p + 2 == buf + encoded_len);
  }
  // p = EncodeVarint32(p, val_size);
  // std::memcpy(p, value.data(), val_size);
  // assert(p + val_size == buf + encoded_len);
  table_.Insert(buf);
  kvCount_++;
}
bool MemTable::Get(const LookupKey& key, std::string* value, Status* s, ReadStats& stats){
  if(TEST_SKIPLIST_NVM || TEST_SKIPLIST_DRAM){
    return Get2(key, value, s);
  }
  uint64_t startTime;
  uint64_t endTime;
  if(READ_TIME_ANALYSIS){
    startTime = NowMicros();
  }
  bool ret = Get(key, value, s);
  if(READ_TIME_ANALYSIS){
    endTime = NowMicros();
    stats.readMemTime += (endTime - startTime);
    stats.readMemCount ++;
  }
  return ret;
}

bool MemTable::Get2(const LookupKey& key, std::string* value, Status* s) {
  Slice memkey = key.memtable_key();
  Table::Iterator iter(&table_);
  iter.Seek(memkey.data());
  if (iter.Valid()) {
    // entry format is:
    //    klength  varint32
    //    userkey  char[klength]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // Check that it belongs to same user key.  We do not check the
    // sequence number since the Seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter.key();
    uint32_t key_length;
    const char* key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);
    if (comparator_.comparator.user_comparator()->Compare(
            Slice(key_ptr, key_length - 8), key.user_key()) == 0) {
      // Correct user key
      const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
      switch (static_cast<ValueType>(tag & 0xff)) {
        case kTypeValue: {
          Slice v = GetLengthPrefixedSlice(key_ptr + key_length);
          value->assign(v.data(), v.size());
          return true;
        }
        case kTypeDeletion:
          *s = Status::NotFound(Slice());
          return true;
      }
    }
  }
  return false;
}

bool MemTable::Get(const LookupKey& key, std::string* value, Status* s) {
  Slice memkey = key.memtable_key();
  Table::Iterator iter(&table_);
  iter.Seek(memkey.data());

  auto getValueFromAddr = [](const char* p){
    uint32_t pointer = DecodeFixed32(p);
    uint16_t index = DecodeFixed16(p+4);
    vPage *vp = (vPage* )(getAbsoluteAddr(((uint64_t)pointer) << 12));
    return vp->v(index);
  };

  if (iter.Valid()) {
    // entry format is:
    //    klength  varint32
    //    userkey  char[klength]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // Check that it belongs to same user key.  We do not check the
    // sequence number since the Seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter.key();
    uint32_t key_length;
    const char* key_ptr = GetVarint32Ptr(entry, entry + 5, &key_length);
    if (comparator_.comparator.user_comparator()->Compare(
            Slice(key_ptr, key_length - 8), key.user_key()) == 0) {
      // Correct user key
      const uint64_t tag = DecodeFixed64(key_ptr + key_length - 8);
      switch (static_cast<ValueType>(tag & 0xff)) {
        case kTypeValue: {
          // Slice v = GetLengthPrefixedSlice(key_ptr + key_length);
          Slice v = getValueFromAddr(key_ptr + key_length);
          value->assign(v.data(), v.size());
          return true;
        }
        case kTypeDeletion:
          *s = Status::NotFound(Slice());
          return true;
      }
    }
  }
  return false;
}

}  // namespace leveldb
