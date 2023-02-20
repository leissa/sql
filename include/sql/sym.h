#pragma once

#include <string>

#include <absl/container/node_hash_set.h>
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

namespace sql {

class Sym {
public:
    Sym() = default;
    Sym(const std::string* ptr)
        : ptr_(ptr) {}

    bool operator==(Sym other) const { return this->ptr_ == other.ptr_; }
    bool operator!=(Sym other) const { return this->ptr_ != other.ptr_; }
    operator const std::string_view() const { return *ptr_; }
    operator const std::string&() const { return *ptr_; }
    explicit operator bool() const { return ptr_; }
    const std::string& operator*() const { return *ptr_; }
    const std::string& operator->() const { return *ptr_; }

private:
    const std::string* ptr_ = nullptr;
};

struct SymHash {
    size_t operator()(Sym sym) const { return absl::Hash<std::string>()(*sym); }
};

std::ostream& operator<<(std::ostream&, Sym);

class Syms {
public:
    Syms() = default;
    Syms(const Syms&) = delete;
    Syms(Syms&& other)
        : pool_(std::move(other.pool_)) {}

    Sym sym(std::string_view s) { return &*pool_.emplace(s).first; }
    Sym sym(const char* s) { return &*pool_.emplace(s).first; }
    Sym sym(std::string&& s) { return &*pool_.emplace(std::move(s)).first; }

    friend void swap(Syms& p1, Syms& p2) {
        using std::swap;
        swap(p1.pool_, p2.pool_);
    }

private:
    absl::node_hash_set<std::string> pool_;
};

template<class V>
using SymMap = absl::flat_hash_map<Sym, V, SymHash>;
using SymSet = absl::flat_hash_set<Sym, SymHash>;

}
