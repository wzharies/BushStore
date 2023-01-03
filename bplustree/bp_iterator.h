#ifndef STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_
#define STORAGE_LEVELDB_BPLUSTREE_BP_ITERATOR_H_

#include <vector>
#include "bplustree/bptree.h"
namespace leveldb{
class BP_Iterator{
public:
    BP_Iterator();
    BP_Iterator(lbtree* tree1, std::vector<void*> pages, int start_pos, int kpage_count) : tree1_(tree1), pos_index_(start_pos), kpage_count_(kpage_count){
        cur_index_page_ = 0;
        index_page_ = (bnode *)pages[cur_index_page_];
        kpage_ = (kPage*)index_page_->ch(pos_index_);
        pos_data_ = 0;
        valid_ = kpage_count_ == 0 ? false : true; 
    }
    ~BP_Iterator();

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
            if(pos_index_ < index_page_->num()){
                pos_index_++;
            }else{
                cur_index_page_++;
                kpage_count_--;
                if(kpage_count_ == 0 || cur_index_page_ >= index_pages.size()){
                    valid_ = false;
                    return;
                }
                index_page_ = (bnode *)index_pages[cur_index_page_];
                pos_index_ = 1;
            }
            kpage_ = (kPage*)index_page_->ch(pos_index_);
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
        vPage *addr = reinterpret_cast<vPage *>(pointer() << 12);
        return addr->v(index());
    }

    void clrValue(){
        vPage *addr = reinterpret_cast<vPage *>(pointer() << 12);
        addr->bitmap = addr->bitmap & (~(1ULL << index()));
        if(addr->bitmap == 0){
            //TODO释放页面
        }
    }

    Status status(){
        return Status::OK();
    }
private:
    lbtree *tree1_;
    std::vector<void*> index_pages;
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