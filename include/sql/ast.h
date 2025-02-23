#pragma once

#include <ostream>
#include <variant>

#include <fe/cast.h>

#include "sql/tok.h"

namespace sql {

template<class T> using AST  = fe::Arena::Ptr<const T>;
template<class T> using ASTs = std::deque<AST<T>>;
using Syms                   = std::deque<Sym>;

/// Base class for all @p Expr%essions.
class Node : public fe::RuntimeCast<Node> {
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
 * Type
 */

class Type : public Node {
public:
    Type(Loc loc, bool not_null)
        : Node(loc)
        , not_null_(not_null) {}

    bool not_null() const { return not_null_; }

private:
    bool not_null_;
};

class SimpleType : public Type {
public:
    SimpleType(Loc loc, Tok::Tag tag, bool not_null)
        : Type(loc, not_null)
        , tag_(tag) {}

    Tok::Tag tag() const { return tag_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
};

/*
 * Expr (<value expression>)
 */

/// Base class for all @p Expr%essions.
class Expr : public Node {
public:
    Expr(Loc loc)
        : Node(loc) {}
};

class ParenExprList : public Expr {
public:
    ParenExprList(Loc loc, ASTs<Expr>&& args)
        : Expr(loc)
        , args_(std::move(args)) {}

    const auto& args() const { return args_; }

    std::ostream& stream(std::ostream&) const override;

private:
    ASTs<Expr> args_;
};

class Id : public Expr {
public:
    Id(Loc loc, Syms&& syms, bool asterisk)
        : Expr(loc)
        , syms_(syms)
        , asterisk_(asterisk) {}

    const auto& syms() const { return syms_; }
    bool asterisk() const { return asterisk_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
    bool asterisk_ = false;

public:
    mutable bool asterisk_allowed_ = false;
};

class UnExpr : public Expr {
public:
    UnExpr(Loc loc, Tok::Tag tag, AST<Expr>&& rhs)
        : Expr(loc)
        , tag_(tag)
        , rhs_(std::move(rhs)) {}

    Tok::Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    AST<Expr> rhs_;
};

class Func : public Expr {
public:
    Func(Loc loc, Tok::Tag tag, ASTs<Expr>&& args)
        : Expr(loc)
        , tag_(tag)
        , args_(std::move(args)) {}

    Tok::Tag tag() const { return tag_; }
    const auto& args() const { return args_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    ASTs<Expr> args_;
};

class BinExpr : public Expr {
public:
    BinExpr(Loc loc, AST<Expr>&& lhs, Tok::Tag tag, AST<Expr>&& rhs)
        : Expr(loc)
        , lhs_(std::move(lhs))
        , tag_(tag)
        , rhs_(std::move(rhs)) {}

    const Expr* lhs() const { return lhs_.get(); }
    Tok::Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> lhs_;
    Tok::Tag tag_;
    AST<Expr> rhs_;
};

class BinExprWithPreTag : public BinExpr {
public:
    BinExprWithPreTag(Loc loc, AST<Expr>&& lhs, Tok::Tag pretag, Tok::Tag tag, AST<Expr>&& rhs)
        : BinExpr(loc, std::move(lhs), tag, std::move(rhs))
        , pretag_(pretag) {}
    Tok::Tag pretag() const { return pretag_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag pretag_;
};

class Val : public Expr {
public:
    Val(Loc loc)
        : Expr(loc) {}
};

class IntVal : public Val {
public:
    IntVal(Loc loc, uint64_t u64)
        : Val(loc)
        , u64_(u64) {}

    uint64_t u64() const { return u64_; }

    std::ostream& stream(std::ostream&) const override;

private:
    uint64_t u64_;
};

/// `TRUE`, `FALSE`, `UNKNOWN`, or `NULL`
class SimpleVal : public Val {
public:
    SimpleVal(Loc loc, Tok::Tag tag)
        : Val(loc)
        , tag_(tag) {}

    Tok::Tag tag() const { return tag_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
};

class Create : public Expr {
public:
    class Elem : public Node {
    public:
        Elem(Loc loc, Sym sym, AST<Type>&& type)
            : Node(loc)
            , sym_(sym)
            , type_(std::move(type)) {}

        Sym sym() const { return sym_; }
        const Type* type() const { return type_.get(); }

        std::ostream& stream(std::ostream&) const override;

    private:
        Sym sym_;
        AST<Type> type_;
    };

    Create(Loc loc, Sym sym, ASTs<Elem>&& elems)
        : Expr(loc)
        , sym_(sym)
        , elems_(std::move(elems)) {}

    Sym sym() const { return sym_; }
    const auto& elems() const { return elems_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    ASTs<Elem> elems_;
};

class Join : public Expr {
public:
    using On    = AST<Expr>;
    using Using = Syms;
    using Spec  = std::variant<std::monostate, On, Using>;

    enum Tag {
        Inner         = 0x0,
        Left          = 0x1,          // Outer
        Right         = 0x2,          // Outer
        Full          = Left | Right, // Outer
        Natural       = 0x4,
        Natural_Inner = Natural,
        Natural_Left  = Natural | Left,
        Natural_Right = Natural | Right,
        Natural_Full  = Natural | Full,
        Cross,
    };

    Join(Loc loc, AST<Expr>&& lhs, Tag tag, AST<Expr>&& rhs, Spec&& spec)
        : Expr(loc)
        , lhs_(std::move(lhs))
        , tag_(tag)
        , rhs_(std::move(rhs))
        , spec_(std::move(spec)) {}

    const Expr* lhs() const { return lhs_.get(); }
    Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }
    const auto& spec() const { return spec_; }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> lhs_;
    Tag tag_;
    AST<Expr> rhs_;
    Spec spec_;
};

class Select : public Expr {
public:
    class Elem : public Node {
    public:
        Elem(Loc loc, AST<Expr>&& expr, Syms&& syms)
            : Node(loc)
            , expr_(std::move(expr))
            , syms_(std::move(syms)) {}

        const Expr* expr() const { return expr_.get(); }
        const auto& syms() const { return syms_; }

        std::ostream& stream(std::ostream&) const override;

    private:
        AST<Expr> expr_;
        Syms syms_;
    };

    class From : public Node {
    public:
        From(Loc loc, Sym from, Sym as)
            : Node(loc)
            , from_(from)
            , as_(as) {}

        Sym from() const { return from_; }
        Sym as() const { return as_; }
        std::ostream& stream(std::ostream&) const override;

    private:
        Sym from_;
        Sym as_;
    };

    Select(Loc loc,
           bool all,
           ASTs<Elem>&& elems,
           ASTs<From>&& froms,
           AST<Expr>&& where,
           AST<Expr>&& group,
           AST<Expr>&& having)
        : Expr(loc)
        , all_(all)
        , elems_(std::move(elems))
        , froms_(std::move(froms))
        , where_(std::move(where))
        , group_(std::move(group))
        , having_(std::move(having)) {}

    bool all() const { return all_; }
    bool distinct() const { return !all_; }
    const auto& elems() const { return elems_; }
    const auto& froms() const { return froms_; }
    const Expr* where() const { return where_.get(); }
    const Expr* group() const { return group_.get(); }
    const Expr* having() const { return having_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    bool all_;
    ASTs<Elem> elems_;
    ASTs<From> froms_;
    AST<Expr> where_;
    AST<Expr> group_;
    AST<Expr> having_;
};

/// Just a dummy that does nothing and will only be constructed during parse errors.
class ErrExpr : public Expr {
public:
    ErrExpr(Loc loc)
        : Expr(loc) {}

    std::ostream& stream(std::ostream&) const override;
};

/*
 * Prog
 */

/// Just a HACK to have a list of Stmt%s.
class Prog : public Node {
public:
    Prog(Loc loc, ASTs<Expr>&& exprs)
        : Node(loc)
        , exprs_(std::move(exprs)) {}

    const auto& exprs() const { return exprs_; }

    std::ostream& stream(std::ostream&) const override;

private:
    ASTs<Expr> exprs_;
};

} // namespace sql
