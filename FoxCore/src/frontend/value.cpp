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
// Value implementation (union-backed: 48-byte payload, one live
// member per value selected by `type`)
// ============================================================

Value::Value() : st_(), type(Type::Void) {}

Value::Value(int v) : st_(), type(Type::Int) { st_.i = v; }

Value::Value(double v) : st_(), type(Type::Double) { st_.d = v; }

Value::Value(const std::string& v) : st_(), type(Type::String) {
    new (&st_.s) std::string(v);
}

Value::Value(const char* v) : st_(), type(Type::String) {
    new (&st_.s) std::string(v);
}

Value::Value(const std::vector<Value>& v) : st_(), type(Type::Array) {
    new (&st_.arr) std::vector<Value>(v);
}

Value::Value(const std::vector<uint8_t>& bytes) : st_(), type(Type::Bytes) {
    new (&st_.bytes) std::vector<uint8_t>(bytes);
}

Value::Value(const std::unordered_map<std::string, GcHandle>& dict)
    : st_(), type(Type::Dict) {
    st_.dict = new std::unordered_map<std::string, GcHandle>(dict);
}

Value::Value(const Value& other)
    : st_(), type(other.type), objDictVal(other.objDictVal), coroId(other.coroId) {
    copyStorage(other);
}

Value::Value(Value&& other) noexcept
    : st_(), type(other.type), objDictVal(other.objDictVal), coroId(other.coroId) {
    moveStorage(std::move(other));
    other.type = Type::Void;
}

Value& Value::operator=(const Value& other) {
    if (this == &other) return *this;
    destroyStorage();
    type = other.type;
    objDictVal = other.objDictVal;
    coroId = other.coroId;
    copyStorage(other);
    return *this;
}

Value& Value::operator=(Value&& other) noexcept {
    if (this == &other) return *this;
    destroyStorage();
    type = other.type;
    objDictVal = other.objDictVal;
    coroId = other.coroId;
    moveStorage(std::move(other));
    other.type = Type::Void;
    return *this;
}

Value::~Value() {
    destroyStorage();
}

void Value::destroyStorage() noexcept {
    switch (type) {
    case Type::String: st_.s.~basic_string(); break;
    case Type::Array: st_.arr.~vector(); break;
    case Type::Bytes: st_.bytes.~vector(); break;
    case Type::Dict: delete st_.dict; st_.dict = nullptr; break;
    case Type::Object: st_.className.~basic_string(); break;
    default: break;
    }
}

void Value::copyStorage(const Value& other) {
    switch (type) {
    case Type::Int: st_.i = other.st_.i; break;
    case Type::Double: st_.d = other.st_.d; break;
    case Type::String: new (&st_.s) std::string(other.st_.s); break;
    case Type::Array: new (&st_.arr) std::vector<Value>(other.st_.arr); break;
    case Type::Bytes: new (&st_.bytes) std::vector<uint8_t>(other.st_.bytes); break;
    case Type::Dict: st_.dict = new std::unordered_map<std::string, GcHandle>(*other.st_.dict); break;
    case Type::Object: new (&st_.className) std::string(other.st_.className); break;
    default: break;
    }
}

void Value::moveStorage(Value&& other) noexcept {
    switch (type) {
    case Type::Int: st_.i = other.st_.i; break;
    case Type::Double: st_.d = other.st_.d; break;
    case Type::String: new (&st_.s) std::string(std::move(other.st_.s)); break;
    case Type::Array: new (&st_.arr) std::vector<Value>(std::move(other.st_.arr)); break;
    case Type::Bytes: new (&st_.bytes) std::vector<uint8_t>(std::move(other.st_.bytes)); break;
    case Type::Dict: st_.dict = other.st_.dict; other.st_.dict = nullptr; break;
    case Type::Object: new (&st_.className) std::string(std::move(other.st_.className)); break;
    default: break;
    }
}

std::vector<Value>& Value::asArrayRef() {
    if (type != Type::Array) throw std::runtime_error("Value is not an array type");
    return st_.arr;
}

Value::Type Value::getType() const { return type; }

int Value::asInt() const {
    if (type != Type::Int) throw std::runtime_error("Value is not an integer type");
    return st_.i;
}

double Value::asDouble() const {
    if (type != Type::Double) throw std::runtime_error("Value is not a double type");
    return st_.d;
}

const std::string& Value::asString() const {
    if (type != Type::String) throw std::runtime_error("Value is not a string type");
    return st_.s;
}

const std::vector<Value>& Value::asArray() const {
    if (type != Type::Array) throw std::runtime_error("Value is not an array type");
    return st_.arr;
}

const std::vector<uint8_t>& Value::asBytes() const {
    if (type != Type::Bytes) throw std::runtime_error("Value is not a bytes type");
    return st_.bytes;
}

std::vector<uint8_t>& Value::asBytesRef() {
    if (type != Type::Bytes) throw std::runtime_error("Value is not a bytes type");
    return st_.bytes;
}

const std::unordered_map<std::string, GcHandle>& Value::asDict() const {
    if (type != Type::Dict) throw std::runtime_error("Value is not a dict type");
    return *st_.dict;
}

std::unordered_map<std::string, GcHandle>& Value::asDictRef() {
    if (type != Type::Dict) throw std::runtime_error("Value is not a dict type");
    return *st_.dict;
}

Value Value::makeObject(const std::string& className) {
    Value v;
    v.type = Type::Object;
    new (&v.st_.className) std::string(className);
    v.objDictVal = Gc::instance().alloc(Value(std::unordered_map<std::string, GcHandle>()));
    return v;
}

const std::string& Value::asObjectClass() const {
    if (type != Type::Object) throw std::runtime_error("Value is not an object type");
    return st_.className;
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
        for (const auto& elem : st_.arr) elem.traceGC();
        break;
    case Type::Dict:
        for (const auto& [key, h] : *st_.dict) Gc::instance().mark(h);
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
    case Type::Bytes: return static_cast<int>(st_.bytes.size());
    case Type::Dict: return STRING_BYTE_SIZE;
    case Type::Object: return STRING_BYTE_SIZE;
    case Type::Coroutine: return STRING_BYTE_SIZE;
    case Type::Void: return VOID_BYTE_SIZE;
    default: return 0;
    }
}

std::string Value::toString() const {
    switch (type) {
    case Type::Int: return std::to_string(st_.i);
    case Type::Double: return std::to_string(st_.d);
    case Type::String: return st_.s;
    case Type::Array: return "[array]";
    case Type::Bytes: return "[bytes]";
    case Type::Dict: return "[dict]";
    case Type::Object: return "[class " + st_.className + "]";
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
