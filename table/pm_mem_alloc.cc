// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "pm_mem_alloc.h"

#include <cassert>

#include "leveldb/table_builder.h"

namespace leveldb {
PMMemAllocator::PMMemAllocator() {
  kPage_slot_count_ = 256 / 6;
  kPage_size_ =
      SuitablePageSize(256 + kPage_slot_count_ * (8 + options_.key_size));
  kPage_count_ = options_.extent_size_ / (kPage_size_ + 1);
  vPage_slot_count_ = (256 - 16) / 2;
  vPage_size_ =
      SuitablePageSize(256 + vPage_slot_count_ * (8 + options_.value_size));
  vPage_count_ = options_.extent_size_ / (vPage_size_ + 1);
}

char* PMMemAllocator::GetNewPage(ExtentType type) {
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

PMExtent* PMMemAllocator::NewExtent(ExtentType type) {
  if (type == key_t) {
    PMExtent* pe = new PMExtent(kPage_count_, kPage_size_, new_extent_id_++, options_);
    Kpage.push_back(pe);
    return pe;
  } else {
    PMExtent* pe = new PMExtent(vPage_count_, vPage_size_, new_extent_id_++, options_);
    Vpage.push_back(pe);
    return pe;
  }
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

}  // namespace leveldb
