#!/bin/bash
#set -x
KB=$((1024))
MB=$(($KB*1024))
GB=$(($MB*1024))
# APP_PREFIX=sudo

db_bench=./build
benchmarks="overwrite,readrandom"
leveldb_path=/tmp/leveldb-wzh
pm_path=/mnt/pmem0.1/pm_test

value_size=1000
num_thread=1
num_kvs=$((10*$MB))
write_buffer_size=$((100*$MB))
max_file_size=$((100*$MB))
pm_size=$((20*$GB))
bucket_nums=$((4*$MB)) # bucket_nums * 4 > nums_kvs
use_pm=1
flush_ssd=0

WRITE10G() {
    leveldb_path=$pm_path;
    value_size=1000
    num_thread=1
    num_kvs=$((10*$MB))
    write_buffer_size=$((100*$MB))
    max_file_size=$((100*$MB))
    pm_size=$((20*$GB))
    bucket_nums=$((4*$MB)) # bucket_nums * 4 > nums_kvs
}
WRITE80G_FLUSHSSD() {
    # leveldb_path=$pm_path;
    value_size=4096
    num_thread=1
    num_kvs=$((20*$MB))
    write_buffer_size=$((100*$MB))
    max_file_size=$((100*$MB))
    pm_size=$((8*$GB))
    bucket_nums=$((8*$MB)) # bucket_nums * 4 > nums_kvs
    flush_ssd=1
}
WRITE100G() {
    leveldb_path=$pm_path;
    value_size=1000
    num_thread=1
    num_kvs=$((100*$MB))
    write_buffer_size=$((1024*$MB))
    max_file_size=$((1024*$MB))
    pm_size=$((200*$GB))
    bucket_nums=$((40*$MB)) # bucket_nums * 4 > nums_kvs
    use_pm=1
}

#NoveLSM specific parameters
#NoveLSM uses memtable levels, always set to num_levels 2
#write_buffer_size DRAM memtable size in MBs
#write_buffer_size_2 specifies NVM memtable size; set it in few GBs for perfomance;
# OTHERPARAMS="--num_levels=2 --write_buffer_size=$DRAMBUFFSZ --nvm_buffer_size=$NVMBUFFSZ"
# NUMREADTHREADS="0"

SETUP() {
  if [ -z "$pm_path" ]
  then
        echo "PM path empty. Run source scripts/setvars.sh from source parent dir"
        exit
  fi
  if [ -z "$leveldb_path" ]
  then
        echo "DB path empty. Run source scripts/setvars.sh from source parent dir"
        exit
  fi
  rm -rf $leveldb_path/*
  rm -rf $pm_path/*
  mkdir -p $pm_path
}

MAKE() {
  cd $db_bench
  #make clean
  make -j32
}

SETUP
MAKE
WRITE10G
cd ..
$APP_PREFIX $db_bench/db_bench --benchmarks=$benchmarks --num=$num_kvs \
--value_size=$value_size --write_buffer_size=$write_buffer_size --max_file_size=$max_file_size \
--pm_size=$pm_size --pm_path=$pm_path --db=$leveldb_path --bucket_nums=$bucket_nums --use_pm=$use_pm --threads=$num_thread --flush_ssd=$flush_ssd
SETUP

#Run all benchmarks
# $APP_PREFIX $DBBENCH/db_bench --threads=$NUMTHREAD --num=$NUMKEYS --value_size=$VALUSESZ \
# $OTHERPARAMS --num_read_threads=$NUMREADTHREADS

