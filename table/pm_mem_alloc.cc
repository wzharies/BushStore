// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "pm_mem_alloc.h"

#include <cassert>

#include "leveldb/table_builder.h"

namespace leveldb {


uint64_t base_addr;

PMMemAllocator::PMMemAllocator(const Options& options) : options_(options), new_extent_id_(0) {
  std::string path =
  options_.pm_path_ + "allocator" + ".edb";
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
      pmem_memset_nodrain(addr, 0, options_.pm_size_);
      break;
      // }
    }
  }else{
    void* addr = calloc(1, options_.pm_size_);
    base_addr = reinterpret_cast<uint64_t>(addr);
  }

  kPage_slot_count_ = (256 - 16) / 6;
  kPage_size_ =
      SuitablePageSize(256 + kPage_slot_count_ * 16);
  //算上bitmap的空间, 减去bitmap->num的8Byte，然后每个kpage多用了1bit
  kPage_count_ = calPageCount(options_.extent_size_, kPage_size_);

  vPage_slot_count_ = (256 - 16) / 4;
  vPage_size_ =
      SuitablePageSize(256 + vPage_slot_count_ * (8 + options_.value_size_));
  vPage_count_ = calPageCount(options_.extent_size_, vPage_size_);
}

PMMemAllocator::~PMMemAllocator(){
  if(options_.use_pm_){
    pmem_unmap(reinterpret_cast<void*>(base_addr), mapped_len_);
  }else{
    free(reinterpret_cast<void*>(base_addr));
  }
}

uint64_t PMMemAllocator::SuitablePageSize(uint64_t page_size) {
  if (page_size < 512) {
    return 512;
  } else if (page_size < 1024) {
    return 1024;
  } else if (page_size < 1280) {
    return 1280;
  } else if (page_size < 4 * 1024){
    return 4 * 1024;
  } else if (page_size < 8 * 1024) {
    return 8 * 1024;
  } else if (page_size < 16 * 1024){
    return 16 * 1024;
  } else if (page_size < 32 * 1024) {
    return 32 * 1024;
  } else if (page_size < 64 * 1024){
    return 64 * 1024;
  } else if (page_size < 128 * 1024) {
    return 128 * 1024;
  }
  return -1;
}

void* PMMemAllocator::mallocPage(PageType type) {
  // char* page_addr;
  if (type == key_t) {
    for (auto extent : Kpage_) {
      if (!extent->isFull()) {
        return extent->getNewPage();
      }
    }
  } else {
    for (auto extent : Vpage_) {
      if (!extent->isFull()) {
        return extent->getNewPage();
      }
    }
  }
  auto extent = NewExtent(type);
  return extent->getNewPage();
}

void PMMemAllocator::freePage(char* addr, PageType type) {
  size_t index = ((uint64_t)addr - base_addr) / options_.extent_size_;
  assert(index < pages.size());
  pages[index]->freePage(addr);
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
  assert(cur_extent_id < options_.pm_size_ / options_.extent_size_);
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
