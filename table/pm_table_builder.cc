// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/table_builder.h"

#include <cassert>

#include "pm_table_builder.h"
#inlcude "pm_table_alloc.h"
#include "libpmem.h"

namespace leveldb {

#define FNV1_PRIME_32 0x01000193
#define FNV1_BASE_32 2166136261U

/* FNV-1a core implementation returning a 32 bit checksum over the first
 * LEN bytes in INPUT.  HASH is the checksum over preceding data (if any).
 */
inline uint32_t fnv1a_32(const void *input, size_t len) {
    uint32_t hash = FNV1_BASE_32;
    const unsigned char *data = input;
    const unsigned char *end = data + len;

    for (; data != end; ++data){
        hash ^= *data;
        hash *= FNV1_PRIME_32;
    }
    return hash;
}
PMTableBuilder::PMTableBuilder(PMMemAllocator* pm_alloc, FileEntry* file, char* raw){
    pm_alloc_ = pm_alloc;
    key_buf_ = new char[max_size_];
    value_buf_ = new char[max_size_];
}
PMTableBuilder::~PMTableBuilder(){
    delete[] key_buf_;
    delete[] value_buf_
}

void PMTableBuilder::flush_kpage(){
    pmem_memcpy_persist(key_raw_, key_buf_, pm_alloc->GetkPageSize());
}

void PMTableBuilder::flush_vpage(){
    
}

void PMTableBuilder::Add(const Slice& key, const Slice& value){
    uint64_t key_total_size = key.size_ + 8;
    uint64_t value_total_size = key.size_ + value.size_ + 8 + 8;
    uint32_t addr = raw_;
    if((offset_ + total_size ) > nvm_cf_->GetSstableEachSize()){
        printf("error:write l0 sstable size over!\n");
        return;
    }

    //写入key 和 value的值到buf
    std::string skey;
    std::string key_value;
    PutFixed64(&skey, key.size_);
    skey.append(key.data_, key.size_);

    key_value += skey;
    PutFixed64(&key_value, value.size_);
    key_value.append(value.data_, value.size_);

    memcpy(key_buf_ + key_offset_ + 256, skey.c_str(), key_total_size);
    memcpy(value_buf_ + value_offset_ + 256, key_value.c_str(), value_total_size);

    //写入key和value的索引到buf
    assert(keys_num_ < 256);
    addr = addr + keys_num;

    EncodeFixed16(key_buf_ + keys_num_ * 2, fnv1a_32(key.data_, key.size_));
    EncodeFixed32(key_buf_ + 84 + keys_num_ * 4, addr);
    EncodeFixed16(value_buf_ + values_num_ * 2, value_offset_);

    key_offset_ += key_total_size;
    value_offset_ += value_total_size;
    keys_num_++;
    values_num_++;
    if(keys_num_ > 256 / 6){
        flush_key();
    }
    if(values_num_ > (256 - 16) / 2) {
        flush_value();
    }
}

Status PMTableBuilder::Finish(){
    file_->keys_num = keys_num_;
    file_->keys_meta = new KeysMetadata[keys_num_];
    int index =0;
    for(auto key_ : first_indexs_){
        file_->keys_meta[index].key = key_->key;
        file_->keys_meta[index].offset=key_->offset;
        file_->keys_meta[index].size = key_->size;
        index++;
    }

    //nvm_cf_->UpdateKeyNext(file_);
    RECORD_LOG("finish L0 table:%lu keynum:%lu size:%2.f MB\n",file_->filenum,file_->keys_num,1.0*offset_/1048576);

    std::string metadatas;
    for(unsigned i=0;i < file_->keys_num;i++){
        Slice key = file_->keys_meta[i].key.Encode();
        PutFixed64(&metadatas,key.size());
        metadatas.append(key.data(),key.size());
        PutFixed32(&metadatas,file_->keys_meta[i].next);
        PutFixed64(&metadatas,file_->keys_meta[i].offset);
        PutFixed64(&metadatas,file_->keys_meta[i].size);
    }
    if((offset_ + metadatas.size()) > max_size_){
        printf("error:write l0 sstable's metadata size over!size:%lu max:%lu\n",offset_ + metadatas.size(),max_size_);
        return Status::IOError();
    }

    keys_meta_size_ = metadatas.size();

    memcpy(buf_ + offset_, metadatas.c_str(), metadatas.size());

    //memcpy(raw_ + offset_,metadatas.c_str(),metadatas.size());
    pmem_memcpy_persist(raw_ , buf_, offset_ + keys_meta_size_);  //libpmem api
    RECORD_LOG("finish L0 table:%lu keynum:%lu size:%.2f MB metadata:%.2f MB\n",file_->filenum,file_->keys_num,1.0*offset_/1048576,metadatas.size()/1048576.0);
    /* std::string buf;
    int32_t a = -1;
    PutFixed32(&buf,a);
    printf("buf:%s   \n",buf.c_str());
    int32_t b = 0;
    b = DecodeFixed32(buf.c_str());
    printf("b:%d   \n",b);*/

    
    return Status();

}

}  // namespace leveldb
