#include "value.hpp"
#include "../util/common.hpp"

// ============================================================
// Gc engine
// ============================================================
#if defined(_WIN32)
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#endif
#include <cstdlib>
#include <algorithm>
#include <cstddef>

Gc& Gc::instance() {
    static Gc g;
    return g;
}

int Gc::addRoot(RootFn fn) {
    roots_.push_back(std::move(fn));
    rootIds_.push_back(nextRootId_);
    return nextRootId_++;
}

void Gc::removeRoot(int id) {
    for (size_t i = 0; i < roots_.size(); i++) {
        if (rootIds_[i] == id) {
            roots_.erase(roots_.begin() + static_cast<ptrdiff_t>(i));
            rootIds_.erase(rootIds_.begin() + static_cast<ptrdiff_t>(i));
            return;
        }
    }
}

bool Gc::isValid(GcHandle h) const {
    if (h == kNoGcHandle) return false;
    uint32_t idx = handleIndex(h);
    if (idx >= slots_.size()) return false;
    const Slot& s = slots_[idx];
    if (!s.value) return false;
    return s.gen == handleGen(h);
}

Value* Gc::deref(GcHandle h) {
    if (h == kNoGcHandle) return nullptr;
    uint32_t idx = handleIndex(h);
    if (idx >= slots_.size()) return nullptr;
    Slot& s = slots_[idx];
    if (!s.value || s.gen != handleGen(h)) return nullptr;
    return s.value;
}

const Value* Gc::deref(GcHandle h) const {
    if (h == kNoGcHandle) return nullptr;
    uint32_t idx = handleIndex(h);
    if (idx >= slots_.size()) return nullptr;
    const Slot& s = slots_[idx];
    if (!s.value || s.gen != handleGen(h)) return nullptr;
    return s.value;
}

GcHandle Gc::alloc(const Value& v) {
    if (!freeSlots_.empty()) {
        uint32_t idx = freeSlots_.back();
        freeSlots_.pop_back();
        Slot& s = slots_[idx];
        // Bump generation so stale handles no longer alias this slot.
        s.gen = (s.gen + 1) & 0xFF;
        if (s.gen == 0) s.gen = 1;
        s.value = new Value(v);
        s.marked = false;
        s.nursery = true;
        liveCount_++;
        allocsSinceCollect_++;
        maybeCollect();
        return makeHandle(idx, s.gen);
    }
    uint32_t idx = static_cast<uint32_t>(slots_.size());
    slots_.push_back(Slot{ new Value(v), 1, false, true });
    liveCount_++;
    allocsSinceCollect_++;
    maybeCollect();
    return makeHandle(idx, 1);
}

void Gc::mark(GcHandle h) {
    if (h == kNoGcHandle) return;
    uint32_t idx = handleIndex(h);
    if (idx >= slots_.size()) return;
    Slot& s = slots_[idx];
    if (!s.value || s.gen != handleGen(h)) return;
    if (s.marked) return;
    s.marked = true;
    s.nursery = false; // formally reachable: no longer new-born
    s.value->traceGC();
}

void Gc::markValue(const Value& v) {
    v.traceGC();
}

void Gc::maybeCollect() {
    if (collecting_) return;
    if (allocsSinceCollect_ >= allocThreshold_) {
        pending_ = true;
    }
}

void Gc::markFromRoots() {
    for (auto& fn : roots_) {
        fn(*this);
    }
    // Conservative native-stack scan: any aligned word in the VM's stack
    // region that looks like a valid handle keeps its object alive. This
    // protects C++ temporaries (pop() results, args vectors, ...) that are
    // not part of any root set.
#if defined(_WIN32)
    {
    ULONG_PTR high = __readgsqword(0x08);
    ULONG_PTR low = __readgsqword(0x10);
    if (high > low && low != 0) {
        uintptr_t sp;
#if defined(__GNUC__)
        __asm__("mov %%rsp, %0" : "=r"(sp));
#else
        sp = reinterpret_cast<uintptr_t>(_AddressOfReturnAddress());
#endif
        uintptr_t end = high - 0x200;
        for (uintptr_t p = sp & ~(sizeof(uintptr_t) - 1);
             p + sizeof(uintptr_t) <= end; p += sizeof(uintptr_t)) {
            uintptr_t w = *reinterpret_cast<uintptr_t*>(p);
            if (w != 0 && w != kNoGcHandle && handleIndex(static_cast<uint32_t>(w)) < slots_.size()) {
                mark(static_cast<GcHandle>(w & 0xFFFFFFFFu));
            }
            uint32_t highWord = static_cast<uint32_t>(w >> 32);
            if (highWord != 0 && highWord != kNoGcHandle && handleIndex(highWord) < slots_.size()) {
                mark(highWord);
            }
        }
    }
    }
#endif
}

void Gc::sweep() {
    for (size_t i = 0; i < slots_.size(); i++) {
        Slot& s = slots_[i];
        if (!s.value) continue;
        if (!s.marked && !s.nursery) {
            delete s.value;
            s.value = nullptr;
            s.marked = false;
            freeSlots_.push_back(static_cast<uint32_t>(i));
            liveCount_--;
        } else {
            // New-born objects that were not reached keep their slot for one
            // more cycle (they may sit in a register between alloc() and
            // being handed to a root); next collection they become collectible.
            s.marked = false;
            s.nursery = false;
        }
    }
}

void Gc::collect() {
    if (collecting_) return;
    collecting_ = true;
    allocsSinceCollect_ = 0;
    pending_ = false;
    for (auto& s : slots_) {
        if (s.value) s.marked = false;
    }
    markFromRoots();
    sweep();
    collecting_ = false;
}

// ============================================================
// Value implementation
// ============================================================

Value::Value() : type(Type::Void), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal(), dictVal() {}
Value::Value(int v) : type(Type::Int), intVal(v), doubleVal(0.0), strVal(""), arrVal(), bytesVal(), dictVal() {}
Value::Value(double v) : type(Type::Double), intVal(0), doubleVal(v), strVal(""), arrVal(), bytesVal(), dictVal() {}
Value::Value(const std::string& v) : type(Type::String), intVal(0), doubleVal(0.0), strVal(v), arrVal(), bytesVal(), dictVal() {}
Value::Value(const char* v) : type(Type::String), intVal(0), doubleVal(0.0), strVal(v), arrVal(), bytesVal(), dictVal() {}
Value::Value(const std::vector<Value>& v) : type(Type::Array), intVal(0), doubleVal(0.0), strVal(""), arrVal(v), bytesVal(), dictVal() {}
Value::Value(const std::vector<uint8_t>& bytes) : type(Type::Bytes), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal(bytes), dictVal() {}
Value::Value(const std::unordered_map<std::string, GcHandle>& dict)
    : type(Type::Dict), intVal(0), doubleVal(0.0), strVal(""), arrVal(), bytesVal(), dictVal(dict) {}

std::vector<Value>& Value::asArrayRef() {
    if (type != Type::Array) throw std::runtime_error("Value is not an array type");
    return arrVal;
}

Value::Type Value::getType() const { return type; }

int Value::asInt() const {
    if (type != Type::Int) throw std::runtime_error("Value is not an integer type");
    return intVal;
}

double Value::asDouble() const {
    if (type != Type::Double) throw std::runtime_error("Value is not a double type");
    return doubleVal;
}

const std::string& Value::asString() const {
    if (type != Type::String) throw std::runtime_error("Value is not a string type");
    return strVal;
}

const std::vector<Value>& Value::asArray() const {
    if (type != Type::Array) throw std::runtime_error("Value is not an array type");
    return arrVal;
}

const std::vector<uint8_t>& Value::asBytes() const {
    if (type != Type::Bytes) throw std::runtime_error("Value is not a bytes type");
    return bytesVal;
}

std::vector<uint8_t>& Value::asBytesRef() {
    if (type != Type::Bytes) throw std::runtime_error("Value is not a bytes type");
    return bytesVal;
}

const std::unordered_map<std::string, GcHandle>& Value::asDict() const {
    if (type != Type::Dict) throw std::runtime_error("Value is not a dict type");
    return dictVal;
}

std::unordered_map<std::string, GcHandle>& Value::asDictRef() {
    if (type != Type::Dict) throw std::runtime_error("Value is not a dict type");
    return dictVal;
}

Value Value::makeObject(const std::string& className) {
    Value v;
    v.type = Type::Object;
    v.objectClassName = className;
    v.objDictVal = Gc::instance().alloc(Value(std::unordered_map<std::string, GcHandle>()));
    return v;
}

const std::string& Value::asObjectClass() const {
    if (type != Type::Object) throw std::runtime_error("Value is not an object type");
    return objectClassName;
}

GcHandle Value::asObjectDictHandle() const {
    if (type != Type::Object) throw std::runtime_error("Value is not an object type");
    return objDictVal;
}

const std::unordered_map<std::string, GcHandle>& Value::asObjectDict() const {
    if (type != Type::Object || objDictVal == kNoGcHandle) throw std::runtime_error("Value is not an object type");
    const Value* tbl = Gc::instance().deref(objDictVal);
    if (!tbl || tbl->getType() != Type::Dict) throw std::runtime_error("corrupt object member table");
    return tbl->asDict();
}

std::unordered_map<std::string, GcHandle>& Value::asObjectDictRef() {
    if (type != Type::Object || objDictVal == kNoGcHandle) throw std::runtime_error("Value is not an object type");
    Value* tbl = Gc::instance().deref(objDictVal);
    if (!tbl || tbl->getType() != Type::Dict) throw std::runtime_error("corrupt object member table");
    return tbl->asDictRef();
}

void Value::traceGC() const {
    switch (type) {
    case Type::Array:
        for (const auto& elem : arrVal) elem.traceGC();
        break;
    case Type::Dict:
        for (const auto& [key, h] : dictVal) Gc::instance().mark(h);
        break;
    case Type::Object:
        if (objDictVal != kNoGcHandle) Gc::instance().mark(objDictVal);
        break;
    default:
        break;
    }
}

int Value::getByteSize() const {
    switch (type) {
    case Type::Int: return INT_BYTE_SIZE;
    case Type::Double: return DOUBLE_BYTE_SIZE;
    case Type::String: return STRING_BYTE_SIZE;
    case Type::Array: return STRING_BYTE_SIZE;
    case Type::Bytes: return static_cast<int>(bytesVal.size());
    case Type::Dict: return STRING_BYTE_SIZE;
    case Type::Object: return STRING_BYTE_SIZE;
    case Type::Coroutine: return STRING_BYTE_SIZE;
    case Type::Void: return VOID_BYTE_SIZE;
    default: return 0;
    }
}

std::string Value::toString() const {
    switch (type) {
    case Type::Int: return std::to_string(intVal);
    case Type::Double: return std::to_string(doubleVal);
    case Type::String: return strVal;
    case Type::Array: return "[array]";
    case Type::Bytes: return "[bytes]";
    case Type::Dict: return "[dict]";
    case Type::Object: return "[class " + objectClassName + "]";
    case Type::Coroutine: return "[coroutine " + std::to_string(coroId) + "]";
    case Type::Void: return "";
    default: return "<?>";
    }
}

bool valuesEqual(const Value& a, const Value& b) {
    if (a.getType() != b.getType()) return false;
    switch (a.getType()) {
    case Value::Type::Int: return a.asInt() == b.asInt();
    case Value::Type::Double: return a.asDouble() == b.asDouble();
    case Value::Type::String: return a.asString() == b.asString();
    case Value::Type::Array: {
        const auto& aa = a.asArray();
        const auto& bb = b.asArray();
        if (aa.size() != bb.size()) return false;
        for (size_t i = 0; i < aa.size(); i++) {
            if (!valuesEqual(aa[i], bb[i])) return false;
        }
        return true;
    }
    case Value::Type::Dict: {
        const auto& ad = a.asDict();
        const auto& bd = b.asDict();
        if (ad.size() != bd.size()) return false;
        for (const auto& [key, h] : ad) {
            auto it = bd.find(key);
            if (it == bd.end()) return false;
            const Value* va = Gc::instance().deref(h);
            const Value* vb = Gc::instance().deref(it->second);
            if (!va || !vb || !valuesEqual(*va, *vb)) return false;
        }
        return true;
    }
    case Value::Type::Bytes: return a.asBytes() == b.asBytes();
    case Value::Type::Coroutine: return a.asCoroutineId() == b.asCoroutineId();
    case Value::Type::Object: {
        if (a.asObjectClass() != b.asObjectClass()) return false;
        const auto& ad = a.asObjectDict();
        const auto& bd = b.asObjectDict();
        if (ad.size() != bd.size()) return false;
        for (const auto& [key, h] : ad) {
            auto it = bd.find(key);
            if (it == bd.end()) return false;
            const Value* va = Gc::instance().deref(h);
            const Value* vb = Gc::instance().deref(it->second);
            if (!va || !vb || !valuesEqual(*va, *vb)) return false;
        }
        return true;
    }
    default: return false;
    }
}
