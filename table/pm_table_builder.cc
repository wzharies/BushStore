// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table_builder.h"

#include <cassert>

#include "pm_table_builder.h"
#inlcude "pm_table_alloc.h"
#include "libpmem.h"

namespace leveldb {

#define FNV1_PRIME_32 0x01000193
#define FNV1_BASE_32 2166136261U

/* FNV-1a core implementation returning a 32 bit checksum over the first
 * LEN bytes in INPUT.  HASH is the checksum over preceding data (if any).
 */
inline uint32_t fnv1a_32(const void *input, size_t len) {
    uint32_t hash = FNV1_BASE_32;
    const unsigned char *data = input;
    const unsigned char *end = data + len;

    for (; data != end; ++data){
        hash ^= *data;
        hash *= FNV1_PRIME_32;
    }
    return hash;
}
PMTableBuilder::PMTableBuilder(PMMemAllocator* pm_alloc){
    pm_alloc_ = pm_alloc;
    key_buf_ = new char[max_size_];
    value_buf_ = new char[max_size_];
}
PMTableBuilder::~PMTableBuilder(){
    delete[] key_buf_;
    delete[] value_buf_
}

void PMTableBuilder::flush_kpage(){
    pmem_memcpy_persist(key_raw_, key_buf_, pm_alloc->GetkPageSize());
}

void PMTableBuilder::flush_vpage(){
    pmem_memcpy_persist(value_raw_, value_buf_, pm_alloc->GetvPageSize());

}

void PMTableBuilder::Add(const Slice& key, const Slice& value){
    uint64_t key_total_size = key.size_ + 8;
    uint64_t value_total_size = key.size_ + value.size_ + 8 + 8;
    uint32_t addr = raw_;
    if((offset_ + total_size ) > nvm_cf_->GetSstableEachSize()){
        printf("error:write l0 sstable size over!\n");
        return;
    }

    //写入key 和 value的值到buf
    std::string skey;
    std::string key_value;
    PutFixed64(&skey, key.size_);
    skey.append(key.data_, key.size_);

    key_value += skey;
    PutFixed64(&key_value, value.size_);
    key_value.append(value.data_, value.size_);

    memcpy(key_buf_ + key_offset_ + 256, skey.c_str(), key_total_size);
    memcpy(value_buf_ + value_offset_ + 256, key_value.c_str(), value_total_size);

    //写入key和value的索引到buf
    assert(keys_num_ < 256);
    addr = addr + keys_num;

    EncodeFixed16(key_buf_ + keys_num_ * 2, fnv1a_32(key.data_, key.size_));
    EncodeFixed32(key_buf_ + 84 + keys_num_ * 4, addr);
    EncodeFixed16(value_buf_ + values_num_ * 2, value_offset_);

    key_offset_ += key_total_size;
    value_offset_ += value_total_size;
    keys_num_++;
    values_num_++;
    if(keys_num_ > 256 / 6){
        flush_key();
    }
    if(values_num_ > (256 - 16) / 2) {
        flush_value();
    }
}

Status PMTableBuilder::Finish(){
    flush_key();
    flush_value();
    return Status();

}

}  // namespace leveldb
