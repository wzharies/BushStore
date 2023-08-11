# BushStore: Efficient B+Tree Group Indexing for LSM-Tree in Hybrid Persistent Storage.

&#160; &#160; &#160; &#160; NVM offers low-latency, non-volatility, and byte-
addressability. This makes it perfectly suited to bridge the
performance gap between DRAM and SSD. A storage system
that combines NVM and SSDs aligns particularly well with
the architecture of the Log-Structured Merge-Tree. While
much of the previous work on LSM-Tree focuses on leveraging
the NVM-SSD hybrid system to mitigate write amplification
and write stall issues, the potential of NVM to boost read
performance has largely been overlooked.

&#160; &#160; &#160; &#160; We propose BushStore, an adaptive and horizontally scal-
able LSM-Tree for NVM-SSD hybrid storage to address the
limitation. BushStore is designed with a three-level architec-
ture, where the lower levels of BushStore contain a group
of non-clustered B+Trees instead of the conventional SSTa-
bles. By storing the non-leaf nodes in the DRAM and the leaf
nodes in NVM, and separating the data pages from the indexes,
these B+Trees are able to produce high performance for both
read and write workloads. We propose four techniques to
enhance the system performance. First, we design novel data
structures to concentrate read/write footprint to small sections
on NVM. Second, we reuse the KV data during flushing and
compaction to expedite writes. Third, we dynamically tune
the size of newly generated B+Trees to orchestrate flushing
and compaction for balanced writing performance. Fourth,
we expedite query processing and compaction via lazy-delete
Cuckoo filtering and lazy-persistent allocation. Results show
that BushStore produces high performance and scalability
under synthetic and real work-loads, and achieves on average
5.4x higher random write throughput and 5.0x higher random
read throughput compared to the state-of-the-art MioDB.

# Important Notes on Tail Latency  (BushStore vs. MioDB) 


MioDB tail latency is good for a variety of reasons, such as one-piece flush, which requires only one whole copy to flush. And skiplist compaction by only rewriting pointers with a lower WA. But it's also not fair to directly compare tail latency with MioDB, which uses NVM by expanding memory and it's hard to guarantee persistence. And we use the PMDK interface, which has performance implications for persistence.

Now we have adjusted our WAL strategy so that our tail latency is finally lower than MioDB. Previously, WAL's persistent granularity was a data page. Too high a persistence granularity can increase throughput, but also increase tail latency. On the contrary, lowering persistent granularity can reduce tail latency. This is a trade-off between throughput and tail latency, and our previous strategy favored high throughput.

So we added an additional WAL strategy where we changed the data page from being persistent once to being persistent multiple times on writes. By losing write throughput by 5.7% in exchange for a 67% reduction in p99.9 and 77% reduction in p99.99, the tail latency is now better than MioDB.

Due to time constraints, there were some performance issues we didn't have time to troubleshoot, so we are clarifying them here. We will update the full results of the new strategy in the paper if we have the opportunity later.

Table: Tail Latency(us)
| KV Stores         | avg.   | p95 | p99  | p99.9 | p99.99  |
|-------------------|--------|-----|------|-------|---------|
| RocksDB           | 18.87  | 26  | 41   | 1097  |  1113   |
| NoveLSM           | 20.582 | 24  | 27   | 60    | 527     |
| SLM-DB            | 33.98  | 18  | 1085 | 1195  | 1225    |
| MatrixKV          | 20.54  | 18  | 199  | 1100  | 1107    |
| MioDB             | 10.21  | 19  | 22   | 32    | 62      |
| BushStore-Old-WAL | 2.915  | 3   | 4    | 46    | 116     |
| BushStore-New-WAL | 3.083  | 10  | 11   | 15    | 26      |



# Code
This is the source code of BushStore, which is based on LevelDB.