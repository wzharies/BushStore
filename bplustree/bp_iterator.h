#ifndef STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_
#define STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_

#include <vector>
#include "bplustree/bptree.h"
#include "table/pm_mem_alloc.h"
namespace leveldb{
class BP_Iterator{
public:
    BP_Iterator();
    BP_Iterator(PMMemAllocator *alloc, lbtree* tree1, std::vector<void*>& pages, int start_pos, int kpage_count) : alloc_(alloc), tree1_(tree1), pages_(pages), pos_index_(start_pos), kpage_count_(kpage_count){
        cur_index_page_ = 0;
        index_page_ = (bnode *)pages_[cur_index_page_];
        kpage_ = (kPage*)index_page_->ch(pos_index_);
        pos_data_ = 0;
        valid_ = kpage_count_ == 0 ? false : true; 
        kPages_.reserve(kpage_count);
        kPages_.push_back(kpage_);
    }
    ~BP_Iterator(){
        releaseKpage();
    }

    void releaseKpage (){
        // std::cout<< "free kPage from : " << ((kPage*)kPages_.front())->minRawKey() <<" to :" << ((kPage*)kPages_.back())->maxRawKey() <<std::endl;
        for(auto& kpage : kPages_){
            alloc_->freePage((char*)kpage, key_t);
        }
    }

    bool Valid(){
        return valid_;
    }

    void SeekToFirst(){};

    void SeekToLast(){};

    void Seek(key_type key){}

    void Next(){
        pos_data_++;
        //超过kpage索引范围
        if(pos_data_ == kpage_->nums){
            //超过bnode的索引范围
            kpage_count_--;
            if(pos_index_ < index_page_->num()){
                pos_index_++;
            }else{
                cur_index_page_++;
                if(cur_index_page_ >= pages_.size()){
                    valid_ = false;
                    return;
                }
                index_page_ = (bnode *)pages_[cur_index_page_];
                pos_index_ = 1;
            }
            if(kpage_count_ == 0){
                valid_ = false;
                return;
            }
            kpage_ = (kPage*)index_page_->ch(pos_index_);
            kPages_.push_back(kpage_);
            pos_data_ = 0;
        }
    }

    void Prev(){

    }

    Slice key(){
        return kpage_->k(pos_data_);
    }

    unsigned char finger(){
        return kpage_->finger[pos_data_];
    }

    uint32_t pointer(){
        return kpage_->pointer[pos_data_];
    }

    unsigned char index(){
        return kpage_->index[pos_data_];
    }

    Slice value(){
        //TODO 也许可以优化？
        vPage *addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
        return addr->v(index());
    }

    void clrValue(){
        vPage *addr = (vPage*)getAbsoluteAddr(((uint64_t)pointer()) << 12);
        addr->clrBitMap(index());
        addr->bitmap = addr->bitmap & (~(1ULL << index()));
        if(addr->bitmap == 0){
            //TODO vPage需要删除,最好把pmALloc设置为全局变量或者单例
            // std::cout<< "free : " << addr <<std::endl;
            alloc_->freePage((char*)addr, value_t);
        }
    }

    Status status(){
        return Status::OK();
    }
private:
    PMMemAllocator *alloc_;
    lbtree *tree1_;
    std::vector<void*>& pages_;
    std::vector<void*> kPages_;
    int kpage_count_; //几个kpage，即bnode中kv的数量

    int cur_index_page_; //第几个bnode
    bnode* index_page_;
    kPage* kpage_;
    int pos_index_; //bnode的索引
    int pos_data_;  //kpage内部的索引
    bool valid_ = false;
};

} // namespace leveldb

#endif