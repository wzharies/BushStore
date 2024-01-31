// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "pm_mem_alloc.h"

#include <cassert>
#include <cstdlib>
#include <string>

#include "leveldb/table_builder.h"
#include "util/global.h"
#include "bplustree/tree.h"

namespace leveldb {

static uint64_t NowMicros() {
    static constexpr uint64_t kUsecondsPerSecond = 1000000;
    struct ::timeval tv;
    ::gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec) * kUsecondsPerSecond + tv.tv_usec;
}

uint64_t base_addr;

PMMemAllocator::PMMemAllocator(const Options& options) : options_(options), new_extent_id_(0) {
  std::string path =
  options_.pm_path_ + "/allocator" + ".edb";
  /* create a pmem file and memory map it */
  if(options_.use_pm_){
    while(true){
      void* addr;
      if ((addr = pmem_map_file(path.c_str(), options_.pm_size_, PMEM_FILE_CREATE, 0666,
                            &mapped_len_, &is_pmem_)) == NULL) {
        perror("pmem_map_file");
        exit(1);
      }
      base_addr = reinterpret_cast<uint64_t>(addr);
      // if(((uint64_t)base_addr) % options_.pm_size_ != 0){
      //   pmem_unmap(reinterpret_cast<void*>(base_addr), mapped_len_);
      // }else{
      // pmem_memset_nodrain(addr, 0, options_.pm_size_);
      break;
      // }
    }
  }else{
    void* addr = malloc(options_.pm_size_);
    base_addr = reinterpret_cast<uint64_t>(addr);
  }

  kPage_slot_count_ = (256 - 16) / 8;
  kPage_size_ =
      SuitablePageSize(256 + kPage_slot_count_ * 16);
    kPage_count_ = calPageCount(options_.extent_size_, kPage_size_);

  // vPage_slot_count_ = (256 - 16) / 4;
  vPage_size_ = VPAGE_CAPACITY;
  if(DEBUG_PRINT){
    printf("key page size:%lu, value page size:%lu\n", kPage_size_, vPage_size_);
  }
      // SuitablePageSize(256 + vPage_slot_count_ * (8 + options_.value_size_));
  vPage_count_ = calPageCount(options_.extent_size_, vPage_size_);
}

PMMemAllocator::~PMMemAllocator(){
  if(options_.use_pm_){
    pmem_unmap(reinterpret_cast<void*>(base_addr), mapped_len_);
  }else{
    free(reinterpret_cast<void*>(base_addr));
  }
  for(auto& page : pages){
    delete page;
  }
}

void* PMMemAllocator::PmAlloc(size_t pm_len){
    std::string path = options_.pm_path_ + "/allocator" + std::to_string(pm_len) + ".edb";
    void* addr;
    if ((addr = pmem_map_file(path.c_str(), pm_len, PMEM_FILE_CREATE, 0666,
                          &mapped_len_, &is_pmem_)) == NULL) {
      perror("pmem_map_file");
      exit(1);
    }
    // base_addr = reinterpret_cast<uint64_t>(addr);
    return addr;
}

uint64_t PMMemAllocator::SuitablePageSize(uint64_t page_size) {
  if (page_size <= 512) {
    return 512;
  } else if (page_size <= 1024) {
    return 1024;
  } else if (page_size <= 1280) {
    return 1280;
  } else if (page_size <= 4 * 1024){
    return 4 * 1024;
  } else if (page_size <= 8 * 1024) {
    return 8 * 1024;
  } else if (page_size <= 16 * 1024){
    return 16 * 1024;
  } else if (page_size <= 32 * 1024) {
    return 32 * 1024;
  } else if (page_size <= 64 * 1024){
    return 64 * 1024;
  } else if (page_size <= 128 * 1024) {
    return 128 * 1024;
  } else if (page_size <= 256 * 1024) {
    return 256 * 1024;
  } else if (page_size <= 512 * 1024) {
    return 512 * 1024;
  } else if (page_size <= 1024 * 1024) {
    return 1024 * 1024;
  }
  std::cout<< "error : big value size" << std::endl;
  return -1;
}

std::vector<void*> PMMemAllocator::mallocPage(PageType type, uint64_t count){
  uint64_t startTime;
  if(MALLOC_TIME){
    startTime = NowMicros();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<void*> res;
  res.reserve(count);
  if (type == key_t) {
    for (auto extent : Kpage_) {
      if (!extent->isFull()) {
        std::vector<void*> ret = extent->getNewPage(count);
        res.insert(res.end(), ret.begin(), ret.end());
        count -= ret.size();
        if(count == 0){
          break;
        }
      }
    }
  } else {
    for (auto extent : Vpage_) {
      if (!extent->isFull()) {
        std::vector<void*> ret = extent->getNewPage(count);
        res.insert(res.end(), ret.begin(), ret.end());
        count -= ret.size();
        if(count == 0){
          break;
        }
      }
    }
  }
  while(count > 0){
    auto extent = NewExtent(type);
    std::vector<void*> ret = extent->getNewPage(count);
    res.insert(res.end(), ret.begin(), ret.end());
    count -= ret.size();
    if(count == 0){
      break;
    }
  }
  if(MALLOC_TIME){
    (*used_time_) += (NowMicros() - startTime);
  }
  return res;
}

void* PMMemAllocator::mallocPage(PageType type) {
  uint64_t startTime;
  if(MALLOC_TIME){
    startTime = NowMicros();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  // char* page_addr;
  if (type == key_t) {
    for(int i = Kpage_.size() - 1; i >= 0; i--){
      if(!Kpage_[i]->isFull()){
        auto result =  Kpage_[i]->getNewPage();
        if(MALLOC_TIME){
          (*used_time_) += (NowMicros() - startTime);
        }
        return result;
      }
    }
    // for (auto extent : Kpage_) {
    //   if (!extent->isFull()) {
    //     return extent->getNewPage();
    //   }
    // }
  } else {
    for(int i = Vpage_.size() - 1; i >= 0; i--){
      if(!Vpage_[i]->isFull()){
        auto result = Vpage_[i]->getNewPage();
        if(MALLOC_TIME){
          (*used_time_) += (NowMicros() - startTime);
        }
        return result;
      }
    }
    // for (auto extent : Vpage_) {
    //   if (!extent->isFull()) {
    //     return extent->getNewPage();
    //   }
    // }
  }
  auto extent = NewExtent(type);
  auto result = extent->getNewPage();
  if(MALLOC_TIME){
    (*used_time_) += (NowMicros() - startTime);
  }
  return result;
}

void PMMemAllocator::freePage(char* addr, PageType type) {
  uint64_t startTime;
  if(MALLOC_TIME){
    startTime = NowMicros();
  }
  std::lock_guard<std::mutex> lock(mutex_);
  size_t index = ((uint64_t)addr - base_addr) / options_.extent_size_;
  assert(index < pages.size());
  pages[index]->freePage(addr);
  if(MALLOC_TIME){
    (*used_time_) += (NowMicros() - startTime);
  }
}

void PMMemAllocator::Sync() {
  if(options_.use_pm_){
    if (is_pmem_)
      pmem_persist(reinterpret_cast<void*>(base_addr), mapped_len_);
    else
      pmem_msync(reinterpret_cast<void*>(base_addr), mapped_len_);
  }
}

PMExtent* PMMemAllocator::NewExtent(PageType type) {
  PMExtent* pe;
  int cur_extent_id = pages.size();
  if(cur_extent_id >= options_.pm_size_ / options_.extent_size_){
    printf("out of pm memory size, program will exit, please reserve enough space for GC or reduce memory_rate.\n");
    exit(-1);
  }
  if (type == key_t) {
    pe = new PMExtent(kPage_count_, kPage_size_, (char*)getAbsoluteAddr(((uint64_t)cur_extent_id) * options_.extent_size_));
    pages.push_back(pe);
    Kpage_.push_back(pe);
  } else {
    pe = new PMExtent(vPage_count_, vPage_size_, (char*)getAbsoluteAddr(((uint64_t)cur_extent_id) * options_.extent_size_));
    pages.push_back(pe);
    Vpage_.push_back(pe);
  }
  assert(pe->page_start_addr_ + pe->page_count_ * pe->page_size_ <= (char*)getAbsoluteAddr(((uint64_t)cur_extent_id + 1) * options_.extent_size_));
  return pe;
}

}  // namespace leveldb
