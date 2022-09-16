// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ENV_PM_H_
#define STORAGE_LEVELDB_UTIL_ENV_PM_H_

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <libpmemlog.h>
#include <libpmem.h>

#include "include/leveldb/env.h"
#include "util/coding.h"

namespace leveldb {
constexpr const size_t kWritableFileBufferSize = 65536;
constexpr const size_t log_size = 2ULL * 1024 * 1024 * 1024;

class PMSequentialFile final : public SequentialFile {
 public:
  PMSequentialFile(const char* path, size_t poolSize = log_size) {
    if((pmemaddr = (char*)pmem_map_file(path, log_size, PMEM_FILE_CREATE, 666, &mapped_len, &is_pmem)) == NULL){
      perror("pmem_map_file");
      exit(1);
    }
    real_size = DecodeFixed64(pmemaddr);
    if(real_size > mapped_len){
      perror("real size > mappped size");
      exit(1);
    }
    cur_mmap_len += skipped;
  }

  ~PMSequentialFile() override {
    if(pmemaddr != NULL){
      pmem_unmap(pmemaddr, mapped_len);
    }
    pmemaddr = NULL;
  }

  Status Read(size_t n, Slice* result, char* scratch) override {
    // *result = Slice();
    // return Status::OK();
    if(cur_mmap_len >= real_size && n){
      return Status::IOError("out of read size");
    }
    Status status;
    size_t read_size;
    if(cur_mmap_len + n <= real_size){
      read_size = n;
    }else{
      read_size = real_size - cur_mmap_len;
    }
    memcpy(scratch, pmemaddr + cur_mmap_len, read_size);
    cur_mmap_len += read_size;
    *result = Slice(scratch, read_size);
    return status;
  }

  Status Skip(uint64_t n) override {
    if(cur_mmap_len + n > real_size){
      perror("Skip over mappped size");
      exit(1);
    }
    cur_mmap_len += n;
    return Status::OK();
  }

 private:
  char* pmemaddr = NULL;
  size_t mapped_len;
  size_t real_size;
  size_t cur_mmap_len = 0;
  size_t skipped = 4096;
  int is_pmem;
};

class PMWritableFileMMap : public WritableFile {
public:
  PMWritableFileMMap(const char* path, size_t poolSize = log_size) {
    if((pmemaddr = (char*)pmem_map_file(path, log_size, PMEM_FILE_CREATE, 666, &mapped_len, &is_pmem)) == NULL){
      perror("pmem_map_file");
      exit(1);
    }
    cur_mmap_len += skipped;
  }
  ~PMWritableFileMMap() { Close(); }
  Status Append(const Slice& data) override {
    size_t write_size = data.size();
    const char* write_data = data.data();

    // Fit as much as possible into buffer.
    size_t copy_size = std::min(write_size, kWritableFileBufferSize - pos_);
    std::memcpy(buf_ + pos_, write_data, copy_size);
    write_data += copy_size;
    write_size -= copy_size;
    pos_ += copy_size;
    if (write_size == 0) {
      return Status::OK();
    }

    // Can't fit in buffer, so need to do at least one write.
    Status status = FlushBuffer();
    if (!status.ok()) {
      return status;
    }

    // Small writes go to buffer, large writes are written directly.
    if (write_size < kWritableFileBufferSize) {
      std::memcpy(buf_, write_data, write_size);
      pos_ = write_size;
      return Status::OK();
    }
    return WriteUnbuffered(write_data, write_size);
  }

  Status Close() override {
    if(pmemaddr != NULL){
      EncodeFixed64(pmemaddr, cur_mmap_len);
      pmem_flush(pmemaddr, 8);
      pmem_unmap(pmemaddr, mapped_len);
    }
    pmemaddr = NULL;
    return Status::OK();
  }

  Status Flush() override { return FlushBuffer(); }

  Status Sync() override {
    Status status = FlushBuffer();
    if (!status.ok()) {
      return status;
    }

    pmem_drain();
    return Status::OK();
  }

 private:
  Status FlushBuffer() {
    Status status = WriteUnbuffered(buf_, pos_);
    pos_ = 0;
    return status;
  }
  Status WriteUnbuffered(const char* data, size_t size) {
    if(cur_mmap_len + size > mapped_len){
      perror("pm log is out of memory");
      exit(1);
    }
    pmem_memcpy_nodrain(pmemaddr + cur_mmap_len, data, size);
    cur_mmap_len += size;
    return Status::OK();
  }
  char buf_[kWritableFileBufferSize];
  size_t pos_ = 0;
  
  char* pmemaddr = NULL;
  size_t mapped_len;
  size_t cur_mmap_len = 0;
  size_t skipped = 4096;
  int is_pmem;
};

// class PMWritableFile : public WritableFile {
// public:
//   PMWritableFile(const char* path, size_t poolSize = log_size) {
//     plp = pmemlog_create(path, poolSize, 666);
//     if (plp == NULL) {
//       std::cout << "log is already exists. open it!" << std::endl;
//       plp = pmemlog_open(path);
//     }
//     if (plp == NULL) {
//       perror(path);
//       exit(1);
//     }
//   }
//   ~PMWritableFile() { Close(); }
//   Status Append(const Slice& data) override {
//     size_t write_size = data.size();
//     const char* write_data = data.data();

//     // Fit as much as possible into buffer.
//     size_t copy_size = std::min(write_size, kWritableFileBufferSize - pos_);
//     std::memcpy(buf_ + pos_, write_data, copy_size);
//     write_data += copy_size;
//     write_size -= copy_size;
//     pos_ += copy_size;
//     if (write_size == 0) {
//       return Status::OK();
//     }

//     // Can't fit in buffer, so need to do at least one write.
//     Status status = FlushBuffer();
//     if (!status.ok()) {
//       return status;
//     }

//     // Small writes go to buffer, large writes are written directly.
//     if (write_size < kWritableFileBufferSize) {
//       std::memcpy(buf_, write_data, write_size);
//       pos_ = write_size;
//       return Status::OK();
//     }
//     return WriteUnbuffered(write_data, write_size);
//   }

//   Status Close() override {
//     if(plp != nullptr){
//         pmemlog_close(plp);
//     }
//     plp = nullptr;
//     return Status::OK();
//   }

//   Status Flush() override { return FlushBuffer(); }

//   Status Sync() override { return FlushBuffer(); }

//  private:
//   Status FlushBuffer() {
//     Status status = WriteUnbuffered(buf_, pos_);
//     pos_ = 0;
//     return status;
//   }
//   Status WriteUnbuffered(const char* data, size_t size) {
//     if (pmemlog_append(plp, data, size) < 0) {
//       perror("pmemlog_append");
//       exit(1);
//     }
//     return Status::OK();
//   }
//   char buf_[kWritableFileBufferSize];
//   size_t pos_;
//   PMEMlogpool* plp = nullptr;
// };

// class PMWritableFileNoBuffer : public WritableFile {
// public:
//   PMWritableFileNoBuffer(const char* path, size_t poolSize) {
//     plp = pmemlog_create(path, poolSize, 666);
//     if (plp == NULL) {
//       std::cout << "log is already exists. open it!" << std::endl;
//       plp = pmemlog_open(path);
//     }
//     if (plp == NULL) {
//       perror(path);
//       exit(1);
//     }
//   }
//   ~PMWritableFileNoBuffer() { Close(); }
//   Status Append(const Slice& data) override {
//     if (pmemlog_append(plp, data.data(), data.size()) < 0) {
//       perror("pmemlog_append");
//       exit(1);
//     }
//     return Status::OK();
//   }

//   Status Close() override {
//     pmemlog_close(plp);
//     return Status::OK();
//   }

//   Status Flush() override { return Status::OK(); }

//   Status Sync() override { return Status::OK(); }

//  private:
//   PMEMlogpool* plp;
// };
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_UTIL_ENV_PM_H_