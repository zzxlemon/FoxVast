#pragma once
// Precision GC for FoxCore heap objects.
//
// Only two kinds of object live on the traced heap:
//   - dict element handles (std::unordered_map<std::string, GcHandle>)
//   - object member tables  (GcHandle pointing at a Dict-typed Value)
// Strings/arrays/bytes are stored inline in Value, so they need no tracking.
//
// Handles are 32-bit: 24-bit slot index | 8-bit generation. A handle whose
// generation no longer matches the slot is stale and deref() returns null,
// which keeps reuse after collection safe.
//
// Roots are registered by the VM / interpreter via addRoot(); in addition a
// conservative scan of the native stack protects temporary C++ Value locals
// that hold handles between root sets. Mark-sweep: no compaction, no cycles
// left behind (unlike the previous shared_ptr bookkeeping).

#include <cstdint>
#include <functional>
#include <vector>

class Value;

using GcHandle = uint32_t;
constexpr GcHandle kNoGcHandle = 0;

class Gc {
public:
    GcHandle alloc(const Value& v);
    Value* deref(GcHandle h);
    const Value* deref(GcHandle h) const;
    bool isValid(GcHandle h) const;

    // Register a root set provider. Runs at every collection before marking
    // the stack; the callback must mark every reachable handle (via mark()).
    using RootFn = std::function<void(Gc&)>;
    int addRoot(RootFn fn);
    void removeRoot(int id);

    // Collection scheduling: call after every heap allocation (cheap; only
    // increments counters). The collector itself runs at explicit safe
    // points (VM instruction loop / interpreter statement loop), where no
    // mid-expression temporaries are live.
    void maybeCollect();
    void collect();
    void checkpoint() {
        if (pending_) {
            pending_ = false;
            collect();
        }
    }

    // A new object: pick a free slot (bumping its generation) or grow.
    // Internal: exposed for mark helpers.
    void mark(GcHandle h);
    void markValue(const Value& v);

    size_t liveObjects() const { return liveCount_; }
    size_t totalSlots() const { return slots_.size(); }
    void setThreshold(size_t allocs) { allocThreshold_ = allocs; }

    static Gc& instance();

private:
    struct Slot {
        Value* value;
        uint32_t gen;   // 8-bit generation (wraps after 256 reuses)
        bool marked;
        bool nursery;   // newly allocated: survives the next collection
    };

    GcHandle makeHandle(uint32_t idx, uint32_t gen) const {
        return static_cast<GcHandle>((idx << 8) | (gen & 0xFF));
    }
    uint32_t handleIndex(GcHandle h) const { return h >> 8; }
    uint32_t handleGen(GcHandle h) const { return h & 0xFF; }

    void markFromRoots();
    void sweep();

    std::vector<Slot> slots_;
    std::vector<uint32_t> freeSlots_;
    std::vector<RootFn> roots_;
    std::vector<int> rootIds_;
    int nextRootId_ = 1;
    size_t allocsSinceCollect_ = 0;
    size_t allocThreshold_ = 8192;
    size_t liveCount_ = 0;
    bool collecting_ = false;
    bool pending_ = false;
};