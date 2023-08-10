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

# Code
This is the source code of BushStore, which is based on LevelDB.