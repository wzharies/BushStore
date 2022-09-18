// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_CUCKOO_FILTER_H_
#define STORAGE_LEVELDB_UTIL_CUCKOO_FILTER_H_

#include <cstddef>
#include <cstdint>
#include <random>
#include <atomic>
#include "leveldb/slice.h"

#define TAG_SIZE (2)
#define LID_SIZE (2)
#define ASSOC_WAY (4)
#define MAX_KICK (100)

namespace leveldb {

struct cuckoo_slot{
//    uint16_t tag;
//    uint16_t lid;
    std::atomic_uint32_t tag;
    std::atomic_uint32_t lid;
};

class CuckooFilter {
public:
    CuckooFilter(uint32_t bucket_num);
    void Get(Slice key, uint32_t* value);
    void Put(Slice key, uint32_t value);
    void Delete(Slice key);
    void Delete(Slice key, uint32_t value);
    void GenerateIndexTagHash(Slice key, size_t *index1, size_t *index2, uint32_t *tag);
private:
    struct cuckoo_slot **buckets_;
    struct cuckoo_slot *slots_;
    uint32_t bucket_num_;
    std::random_device rd_;
};


}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ARENA_H_
