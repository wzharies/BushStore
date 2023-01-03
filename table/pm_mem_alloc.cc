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
  //算上bitmap的空间
  kPage_count_ = options_.extent_size_ * 8 / (kPage_size_ * 8 + 1);

  vPage_slot_count_ = (256 - 16) / 4;
  vPage_size_ =
      SuitablePageSize(256 + vPage_slot_count_ * (8 + options_.value_size_));
  vPage_count_ = options_.extent_size_ * 8 / (vPage_size_ * 8 + 1);
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
  pages[index]->freePage(addr);
}

void PMMemAllocator::Sync() {
  if (is_pmem_)
    pmem_persist(reinterpret_cast<void*>(base_addr), mapped_len_);
  else
    pmem_msync(reinterpret_cast<void*>(base_addr), mapped_len_);
}
PMExtent* PMMemAllocator::NewExtent(PageType type) {
  PMExtent* pe;
  assert(new_extent_id_ < options_.pm_size_ / options_.extent_size_);
  if (type == key_t) {
    pe = new PMExtent(kPage_count_, kPage_size_, (char*)getAbsoluteAddr(new_extent_id_ * options_.extent_size_));
    pages[new_extent_id_] = pe;
    new_extent_id_++;
    Kpage_.push_back(pe);
  } else {
    pe = new PMExtent(vPage_count_, vPage_size_, (char*)getAbsoluteAddr(new_extent_id_ * options_.extent_size_));
    pages[new_extent_id_] = pe;
    new_extent_id_++;
    Vpage_.push_back(pe);
  }
  return pe;
}

}  // namespace leveldb
