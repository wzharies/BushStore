// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "pm_mem_alloc.h"

#include <cassert>

#include "leveldb/table_builder.h"

namespace leveldb {
PMMemAllocator::PMMemAllocator(Options& options) : options_(options) {
  std::string path =
  options_.pm_path_ + "allocator" + ".edb";
  /* create a pmem file and memory map it */
  while(true){
    if ((Options::base_addr=
            (char *)pmem_map_file(path, options_.pm_size_, PMEM_FILE_CREATE, 0666,
                          &mapped_len_, &is_pmem_)) == NULL) {
      perror("pmem_map_file");
      exit(1);
    }
    if(((uint64_t)Options::base_addr) % options_.pm_size_ != 0){
      pmem_unmap(Options::base_addr, mapped_len_);
    }else{
      break;
    }
  }

  kPage_slot_count_ = 256 / 6;
  kPage_size_ =
      SuitablePageSize(256 + kPage_slot_count_ * (8 + options_.key_size));
  kPage_count_ = options_.extent_size_ / (kPage_size_ + 1);

  vPage_slot_count_ = (256 - 16) / 2;
  vPage_size_ =
      SuitablePageSize(256 + vPage_slot_count_ * (8 + options_.value_size));
  vPage_count_ = options_.extent_size_ / (vPage_size_ + 1);
}

uint64_t PMMemAllocator::SuitablePageSize(uint64_t page_size) {
  if (page_size < 1024) {
    return 1024;
  } else if (page_size < 1280) {
    return 1280;
  } else if (page_size < 128 * 1024) {
    return 128 * 1024;
  }
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

void PMMemAllocator::freePage(void* addr, PageType type) {
  size_t index = ((uint64_t)addr - Options::base_addr) / options_.extent_size_;
  pages[index]->freePage(addr);
}

void PMMemAllocator::Sync() {
  if (is_pmem_)
    pmem_persist(Options::base_addr, mapped_len_);
  else
    pmem_msync(Options::base_addr, mapped_len_);
}
PMExtent* PMMemAllocator::NewExtent(PageType type) {
  PMExtent* pe;
  assert(new_extent_id_ < pm_size_ / extent_size_);
  if (type == key_t) {
    pe = new PMExtent(kPage_count_, kPage_size_, pmem_addr_ + new_extent_id_ * options_.pm_size_);
    pages[new_extent_id_] = pe;
    new_extent_id_++;
    Kpage.push_back(pe);
  } else {
    pe = new PMExtent(vPage_count_, vPage_size_, pmem_addr_ + new_extent_id_ * options_.pm_size_);
    pages[new_extent_id_] = pe;
    new_extent_id_++;
    Vpage.push_back(pe);
  }
  return pe;
}

}  // namespace leveldb
