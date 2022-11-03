
// #program once
// #include <memory>
// #include <string>
// #include "db/dbformat.h"
// namespace leveldb{
// struct TableFirstIndex{

// };
// struct KeysMetadata{
//     InternalKey key;   //InternalKey 的key
//     int32_t next;   //指向下一个key的index,空为-1,从0开始。
//     uint64_t offset;  //key-value结构的offset
//     uint64_t size;  //key-value结构的大小

//     KeysMetadata(){
//         next = -1;
//         offset = 0;
//         size = 0;
//     }
//     ~KeysMetadata(){}

// };
// }