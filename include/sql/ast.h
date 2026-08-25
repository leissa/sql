#pragma once

#include <ostream>
#include <variant>

#include <fe/cast.h>

#include "sql/tok.h"

namespace sql {

class Expr;

template<class T>
using AST = fe::Arena::Ptr<const T>;
template<class T>
using ASTs = std::deque<AST<T>>;
using Syms = std::deque<Sym>;

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

/// A type named by a reserved word - optionally `VARYING` and/or with parenthesized arguments:
/// `INTEGER`, `CHARACTER VARYING(12)`, `NUMERIC(10, 2)`.
class SimpleType : public Type {
public:
    SimpleType(Loc loc, Tok::Tag tag, bool varying, ASTs<Expr>&& args, bool not_null)
        : Type(loc, not_null)
        , tag_(tag)
        , varying_(varying)
        , args_(std::move(args)) {}

    Tok::Tag tag() const { return tag_; }
    bool varying() const { return varying_; }
    const auto& args() const { return args_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    bool varying_;
    ASTs<Expr> args_;
};

/// A type named by an identifier, i.e. anything not a reserved word: `text`, `jsonb`, `uuid`, ...
class NamedType : public Type {
public:
    NamedType(Loc loc, Sym sym, ASTs<Expr>&& args, bool not_null)
        : Type(loc, not_null)
        , sym_(sym)
        , args_(std::move(args)) {}

    Sym sym() const { return sym_; }
    const auto& args() const { return args_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    ASTs<Expr> args_;
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

/*
 * Constraint
 */

/// A column- or table-level constraint of a Create statement.
/// One node covers all flavors: which of Constraint::cols, Constraint::table, Constraint::ref_cols,
/// and Constraint::expr carry meaning depends on Constraint::tag.
class Constraint : public Node {
public:
    enum Tag {
        Primary_Key, ///< `PRIMARY KEY [(cols)]`
        Unique,      ///< `UNIQUE [(cols)]`
        Check,       ///< `CHECK (expr)`
        Default,     ///< `DEFAULT expr`
        References,  ///< `REFERENCES table [(ref_cols)]` - column-level
        Foreign_Key, ///< `FOREIGN KEY (cols) REFERENCES table [(ref_cols)]` - table-level
    };

    Constraint(Loc loc, Sym name, Tag tag, Syms&& cols, Sym table, Syms&& ref_cols, AST<Expr>&& expr)
        : Node(loc)
        , name_(name)
        , tag_(tag)
        , cols_(std::move(cols))
        , table_(table)
        , ref_cols_(std::move(ref_cols))
        , expr_(std::move(expr)) {}

    Sym name() const { return name_; } ///< From a leading `CONSTRAINT <name>`; may be empty.
    Tag tag() const { return tag_; }
    const auto& cols() const { return cols_; }
    Sym table() const { return table_; }
    const auto& ref_cols() const { return ref_cols_; }
    const Expr* expr() const { return expr_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym name_;
    Tag tag_;
    Syms cols_;
    Sym table_;
    Syms ref_cols_;
    AST<Expr> expr_;
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

/// Any `name(args)` call - an aggregate like `COUNT(DISTINCT x)` just as much as a scalar function.
class Func : public Expr {
public:
    Func(Loc loc, Sym sym, bool distinct, ASTs<Expr>&& args)
        : Expr(loc)
        , sym_(sym)
        , distinct_(distinct)
        , args_(std::move(args)) {}

    Sym sym() const { return sym_; }
    bool distinct() const { return distinct_; }
    const auto& args() const { return args_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    bool distinct_;
    ASTs<Expr> args_;
};

/// `expr [NOT] BETWEEN lo AND hi`
class Between : public Expr {
public:
    Between(Loc loc, AST<Expr>&& expr, AST<Expr>&& lo, AST<Expr>&& hi, bool negated)
        : Expr(loc)
        , expr_(std::move(expr))
        , lo_(std::move(lo))
        , hi_(std::move(hi))
        , negated_(negated) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* lo() const { return lo_.get(); }
    const Expr* hi() const { return hi_.get(); }
    bool negated() const { return negated_; }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> lo_;
    AST<Expr> hi_;
    bool negated_;
};

/// `CAST(expr AS type)`
class Cast : public Expr {
public:
    Cast(Loc loc, AST<Expr>&& expr, AST<Type>&& type)
        : Expr(loc)
        , expr_(std::move(expr))
        , type_(std::move(type)) {}

    const Expr* expr() const { return expr_.get(); }
    const Type* type() const { return type_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Type> type_;
};

/// `CASE [operand] WHEN ... THEN ... [ELSE ...] END` - CaseExpr::operand is null for the *searched* form.
class CaseExpr : public Expr {
public:
    class When : public Node {
    public:
        When(Loc loc, AST<Expr>&& cond, AST<Expr>&& then)
            : Node(loc)
            , cond_(std::move(cond))
            , then_(std::move(then)) {}

        const Expr* cond() const { return cond_.get(); }
        const Expr* then() const { return then_.get(); }

        std::ostream& stream(std::ostream&) const override;

    private:
        AST<Expr> cond_;
        AST<Expr> then_;
    };

    CaseExpr(Loc loc, AST<Expr>&& operand, ASTs<When>&& whens, AST<Expr>&& elze)
        : Expr(loc)
        , operand_(std::move(operand))
        , whens_(std::move(whens))
        , elze_(std::move(elze)) {}

    const Expr* operand() const { return operand_.get(); }
    const auto& whens() const { return whens_; }
    const Expr* elze() const { return elze_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> operand_;
    ASTs<When> whens_;
    AST<Expr> elze_;
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

/// A single-quoted string literal; Sym holds the *unquoted* content.
class StrVal : public Val {
public:
    StrVal(Loc loc, Sym sym)
        : Val(loc)
        , sym_(sym) {}

    Sym sym() const { return sym_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
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
    /// One column definition: `<name> <type> <constraints>`.
    class Elem : public Node {
    public:
        Elem(Loc loc, Sym sym, AST<Type>&& type, ASTs<Constraint>&& constraints)
            : Node(loc)
            , sym_(sym)
            , type_(std::move(type))
            , constraints_(std::move(constraints)) {}

        Sym sym() const { return sym_; }
        const Type* type() const { return type_.get(); }
        const auto& constraints() const { return constraints_; }

        std::ostream& stream(std::ostream&) const override;

    private:
        Sym sym_;
        AST<Type> type_;
        ASTs<Constraint> constraints_;
    };

    Create(Loc loc, Sym sym, ASTs<Elem>&& elems, ASTs<Constraint>&& constraints)
        : Expr(loc)
        , sym_(sym)
        , elems_(std::move(elems))
        , constraints_(std::move(constraints)) {}

    Sym sym() const { return sym_; }
    const auto& elems() const { return elems_; }
    const auto& constraints() const { return constraints_; } ///< Table-level constraints.

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    ASTs<Elem> elems_;
    ASTs<Constraint> constraints_;
};

/// `DROP TABLE <name>`
class Drop : public Expr {
public:
    Drop(Loc loc, Sym sym)
        : Expr(loc)
        , sym_(sym) {}

    Sym sym() const { return sym_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
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

    /// One entry of the `FROM` list: a table reference - a name, a Join, or a parenthesized
    /// subquery - plus its optional correlation name and column aliases.
    class From : public Node {
    public:
        From(Loc loc, AST<Expr>&& expr, Sym as, Syms&& cols)
            : Node(loc)
            , expr_(std::move(expr))
            , as_(as)
            , cols_(std::move(cols)) {}

        const Expr* expr() const { return expr_.get(); }
        Sym as() const { return as_; }
        const auto& cols() const { return cols_; }

        std::ostream& stream(std::ostream&) const override;

    private:
        AST<Expr> expr_;
        Sym as_;
        Syms cols_;
    };

    Select(Loc loc,
           bool all,
           ASTs<Elem>&& elems,
           ASTs<From>&& froms,
           AST<Expr>&& where,
           ASTs<Expr>&& groups,
           AST<Expr>&& having)
        : Expr(loc)
        , all_(all)
        , elems_(std::move(elems))
        , froms_(std::move(froms))
        , where_(std::move(where))
        , groups_(std::move(groups))
        , having_(std::move(having)) {}

    bool all() const { return all_; }
    bool distinct() const { return !all_; }
    const auto& elems() const { return elems_; }
    const auto& froms() const { return froms_; }
    const Expr* where() const { return where_.get(); }
    const auto& groups() const { return groups_; }
    const Expr* having() const { return having_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    bool all_;
    ASTs<Elem> elems_;
    ASTs<From> froms_;
    AST<Expr> where_;
    ASTs<Expr> groups_;
    AST<Expr> having_;
};

/// `lhs UNION|INTERSECT|EXCEPT [ALL] rhs`
class SetOp : public Expr {
public:
    enum Tag { Union, Intersect, Except };

    SetOp(Loc loc, AST<Expr>&& lhs, Tag tag, bool all, AST<Expr>&& rhs)
        : Expr(loc)
        , lhs_(std::move(lhs))
        , tag_(tag)
        , all_(all)
        , rhs_(std::move(rhs)) {}

    const Expr* lhs() const { return lhs_.get(); }
    Tag tag() const { return tag_; }
    bool all() const { return all_; }
    const Expr* rhs() const { return rhs_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> lhs_;
    Tag tag_;
    bool all_;
    AST<Expr> rhs_;
};

/// A query body wrapped in its trailing `ORDER BY` / `OFFSET` / `FETCH` clauses.
/// Only constructed when at least one of them is present.
class Query : public Expr {
public:
    /// One `ORDER BY` key.
    class Order : public Node {
    public:
        Order(Loc loc, AST<Expr>&& expr, bool desc)
            : Node(loc)
            , expr_(std::move(expr))
            , desc_(desc) {}

        const Expr* expr() const { return expr_.get(); }
        bool desc() const { return desc_; }

        std::ostream& stream(std::ostream&) const override;

    private:
        AST<Expr> expr_;
        bool desc_;
    };

    Query(Loc loc, AST<Expr>&& body, ASTs<Order>&& orders, AST<Expr>&& offset, AST<Expr>&& fetch)
        : Expr(loc)
        , body_(std::move(body))
        , orders_(std::move(orders))
        , offset_(std::move(offset))
        , fetch_(std::move(fetch)) {}

    const Expr* body() const { return body_.get(); }
    const auto& orders() const { return orders_; }
    const Expr* offset() const { return offset_.get(); }
    const Expr* fetch() const { return fetch_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> body_;
    ASTs<Order> orders_;
    AST<Expr> offset_;
    AST<Expr> fetch_;
};

/// `INSERT INTO <table> [(cols)] VALUES (...), (...)` or `INSERT INTO <table> [(cols)] <query>`.
/// Exactly one of Insert::rows and Insert::query carries the source of the new rows.
class Insert : public Expr {
public:
    Insert(Loc loc, Sym sym, Syms&& cols, ASTs<Expr>&& rows, AST<Expr>&& query)
        : Expr(loc)
        , sym_(sym)
        , cols_(std::move(cols))
        , rows_(std::move(rows))
        , query_(std::move(query)) {}

    Sym sym() const { return sym_; }
    const auto& cols() const { return cols_; }
    const auto& rows() const { return rows_; }
    const Expr* query() const { return query_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    Syms cols_;
    ASTs<Expr> rows_;
    AST<Expr> query_;
};

/// `UPDATE <table> [AS <as>] SET <assigns> [WHERE <where>]`
class Update : public Expr {
public:
    /// One `<column> = <expr>` of the `SET` clause.
    class Assign : public Node {
    public:
        Assign(Loc loc, Sym sym, AST<Expr>&& expr)
            : Node(loc)
            , sym_(sym)
            , expr_(std::move(expr)) {}

        Sym sym() const { return sym_; }
        const Expr* expr() const { return expr_.get(); }

        std::ostream& stream(std::ostream&) const override;

    private:
        Sym sym_;
        AST<Expr> expr_;
    };

    Update(Loc loc, Sym sym, Sym as, ASTs<Assign>&& assigns, AST<Expr>&& where)
        : Expr(loc)
        , sym_(sym)
        , as_(as)
        , assigns_(std::move(assigns))
        , where_(std::move(where)) {}

    Sym sym() const { return sym_; }
    Sym as() const { return as_; }
    const auto& assigns() const { return assigns_; }
    const Expr* where() const { return where_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    Sym as_;
    ASTs<Assign> assigns_;
    AST<Expr> where_;
};

/// `DELETE FROM <table> [AS <as>] [WHERE <where>]`
class Delete : public Expr {
public:
    Delete(Loc loc, Sym sym, Sym as, AST<Expr>&& where)
        : Expr(loc)
        , sym_(sym)
        , as_(as)
        , where_(std::move(where)) {}

    Sym sym() const { return sym_; }
    Sym as() const { return as_; }
    const Expr* where() const { return where_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    Sym as_;
    AST<Expr> where_;
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
