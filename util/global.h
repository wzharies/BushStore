// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Simple hash function used for internal data structures

#ifndef STORAGE_LEVELDB_UTIL_GLOBAL_H_
#define STORAGE_LEVELDB_UTIL_GLOBAL_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace leveldb {
constexpr int VPAGE_CAPACITY = 512 * 1024;
constexpr int FLUSH_SIZE = 16 * 1024;
constexpr bool NEW_WAL = true;
constexpr int MAX_FILE_NUM = 2;
constexpr int MAX_BNODE_NUM = 150;
constexpr int TASK_COUNT = 64;
constexpr int max_size = 64 * 1024;
constexpr bool use_pm = true;
constexpr double memory_rate = 0.85;

constexpr bool KV_SEPERATE = true; // must be true
constexpr int STOP_GC_COUNT = 5;
constexpr double STOP_GC_RATE = 0.05;
constexpr uint64_t STOP_THRESHOLD = 10ULL * 1024 * 1024 * 1024;
constexpr int MAX_GC_VPAGE = 1024 * 10;// 5G / 512K = 10K


constexpr bool TEST_FLUSH_SSD = false; // must be false
constexpr bool TEST_SKIPLIST_DRAM = false; // must be false;
constexpr bool TEST_SKIPLIST_NVM = false; // must be false;
constexpr bool TEST_BPTREE_NVM = false; // must be false;
constexpr bool TEST_BPTREE_DRAM = false; // must be false;
constexpr bool TEST_CUCKOOFILTER = false; // must be false;
constexpr bool TEST_CUCKOO_DELETE = false; // must be false;

// compact
constexpr int initMemtableSize = 1 * 1024 * 1024;
constexpr int addMemtableSize = 4 * 1024 * 1024;
constexpr int minMergeCount = 1;
constexpr int maxMergeCount = 15;
constexpr int L0BufferCount = 40;
constexpr int L0BufferCountMax = 50;
constexpr int MinCompactionL0Count = 8;

// compile
constexpr bool CUCKOO_FILTER = true;
constexpr bool BLOOM_FILTER = false;
constexpr bool DEBUG_CHECK = false;
constexpr bool DEBUG_PRINT = true;
constexpr bool TIME_ANALYSIS = true; // must be true, dynamic change B+Tree size reley on it
constexpr bool READ_TIME_ANALYSIS = false;
constexpr bool WRITE_TIME_ANALYSIS = false;

constexpr bool SKIPLIST_NVM = false; // no use
constexpr bool MALLO_CFLUSH = false;

struct ReadStats {
  int64_t readCount = 0;
  int64_t readRight = 0;
  int64_t readWrong = 0;
  int64_t readNotFound = 0;
  int64_t readL0Found = 0;
  int64_t readL1Found = 0;
  int64_t readL2Found = 0;
  int64_t readExpire = 0;
  int64_t bloomfilter = 0;
  int64_t bloomNofilter = 0;

  uint64_t readMemTime = 0;
  uint64_t readL0Time = 0;
  uint64_t readL1Time = 0;
  uint64_t readL2Time = 0;

  int64_t readMemCount = 0;
  int64_t readL0Count = 0;
  int64_t readL1Count = 0;
  int64_t readL2Count = 0;

  int64_t readBloomCount = 0;
  int64_t readBloomTime = 0;
  int64_t readCuckooCount = 0;
  int64_t readCuckooTime = 0;


  std::string getStats() {
    readL0Time += readCuckooTime;
    double avgMemTime =
        readMemCount == 0 ? 0 : readMemTime * 1.0 / readMemCount;
    double avgL0Time = readL0Count == 0 ? 0 : readL0Time * 1.0 / readL0Count;
    double avgL1Time = readL1Count == 0 ? 0 : readL1Time * 1.0 / readL1Count;
    double avgL2Time = readL2Count == 0 ? 0 : readL2Time * 1.0 / readL2Count;
    double avgcuckooTime = readCuckooCount == 0 ? 0 : readCuckooTime * 1.0 / readCuckooCount;
    double avgbloomTime = readBloomCount == 0 ? 0 : readBloomTime * 1.0 / readBloomCount;
    std::string s="read time analysis:";
    s = s + 
        "\ncount        : " + std::to_string(readCount) +
        "\nright        : " + std::to_string(readRight) +
        "\nwrong        : " + std::to_string(readWrong) +
        "\nnotfound     : " + std::to_string(readNotFound) +
        "\nreadL0Found   : " + std::to_string(readL0Found) +
        "\nexpire       : " + std::to_string(readExpire) +
        "\nbloomfilte r : " + std::to_string(bloomfilter) +
        "\nbloomNofilter: " + std::to_string(bloomNofilter) +
        "\n<MemTime>    : " + std::to_string(readMemTime) +
        "\n<L0Time>     : " + std::to_string(readL0Time) +
        "\n<L1Time>     : " + std::to_string(readL1Time) +
        "\n<L2Time>     : " + std::to_string(readL2Time) +
        "\n<bloomTime>     : " + std::to_string(readBloomTime) +
        "\n<cuckooTime>     : " + std::to_string(readCuckooTime) +
        "\n[MemCount]   : " + std::to_string(readMemCount) +
        "\n[L0Count]    : " + std::to_string(readL0Count) +
        "\n[L1Count]    : " + std::to_string(readL1Count) +
        "\n[L2Count]    : " + std::to_string(readL2Count) +
        "\n[BloomCount] : " + std::to_string(readBloomCount) +
        "\n[CuckooCount]: " + std::to_string(readCuckooCount) +
        "\n{MemTimeAVG} : " + std::to_string(avgMemTime) +
        "\n{L0TimeAVG}  : " + std::to_string(avgL0Time) +
        "\n{L1TimeAVG}  : " + std::to_string(avgL1Time) +
        "\n{L2TimeAVG}  : " + std::to_string(avgL2Time) +
        "\n{BloomTimeAVG}  : " + std::to_string(avgbloomTime) +
        "\n{CuckooTimeAVG}  : " + std::to_string(avgcuckooTime);
    return s;
  }
};

struct WriteStats {
  uint64_t writeL0Time = 0;
  uint64_t writeL1Time = 0;
  uint64_t writeL2Time = 0;

  int64_t writeL0Count = 0;
  int64_t writeL1Count = 0;
  int64_t writeL2Count = 0;
  std::string getStats() {
    double avgL0Time = writeL0Count == 0 ? 0 : writeL0Time * 1.0 / writeL0Count;
    double avgL1Time = writeL1Count == 0 ? 0 : writeL1Time * 1.0 / writeL1Count;
    double avgL2Time = writeL2Count == 0 ? 0 : writeL2Time * 1.0 / writeL2Count;
    std::string s = "write time analysis:";
    s = s + "\n<L0Time>    : " + std::to_string(writeL0Time) +
        "\n<L1Time>    : " + std::to_string(writeL1Time) +
        "\n<L2Time>    : " + std::to_string(writeL2Time) +
        "\n[L0Count]   : " + std::to_string(writeL0Count) +
        "\n[L1Count]   : " + std::to_string(writeL1Count) +
        "\n[L2Count]   : " + std::to_string(writeL2Count) +
        "\n{L0TimeAVG} : " + std::to_string(avgL0Time) +
        "\n{L1TimeAVG} : " + std::to_string(avgL1Time) +
        "\n{L2TimeAVG} : " + std::to_string(avgL2Time);
    return s;
  }
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_GLOBAL_H_
