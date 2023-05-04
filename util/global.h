// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Simple hash function used for internal data structures

#ifndef STORAGE_LEVELDB_UTIL_GLOBAL_H_
#define STORAGE_LEVELDB_UTIL_GLOBAL_H_

#include <cstddef>
#include <cstdint>

namespace leveldb {
constexpr int MAX_FILE_NUM = 2;
constexpr int MAX_BNODE_NUM = 150;
constexpr int TASK_COUNT = 64;
constexpr bool LOG_PM = true;
constexpr int max_size = 64 * 1024;
constexpr bool use_pm = true;
constexpr double memory_rate = 0.75;

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_GLOBAL_H_
