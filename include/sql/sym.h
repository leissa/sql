#pragma once

#include <string>

#include <absl/container/node_hash_set.h>

namespace sql {

class Sym {
public:
    Sym() = default;
    Sym(const std::string* ptr)
        : ptr_(ptr) {}

    bool is_anonymous() const { return ptr_ && *ptr_ == "_"; }
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

std::ostream& operator<<(std::ostream&, Sym);

class SymTab {
public:
    SymTab()              = default;
    SymTab(const SymTab&) = delete;
    SymTab(SymTab&& other)
        : pool_(std::move(other.pool_)) {}

    Sym add(std::string_view s) { return &*pool_.emplace(s).first; }
    Sym add(const char* s) { return &*pool_.emplace(s).first; }
    Sym add(std::string&& s) { return &*pool_.emplace(std::move(s)).first; }

    friend void swap(SymTab& p1, SymTab& p2) {
        using std::swap;
        swap(p1.pool_, p2.pool_);
    }

private:
    absl::node_hash_set<std::string> pool_;
};

}
