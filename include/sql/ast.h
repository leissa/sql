#pragma once

#include <memory>
#include <ostream>
#include <unordered_set>

#include "sql/loc.h"
#include "sql/sym.h"
#include "sql/tok.h"

namespace sql {

template<class T>
using Ptr = std::unique_ptr<const T>;

template<class T, class... Args>
Ptr<T> mk(Args&&... args) {
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/// Base class for all @p Expr%essions.
class Node {
public:
    Node(Loc loc)
        : loc_(loc) {}
    virtual ~Node() {}

    Loc loc() const { return loc_; }
    void dump() const;

    /// Stream to @p o.
    virtual std::ostream& stream(std::ostream& o) const = 0;

private:
    Loc loc_;
};

/*
 * Expr
 */

/// Base class for all @p Expr%essions.
class Expr : public Node {
public:
    Expr(Loc loc)
        : Node(loc) {}
};

class IdExpr : public Expr {
public:
    IdExpr(Loc loc, Sym sym)
        : Expr(loc)
        , sym_(sym) {}

    Sym sym() const { return sym_; }

    std::ostream& stream(std::ostream& o) const override;

private:
    Sym sym_;
};

class LitExpr : public Expr {
public:
    LitExpr(Loc loc, uint64_t u64)
        : Expr(loc)
        , u64_(u64) {}

    uint64_t u64() const { return u64_; }

    std::ostream& stream(std::ostream& o) const override;

private:
    uint64_t u64_;
};

class UnExpr : public Expr {
public:
    UnExpr(Loc loc, Tok::Tag tag, Ptr<Expr>&& rhs)
        : Expr(loc)
        , tag_(tag)
        , rhs_(std::move(rhs)) {}

    Tok::Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }

    std::ostream& stream(std::ostream& o) const override;

private:
    Tok::Tag tag_;
    Ptr<Expr> rhs_;
};

class BinExpr : public Expr {
public:
    BinExpr(Loc loc, Ptr<Expr>&& lhs, Tok::Tag tag, Ptr<Expr>&& rhs)
        : Expr(loc)
        , lhs_(std::move(lhs))
        , tag_(tag)
        , rhs_(std::move(rhs)) {}

    const Expr* lhs() const { return rhs_.get(); }
    Tok::Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }

    std::ostream& stream(std::ostream& o) const override;

private:
    Ptr<Expr> lhs_;
    Tok::Tag tag_;
    Ptr<Expr> rhs_;
};

/// The @p Err%or @p Expr%ression is a dummy that does nothing and will only be constructed during parse errors.
class ErrExpr : public Expr {
public:
    ErrExpr(Loc loc)
        : Expr(loc) {}

    std::ostream& stream(std::ostream& o) const override;
};

/*
 * Stmt
 */

/// Base class for all @p Stmt%s
class Stmt : public Node {
public:
    Stmt(Loc loc)
        : Node(loc) {}
};

class SelectStmt : public Stmt {
public:
    SelectStmt(Loc loc, bool all, Ptr<Expr>&& select, Ptr<Expr>&& from, Ptr<Expr>&& where, Ptr<Expr>&& group)
        : Stmt(loc)
        , all_(all)
        , select_(std::move(select))
        , from_(std::move(from))
        , where_(std::move(where))
        , group_(std::move(group)) {}

    bool all() const { return all_; }
    bool distinct() const { return !all_; }
    const Expr* select() const { return select_.get(); }
    const Expr* from() const { return from_.get(); }
    const Expr* where() const { return where_.get(); }
    const Expr* group() const { return group_.get(); }

    std::ostream& stream(std::ostream& o) const override;

private:
    bool all_;
    Ptr<Expr> select_;
    Ptr<Expr> from_;
    Ptr<Expr> where_;
    Ptr<Expr> group_;
};

} // namespace sql
