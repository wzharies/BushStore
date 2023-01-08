#ifndef STORAGE_LEVELDB_TABLE_BITMAP_H_
#define STORAGE_LEVELDB_TABLE_BITMAP_H_
#include<memory>

namespace leveldb{
class Bitmap{
public:
    uint64_t nums_;
    char bitmaps_[];

public:
    void set(size_t index){
        if(index > nums_) return ;
        int charIndex = (index >> 3);
        int innerIndex = (index & 7);
        bitmaps_[charIndex] |= (1 << innerIndex);
    };

    void clr(size_t index){
        if(index > nums_) return ;
        int charIndex = (index >> 3);
        int innerIndex = (index & 7);
        bitmaps_[charIndex] ^= (1 << innerIndex);
    };

    bool get(size_t index){
        if(index > nums_) return 0;
        int charIndex = (index >> 3);
        int innerIndex = (index & 7);
        return (bitmaps_[charIndex] >> innerIndex) & 1;
    };

    void getEmpty(size_t &last_empty){
        while(get(last_empty)){
            last_empty = (last_empty + 1) % nums_;
        }
    }
};
}

#endif 