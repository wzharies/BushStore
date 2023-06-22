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
constexpr int MAX_FILE_NUM = 2;
constexpr int MAX_BNODE_NUM = 150;
constexpr int TASK_COUNT = 64;
constexpr int max_size = 64 * 1024;
constexpr bool use_pm = true;
constexpr double memory_rate = 0.75;

// compact
constexpr int initMemtableSize = 1 * 1024 * 1024;
constexpr int addMemtableSize = 2 * 1024 * 1024;
constexpr int minMergeCount = 1;
constexpr int maxMergeCount = 15;
constexpr int L0BufferCount = 25;

// compile
constexpr bool CUCKOO_FILTER = true;
constexpr bool DEBUG_CHECK = false;
constexpr bool DEBUG_PRINT = false;
constexpr bool TIME_ANALYSIS = true;
constexpr bool READ_TIME_ANALYSIS = false;

  struct ReadStats {
    int64_t readCount = 0;
    int64_t readRight = 0;
    int64_t readWrong = 0;
    int64_t readNotFound = 0;
    int64_t readL0Found = 0;
    int64_t readL1Found = 0;
    int64_t readL2Found = 0;
    int64_t readExpire = 0;

    uint64_t readMemTime = 0;
    uint64_t readL0Time = 0;
    uint64_t readL1Time = 0;
    uint64_t readL2Time = 0;

    int64_t readMemCount = 0;
    int64_t readL0Count = 0;
    int64_t readL1Count = 0;
    int64_t readL2Count = 0;
    std::string getStats(){
      double avgMemTime = readMemCount == 0 ? 0 : readMemTime * 1.0 / readMemCount;
      double avgL0Time = readL0Count == 0 ? 0 : readL0Time * 1.0 / readL0Count;
      double avgL1Time = readL1Count == 0 ? 0 : readL1Time * 1.0 / readL1Count;
      double avgL2Time = readL2Count == 0 ? 0 : readL2Time * 1.0 / readL2Count;
      std::string s;
      s = s + "count       : " + std::to_string(readCount)
            + "\nright       : " + std::to_string(readRight)
            + "\nwrong       : " + std::to_string(readWrong)
            + "\nnotfound    : " + std::to_string(readNotFound)
            + "\nexpire      : " + std::to_string(readExpire)
            + "\n<MemTime>   : " + std::to_string(readMemTime)
            + "\n<L0Time>    : " + std::to_string(readL0Time)
            + "\n<L1Time>    : " + std::to_string(readL1Time)
            + "\n<L2Time>    : " + std::to_string(readL2Time)
            + "\n[MemCount]  : " + std::to_string(readMemCount)
            + "\n[L0Count]   : " + std::to_string(readL0Count)
            + "\n[L1Count]   : " + std::to_string(readL1Count)
            + "\n[L2Count]   : " + std::to_string(readL2Count)
            + "\n{MemTimeAVG}: " + std::to_string(avgMemTime)
            + "\n{L0TimeAVG} : " + std::to_string(avgL0Time)
            + "\n{L1TimeAVG} : " + std::to_string(avgL1Time)
            + "\n{L2TimeAVG} : " + std::to_string(avgL2Time);
      return s;
    }
  };

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_GLOBAL_H_
