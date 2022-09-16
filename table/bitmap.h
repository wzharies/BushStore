#program once
#include<memory>

namespace leveldb{

class Bitmap{
public:
    Bitmap(int nums){
        int bitSize = (nums >> 3) + 1;
        bitmaps_ = (char *)calloc(bitSize, sizeof(char));
        // cur_empty_ = 0;
    };

    ~Bitmap(){
        free(bitmaps_);
    };

    int set(size_t index){
        if(index > nums_) return 0;
        int charIndex = (index >> 3);
        int innerIndex = (charIndex & 7);
        bitmaps_[charIndex] |= (1 << innerIndex);
    };

    int clr(size_t index){
        if(index > nums_) return 0;
        int charIndex = (index >> 3);
        int innerIndex = (charIndex & 7);
        bitmaps_[charIndex] ^= (1 << innerIndex);
    };

    bool get(size_t index){
        if(index > nums_) return 0;
        int charIndex = (index >> 3);
        int innerIndex = (charIndex & 7);
        return (bitmaps_[charIndex] >> innerIndex) & 1;
    };

    // size_t getEmpty(){
    //     while(!get(cur_empty_)){
    //         cur_empty_ = (cur_empty_ + 1) % nums_;
    //     }
    //     return cur_empty_;
    // }


private:
    size_t nums_;
    char * bitmaps_;
    // size_t cur_empty_;
};

}