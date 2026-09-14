#pragma once

#include <concepts>

#include <ostream>
#include <tuple>

#include <fe/arena.h>
#include <fe/cast.h>
#include <fe/format.h>
#include <fe/vector.h>
#include <fe/vla.h>

#include "sql/tok.h"

namespace sql {

class Expr;

/// Nodes live in the Driver's Arena and are never destroyed, so this merely points at one.
template<class T>
using AST = fe::Arena::Ref<const T>;

/// Scratch buffers the Parser fills before it creates a node; a node keeps its own lists right
/// behind itself - see fe::VLA - and hands them out as a fe::View.
template<class T>
using ASTs = fe::Vector<AST<T>>;
using Syms = fe::Vector<Sym>;

/// Base class for all @p Expr%essions.
class Node : public fe::RuntimeCast<Node> {
public:
    Node(Loc loc)
        : loc_(loc) {}

    Loc loc() const { return loc_; }
    void dump() const;

    /// Stream to @p o.
    virtual void stream(std::ostream& o) const = 0;
    friend std::ostream& operator<<(std::ostream& o, const Node& node) { return node.stream(o), o; }

private:
    Loc loc_;
};

template<class T>
requires std::derived_from<T, Node>
std::ostream& operator<<(std::ostream& o, const AST<T>& ast) {
    return ast->stream(o), o;
}

/*
 * Interval
 */

/// The `<field> [(p)] [TO <field> [(p)]]` tail shared by the `INTERVAL` type and the `INTERVAL`
/// literal: `INTERVAL '1-2' YEAR TO MONTH`, `CAST(x AS INTERVAL DAY(3) TO SECOND(6))`.
class Interval : public Node, public fe::VLA<Interval> {
public:
    using VLA_Types = std::tuple<AST<Expr>, AST<Expr>>;

    Interval(Loc loc, Tok::Tag from, Tok::Tag to)
        : Node(loc)
        , from_(from)
        , to_(to) {}

    Tok::Tag from() const { return from_; }
    auto from_args() const { return vla<0>(); }
    Tok::Tag to() const { return to_; } ///< Tok::Tag::Nil if there is no `TO` field.
    auto to_args() const { return vla<1>(); }

    void stream(std::ostream&) const override;

private:
    Tok::Tag from_;
    Tok::Tag to_;
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
/// `INTEGER`, `CHARACTER VARYING(12)`, `NUMERIC(10, 2)`, `TIMESTAMP WITH TIME ZONE`.
class SimpleType : public Type, public fe::VLA<SimpleType> {
public:
    using VLA_Types = std::tuple<AST<Expr>>;

    SimpleType(Loc loc, Tok::Tag tag, bool varying, Tok::Tag zone, AST<Interval> interval, bool not_null)
        : Type(loc, not_null)
        , tag_(tag)
        , varying_(varying)
        , zone_(zone)
        , interval_(interval) {}

    Tok::Tag tag() const { return tag_; }
    bool varying() const { return varying_; }
    auto args() const { return vla<0>(); }
    /// Tok::Tag::K_WITH or Tok::Tag::K_WITHOUT for a `[WITHOUT] TIME ZONE`; Tok::Tag::Nil otherwise.
    Tok::Tag zone() const { return zone_; }
    const Interval* interval() const { return interval_.get(); } ///< Qualifier of an `INTERVAL` type.

    void stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    bool varying_;
    Tok::Tag zone_;
    AST<Interval> interval_;
};

/// A type named by an identifier, i.e. anything not a reserved word: `text`, `jsonb`, `uuid`, ...
class NamedType : public Type, public fe::VLA<NamedType> {
public:
    using VLA_Types = std::tuple<AST<Expr>>;

    NamedType(Loc loc, Sym sym, bool not_null)
        : Type(loc, not_null)
        , sym_(sym) {}

    Sym sym() const { return sym_; }
    auto args() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
    Sym sym_;
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
 * Order / Window
 */

/// One sort key: `<expr> [ASC|DESC] [NULLS FIRST|NULLS LAST]`.
/// Used by `ORDER BY`, by a window specification, by `WITHIN GROUP`, and by `CREATE INDEX`.
class Order : public Node {
public:
    enum Nulls { Nulls_None, Nulls_First, Nulls_Last };

    Order(Loc loc, AST<Expr> expr, bool desc, Nulls nulls)
        : Node(loc)
        , expr_(expr)
        , desc_(desc)
        , nulls_(nulls) {}

    const Expr* expr() const { return expr_.get(); }
    bool desc() const { return desc_; }
    Nulls nulls() const { return nulls_; }

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    bool desc_;
    Nulls nulls_;
};

/// A window frame: `ROWS BETWEEN 1 PRECEDING AND CURRENT ROW EXCLUDE TIES`.
class Frame : public Node {
public:
    /// One frame bound; Bound::expr only carries meaning for Bound::Preceding and Bound::Following.
    class Bound : public Node {
    public:
        enum Tag { Unbounded_Preceding, Preceding, Current_Row, Following, Unbounded_Following };

        Bound(Loc loc, Tag tag, AST<Expr> expr)
            : Node(loc)
            , tag_(tag)
            , expr_(expr) {}

        Tag tag() const { return tag_; }
        const Expr* expr() const { return expr_.get(); }

        void stream(std::ostream&) const override;

    private:
        Tag tag_;
        AST<Expr> expr_;
    };

    enum Exclude { Exclude_None, Exclude_Current_Row, Exclude_Group, Exclude_Ties, Exclude_No_Others };

    Frame(Loc loc, Tok::Tag unit, AST<Bound> lo, AST<Bound> hi, Exclude exclude)
        : Node(loc)
        , unit_(unit)
        , lo_(lo)
        , hi_(hi)
        , exclude_(exclude) {}

    Tok::Tag unit() const { return unit_; } ///< Tok::Tag::K_ROWS, Tok::Tag::K_RANGE, or Tok::Tag::K_GROUPS.
    const Bound* lo() const { return lo_.get(); }
    const Bound* hi() const { return hi_.get(); } ///< Only set for the `BETWEEN lo AND hi` form.
    Exclude exclude() const { return exclude_; }

    void stream(std::ostream&) const override;

private:
    Tok::Tag unit_;
    AST<Bound> lo_;
    AST<Bound> hi_;
    Exclude exclude_;
};

/// A window specification: the `(...)` of an `OVER` clause or of a `WINDOW` definition.
/// A bare Window::name with nothing else refers to a window defined in the `WINDOW` clause.
class Window : public Node, public fe::VLA<Window> {
public:
    using VLA_Types = std::tuple<AST<Expr>, AST<Order>>;

    Window(Loc loc, Sym name, bool paren, AST<Frame> frame)
        : Node(loc)
        , name_(name)
        , paren_(paren)
        , frame_(frame) {}

    Sym name() const { return name_; }    ///< An existing window this one refines; may be empty.
    bool paren() const { return paren_; } ///< Tells the parenthesized `OVER (w)` from the bare `OVER w`.
    auto partitions() const { return vla<0>(); }
    auto orders() const { return vla<1>(); }
    const Frame* frame() const { return frame_.get(); }

    void stream(std::ostream&) const override;

private:
    Sym name_;
    bool paren_;
    AST<Frame> frame_;
};

/// The drop behavior of a `DROP` or `ALTER` statement: `CASCADE` and `RESTRICT` are not reserved
/// words, so - unlike e.g. `WITH TIME ZONE` - there is no Tok::Tag to record them with.
enum class Behavior { None, Cascade, Restrict };

/*
 * Constraint
 */

/// A column- or table-level constraint of a Create statement.
/// One node covers all flavors: which of Constraint::cols, Constraint::table, Constraint::ref_cols,
/// and Constraint::expr carry meaning depends on Constraint::tag.
class Constraint : public Node, public fe::VLA<Constraint> {
public:
    enum Tag {
        Primary_Key, ///< `PRIMARY KEY [(cols)]`
        Unique,      ///< `UNIQUE [(cols)]`
        Check,       ///< `CHECK (expr)`
        Default,     ///< `DEFAULT expr`
        References,  ///< `REFERENCES table [(ref_cols)]` - column-level
        Foreign_Key, ///< `FOREIGN KEY (cols) REFERENCES table [(ref_cols)]` - table-level
    };

    /// A referential action of an `ON DELETE`/`ON UPDATE` clause.
    enum Action { Action_None, No_Action, Restrict, Cascade, Set_Null, Set_Default };

    using VLA_Types = std::tuple<Sym, Sym, Sym>;

    Constraint(Loc loc, Sym name, Tag tag, AST<Expr> expr, Action on_delete, Action on_update)
        : Node(loc)
        , name_(name)
        , tag_(tag)
        , expr_(expr)
        , on_delete_(on_delete)
        , on_update_(on_update) {}

    Sym name() const { return name_; } ///< From a leading `CONSTRAINT <name>`; may be empty.
    Tag tag() const { return tag_; }
    auto cols() const { return vla<0>(); }
    auto table() const { return vla<1>(); } ///< The referenced table, possibly qualified.
    auto ref_cols() const { return vla<2>(); }
    const Expr* expr() const { return expr_.get(); }
    Action on_delete() const { return on_delete_; }
    Action on_update() const { return on_update_; }

    void stream(std::ostream&) const override;

private:
    Sym name_;
    Tag tag_;
    AST<Expr> expr_;
    Action on_delete_;
    Action on_update_;
};

/*
 * Val
 */

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

    void stream(std::ostream&) const override;

private:
    uint64_t u64_;
};

/// An exact or approximate numeric literal with a fraction or an exponent: `1.5`, `.5`, `1e-3`.
/// Sym holds the literal *verbatim*, so the printer can emit it back unchanged.
class RealVal : public Val {
public:
    RealVal(Loc loc, Sym sym)
        : Val(loc)
        , sym_(sym) {}

    Sym sym() const { return sym_; }
    double f64() const; ///< The literal's value.

    void stream(std::ostream&) const override;

private:
    Sym sym_;
};

/// A single-quoted string literal; Sym holds the *unquoted* content.
class StrVal : public Val {
public:
    StrVal(Loc loc, Sym sym)
        : Val(loc)
        , sym_(sym) {}

    Sym sym() const { return sym_; }

    void stream(std::ostream&) const override;

private:
    Sym sym_;
};

/// `TRUE`, `FALSE`, `UNKNOWN`, `NULL`, `DEFAULT`, or the `*` of `COUNT(*)`.
class SimpleVal : public Val {
public:
    SimpleVal(Loc loc, Tok::Tag tag)
        : Val(loc)
        , tag_(tag) {}

    Tok::Tag tag() const { return tag_; }

    void stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
};

/// A typed literal: `DATE '2024-01-01'`, `TIMESTAMP '...'`, `INTERVAL '1-2' YEAR TO MONTH`.
class TypedVal : public Val {
public:
    TypedVal(Loc loc, Tok::Tag tag, Sym sym, AST<Interval> interval)
        : Val(loc)
        , tag_(tag)
        , sym_(sym)
        , interval_(interval) {}

    Tok::Tag tag() const { return tag_; }                        ///< `DATE`, `TIME`, `TIMESTAMP`, or `INTERVAL`.
    Sym sym() const { return sym_; }                             ///< The *unquoted* body of the literal.
    const Interval* interval() const { return interval_.get(); } ///< `INTERVAL` only; may be null.

    void stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    Sym sym_;
    AST<Interval> interval_;
};

/// A dynamic parameter marker: `?`, `$1`, or `:name`. Sym holds the marker verbatim.
class Param : public Val {
public:
    Param(Loc loc, Sym sym)
        : Val(loc)
        , sym_(sym) {}

    Sym sym() const { return sym_; }

    void stream(std::ostream&) const override;

private:
    Sym sym_;
};

/*
 * Expr
 */

class ParenExprList : public Expr, public fe::VLA<ParenExprList> {
public:
    using VLA_Types = std::tuple<AST<Expr>>;

    ParenExprList(Loc loc)
        : Expr(loc) {}

    auto args() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
};

class Id : public Expr, public fe::VLA<Id> {
public:
    using VLA_Types = std::tuple<Sym>;

    Id(Loc loc, bool asterisk)
        : Expr(loc)
        , asterisk_(asterisk) {}

    auto syms() const { return vla<0>(); }
    bool asterisk() const { return asterisk_; }

    void stream(std::ostream&) const override;

private:
    bool asterisk_ = false;

public:
    mutable bool asterisk_allowed_ = false;
};

class UnExpr : public Expr {
public:
    UnExpr(Loc loc, Tok::Tag tag, AST<Expr> rhs)
        : Expr(loc)
        , tag_(tag)
        , rhs_(rhs) {}

    Tok::Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }

    void stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    AST<Expr> rhs_;
};

/// Any `name(args)` call - an aggregate like `COUNT(DISTINCT x)` just as much as a scalar function.
/// The trailing clauses are what turn one into an ordered-set aggregate or a window function.
class Func : public Expr, public fe::VLA<Func> {
public:
    using VLA_Types = std::tuple<Sym, AST<Expr>, AST<Order>>;

    Func(Loc loc, bool distinct, AST<Expr> filter, AST<Window> over)
        : Expr(loc)
        , distinct_(distinct)
        , filter_(filter)
        , over_(over) {}

    auto syms() const { return vla<0>(); } ///< The function name, possibly schema-qualified.
    bool distinct() const { return distinct_; }
    auto args() const { return vla<1>(); }
    auto withins() const { return vla<2>(); }            ///< `WITHIN GROUP (ORDER BY ...)`
    const Expr* filter() const { return filter_.get(); } ///< `FILTER (WHERE ...)`
    const Window* over() const { return over_.get(); }   ///< `OVER (...)`

    void stream(std::ostream&) const override;

private:
    bool distinct_;
    AST<Expr> filter_;
    AST<Window> over_;
};

/// `expr [NOT] BETWEEN lo AND hi`
class Between : public Expr {
public:
    Between(Loc loc, AST<Expr> expr, AST<Expr> lo, AST<Expr> hi, bool negated)
        : Expr(loc)
        , expr_(expr)
        , lo_(lo)
        , hi_(hi)
        , negated_(negated) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* lo() const { return lo_.get(); }
    const Expr* hi() const { return hi_.get(); }
    bool negated() const { return negated_; }

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> lo_;
    AST<Expr> hi_;
    bool negated_;
};

/// `expr [NOT] LIKE pattern [ESCAPE escape]` - or `SIMILAR TO` instead of `LIKE`.
class Like : public Expr {
public:
    /// Tok::Tag::K_LIKE, Tok::Tag::K_ILIKE - the case-insensitive `LIKE` every dialect but the
    /// standard has - or Tok::Tag::K_SIMILAR, which matches a regular expression instead.
    Like(Loc loc, AST<Expr> expr, AST<Expr> pattern, AST<Expr> escape, bool negated, Tok::Tag tag)
        : Expr(loc)
        , expr_(expr)
        , pattern_(pattern)
        , escape_(escape)
        , negated_(negated)
        , tag_(tag) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* pattern() const { return pattern_.get(); }
    const Expr* escape() const { return escape_.get(); } ///< May be null.
    bool negated() const { return negated_; }
    Tok::Tag tag() const { return tag_; }

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> pattern_;
    AST<Expr> escape_;
    bool negated_;
    Tok::Tag tag_;
};

/// `expr[index]` - an array element. Binds tighter than every operator, so it never needs
/// parentheses of its own and none of them can come between it and the expression it indexes.
class Subscript : public Expr {
public:
    Subscript(Loc loc, AST<Expr> expr, AST<Expr> index)
        : Expr(loc)
        , expr_(expr)
        , index_(index) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* index() const { return index_.get(); }

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> index_;
};

/// `CAST(expr AS type)`
class Cast : public Expr {
public:
    Cast(Loc loc, AST<Expr> expr, AST<Type> type)
        : Expr(loc)
        , expr_(expr)
        , type_(type) {}

    const Expr* expr() const { return expr_.get(); }
    const Type* type() const { return type_.get(); }

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Type> type_;
};

/// `expr COLLATE <collation>`
class Collate : public Expr, public fe::VLA<Collate> {
public:
    using VLA_Types = std::tuple<Sym>;

    Collate(Loc loc, AST<Expr> expr)
        : Expr(loc)
        , expr_(expr) {}

    const Expr* expr() const { return expr_.get(); }
    auto syms() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
};

/// `CASE [operand] WHEN ... THEN ... [ELSE ...] END` - CaseExpr::operand is null for the *searched* form.
class CaseExpr : public Expr, public fe::VLA<CaseExpr> {
public:
    class When : public Node {
    public:
        When(Loc loc, AST<Expr> cond, AST<Expr> then)
            : Node(loc)
            , cond_(cond)
            , then_(then) {}

        const Expr* cond() const { return cond_.get(); }
        const Expr* then() const { return then_.get(); }

        void stream(std::ostream&) const override;

    private:
        AST<Expr> cond_;
        AST<Expr> then_;
    };

    using VLA_Types = std::tuple<AST<When>>;

    CaseExpr(Loc loc, AST<Expr> operand, AST<Expr> elze)
        : Expr(loc)
        , operand_(operand)
        , elze_(elze) {}

    const Expr* operand() const { return operand_.get(); }
    auto whens() const { return vla<0>(); }
    const Expr* elze() const { return elze_.get(); }

    void stream(std::ostream&) const override;

private:
    AST<Expr> operand_;
    AST<Expr> elze_;
};

/// `EXTRACT(<field> FROM expr)` - the field is kept as a Sym, so vendor fields like `epoch` work too.
class Extract : public Expr {
public:
    Extract(Loc loc, Sym field, AST<Expr> expr)
        : Expr(loc)
        , field_(field)
        , expr_(expr) {}

    Sym field() const { return field_; }
    const Expr* expr() const { return expr_.get(); }

    void stream(std::ostream&) const override;

private:
    Sym field_;
    AST<Expr> expr_;
};

/// `SUBSTRING(expr FROM start [FOR len])`.
/// The comma-separated `SUBSTRING(x, 1, 2)` is an ordinary Func instead.
class Substring : public Expr {
public:
    Substring(Loc loc, AST<Expr> expr, AST<Expr> from, AST<Expr> four)
        : Expr(loc)
        , expr_(expr)
        , from_(from)
        , four_(four) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* from() const { return from_.get(); }
    const Expr* four() const { return four_.get(); } ///< The `FOR` length; may be null.

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> from_;
    AST<Expr> four_;
};

/// `TRIM([[LEADING|TRAILING|BOTH] [chars] FROM] expr)`
class Trim : public Expr {
public:
    Trim(Loc loc, Tok::Tag tag, AST<Expr> chars, AST<Expr> expr)
        : Expr(loc)
        , tag_(tag)
        , chars_(chars)
        , expr_(expr) {}

    /// Tok::Tag::K_LEADING, Tok::Tag::K_TRAILING, Tok::Tag::K_BOTH, or Tok::Tag::Nil.
    Tok::Tag tag() const { return tag_; }
    const Expr* chars() const { return chars_.get(); } ///< What to trim; may be null.
    const Expr* expr() const { return expr_.get(); }

    void stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    AST<Expr> chars_;
    AST<Expr> expr_;
};

/// `POSITION(needle IN haystack)`
class Position : public Expr {
public:
    Position(Loc loc, AST<Expr> needle, AST<Expr> haystack)
        : Expr(loc)
        , needle_(needle)
        , haystack_(haystack) {}

    const Expr* needle() const { return needle_.get(); }
    const Expr* haystack() const { return haystack_.get(); }

    void stream(std::ostream&) const override;

private:
    AST<Expr> needle_;
    AST<Expr> haystack_;
};

/// `OVERLAY(expr PLACING placing FROM from [FOR len])`
class Overlay : public Expr {
public:
    Overlay(Loc loc, AST<Expr> expr, AST<Expr> placing, AST<Expr> from, AST<Expr> four)
        : Expr(loc)
        , expr_(expr)
        , placing_(placing)
        , from_(from)
        , four_(four) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* placing() const { return placing_.get(); }
    const Expr* from() const { return from_.get(); }
    const Expr* four() const { return four_.get(); } ///< The `FOR` length; may be null.

    void stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> placing_;
    AST<Expr> from_;
    AST<Expr> four_;
};

class BinExpr : public Expr {
public:
    BinExpr(Loc loc, AST<Expr> lhs, Tok::Tag tag, AST<Expr> rhs)
        : Expr(loc)
        , lhs_(lhs)
        , tag_(tag)
        , rhs_(rhs) {}

    const Expr* lhs() const { return lhs_.get(); }
    Tok::Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }

    void stream(std::ostream&) const override;

private:
    AST<Expr> lhs_;
    Tok::Tag tag_;
    AST<Expr> rhs_;
};

class BinExprWithPreTag : public BinExpr {
public:
    BinExprWithPreTag(Loc loc, AST<Expr> lhs, Tok::Tag pretag, Tok::Tag tag, AST<Expr> rhs)
        : BinExpr(loc, lhs, tag, rhs)
        , pretag_(pretag) {}
    Tok::Tag pretag() const { return pretag_; }

    void stream(std::ostream&) const override;

private:
    Tok::Tag pretag_;
};

/// A quantified comparison: `lhs = ANY (subquery)`, `lhs > ALL (subquery)`.
class QuantExpr : public BinExpr {
public:
    QuantExpr(Loc loc, AST<Expr> lhs, Tok::Tag tag, Tok::Tag quant, AST<Expr> rhs)
        : BinExpr(loc, lhs, tag, rhs)
        , quant_(quant) {}

    /// Tok::Tag::K_ALL, Tok::Tag::K_ANY, or Tok::Tag::K_SOME.
    Tok::Tag quant() const { return quant_; }

    void stream(std::ostream&) const override;

private:
    Tok::Tag quant_;
};

/*
 * Query
 */

/// A grouping element beyond a plain expression: `ROLLUP (a, b)`, `CUBE (a, b)`,
/// `GROUPING SETS ((a), ())`, or the empty grouping set `()`.
class Grouping : public Expr, public fe::VLA<Grouping> {
public:
    enum Tag { Rollup, Cube, Sets, Empty };

    using VLA_Types = std::tuple<AST<Expr>>;

    Grouping(Loc loc, Tag tag)
        : Expr(loc)
        , tag_(tag) {}

    Tag tag() const { return tag_; }
    auto args() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
    Tag tag_;
};

/// A `VALUES` table: `VALUES (1, 'a'), (2, 'b')`. Stands on its own as a query, and is what an
/// `INSERT` without a source query carries.
class Values : public Expr, public fe::VLA<Values> {
public:
    using VLA_Types = std::tuple<AST<Expr>>;

    Values(Loc loc)
        : Expr(loc) {}

    auto rows() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
};

/// `TABLE <name>` - the explicit-table shorthand for `SELECT * FROM <name>`.
class Table : public Expr, public fe::VLA<Table> {
public:
    using VLA_Types = std::tuple<Sym>;

    Table(Loc loc)
        : Expr(loc) {}

    auto syms() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
};

/*
 * DDL
 */

/// `CREATE [GLOBAL|LOCAL TEMPORARY] TABLE [IF NOT EXISTS] <name> (<elems>)`, or
/// `CREATE TABLE <name> [(cols)] AS <query>`.
class Create : public Expr, public fe::VLA<Create> {
public:
    /// One column definition: `<name> <type> <constraints>`.
    class Elem : public Node, public fe::VLA<Elem> {
    public:
        using VLA_Types = std::tuple<AST<Constraint>>;

        Elem(Loc loc, Sym sym, AST<Type> type)
            : Node(loc)
            , sym_(sym)
            , type_(type) {}

        Sym sym() const { return sym_; }
        const Type* type() const { return type_.get(); }
        auto constraints() const { return vla<0>(); }

        void stream(std::ostream&) const override;

    private:
        Sym sym_;
        AST<Type> type_;
    };

    using VLA_Types = std::tuple<Sym, AST<Elem>, AST<Constraint>>;

    Create(Loc loc, bool temporary, bool if_not_exists, AST<Expr> query)
        : Expr(loc)
        , temporary_(temporary)
        , if_not_exists_(if_not_exists)
        , query_(query) {}

    auto syms() const { return vla<0>(); }
    bool temporary() const { return temporary_; }
    bool if_not_exists() const { return if_not_exists_; }
    auto elems() const { return vla<1>(); }
    auto constraints() const { return vla<2>(); }      ///< Table-level constraints.
    const Expr* query() const { return query_.get(); } ///< `CREATE TABLE ... AS <query>`; may be null.

    void stream(std::ostream&) const override;

private:
    bool temporary_;
    bool if_not_exists_;
    AST<Expr> query_;
};

/// `CREATE [OR REPLACE] VIEW <name> [(cols)] AS <query> [WITH [CASCADED|LOCAL] CHECK OPTION]`
class CreateView : public Expr, public fe::VLA<CreateView> {
public:
    using VLA_Types = std::tuple<Sym, Sym>;

    CreateView(Loc loc, bool replace, AST<Expr> query, Tok::Tag check)
        : Expr(loc)
        , replace_(replace)
        , query_(query)
        , check_(check) {}

    auto syms() const { return vla<0>(); }
    bool replace() const { return replace_; }
    auto cols() const { return vla<1>(); }
    const Expr* query() const { return query_.get(); }
    /// `WITH CHECK OPTION`: Tok::Tag::K_CASCADED, Tok::Tag::K_LOCAL, Tok::Tag::K_CHECK (unqualified),
    /// or Tok::Tag::Nil for no check option at all.
    Tok::Tag check() const { return check_; }

    void stream(std::ostream&) const override;

private:
    bool replace_;
    AST<Expr> query_;
    Tok::Tag check_;
};

/// `CREATE [UNIQUE] INDEX [IF NOT EXISTS] <name> ON <table> (<cols>)`
class CreateIndex : public Expr, public fe::VLA<CreateIndex> {
public:
    using VLA_Types = std::tuple<Sym, AST<Order>>;

    CreateIndex(Loc loc, Sym sym, bool unique, bool if_not_exists)
        : Expr(loc)
        , sym_(sym)
        , unique_(unique)
        , if_not_exists_(if_not_exists) {}

    Sym sym() const { return sym_; }
    bool unique() const { return unique_; }
    bool if_not_exists() const { return if_not_exists_; }
    auto table() const { return vla<0>(); }
    auto cols() const { return vla<1>(); } ///< Index keys, each with its own `ASC`/`DESC`.

    void stream(std::ostream&) const override;

private:
    Sym sym_;
    bool unique_;
    bool if_not_exists_;
};

/// `CREATE SCHEMA [IF NOT EXISTS] <name>`
class CreateSchema : public Expr, public fe::VLA<CreateSchema> {
public:
    using VLA_Types = std::tuple<Sym>;

    CreateSchema(Loc loc, bool if_not_exists)
        : Expr(loc)
        , if_not_exists_(if_not_exists) {}

    auto syms() const { return vla<0>(); }
    bool if_not_exists() const { return if_not_exists_; }

    void stream(std::ostream&) const override;

private:
    bool if_not_exists_;
};

/// `ALTER TABLE <table> <action>` - the standard allows exactly one action per statement.
/// As with Constraint, one node covers every flavor and Alter::tag says which fields carry meaning.
class Alter : public Expr, public fe::VLA<Alter> {
public:
    enum Tag {
        Add_Column,      ///< `ADD [COLUMN] <elem>`
        Drop_Column,     ///< `DROP [COLUMN] <sym> [CASCADE|RESTRICT]`
        Add_Constraint,  ///< `ADD <constraint>`
        Drop_Constraint, ///< `DROP CONSTRAINT <sym> [CASCADE|RESTRICT]`
        Set_Default,     ///< `ALTER [COLUMN] <sym> SET DEFAULT <expr>`
        Drop_Default,    ///< `ALTER [COLUMN] <sym> DROP DEFAULT`
        Set_Not_Null,    ///< `ALTER [COLUMN] <sym> SET NOT NULL`
        Drop_Not_Null,   ///< `ALTER [COLUMN] <sym> DROP NOT NULL`
        Set_Data_Type,   ///< `ALTER [COLUMN] <sym> SET DATA TYPE <type>`
        Rename_Table,    ///< `RENAME TO <sym>`
        Rename_Column,   ///< `RENAME [COLUMN] <sym> TO <sym2>`
    };

    using VLA_Types = std::tuple<Sym>;

    Alter(Loc loc,
          Tag tag,
          Sym sym,
          Sym sym2,
          AST<Create::Elem> elem,
          AST<Constraint> constraint,
          AST<Type> type,
          AST<Expr> expr,
          Behavior behavior)
        : Expr(loc)
        , tag_(tag)
        , sym_(sym)
        , sym2_(sym2)
        , elem_(elem)
        , constraint_(constraint)
        , type_(type)
        , expr_(expr)
        , behavior_(behavior) {}

    auto table() const { return vla<0>(); }
    Tag tag() const { return tag_; }
    Sym sym() const { return sym_; }   ///< The column, constraint, or new table name.
    Sym sym2() const { return sym2_; } ///< The new column name of an Alter::Rename_Column.
    const Create::Elem* elem() const { return elem_.get(); }
    const Constraint* constraint() const { return constraint_.get(); }
    const Type* type() const { return type_.get(); }
    const Expr* expr() const { return expr_.get(); }
    Behavior behavior() const { return behavior_; }

    void stream(std::ostream&) const override;

private:
    Tag tag_;
    Sym sym_;
    Sym sym2_;
    AST<Create::Elem> elem_;
    AST<Constraint> constraint_;
    AST<Type> type_;
    AST<Expr> expr_;
    Behavior behavior_;
};

/// `DROP TABLE|VIEW|INDEX|SCHEMA [IF EXISTS] <name> [CASCADE|RESTRICT]`
class Drop : public Expr, public fe::VLA<Drop> {
public:
    enum Tag { Table, View, Index, Schema };

    using VLA_Types = std::tuple<Sym>;

    Drop(Loc loc, Tag tag, bool if_exists, Behavior behavior)
        : Expr(loc)
        , tag_(tag)
        , if_exists_(if_exists)
        , behavior_(behavior) {}

    Tag tag() const { return tag_; }
    auto syms() const { return vla<0>(); }
    bool if_exists() const { return if_exists_; }
    Behavior behavior() const { return behavior_; }

    void stream(std::ostream&) const override;

private:
    Tag tag_;
    bool if_exists_;
    Behavior behavior_;
};

/// `TRUNCATE TABLE <name>`
class Truncate : public Expr, public fe::VLA<Truncate> {
public:
    using VLA_Types = std::tuple<Sym>;

    Truncate(Loc loc)
        : Expr(loc) {}

    auto syms() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
};

/// A transaction-control statement; Transact::sym names the savepoint where one is involved.
class Transact : public Expr {
public:
    enum Tag {
        Start,       ///< `START TRANSACTION` - `BEGIN` parses into this, too.
        Commit,      ///< `COMMIT [WORK]`
        Rollback,    ///< `ROLLBACK [WORK]`
        Rollback_To, ///< `ROLLBACK [WORK] TO SAVEPOINT <sym>`
        Savepoint,   ///< `SAVEPOINT <sym>`
        Release,     ///< `RELEASE SAVEPOINT <sym>`
    };

    Transact(Loc loc, Tag tag, Sym sym)
        : Expr(loc)
        , tag_(tag)
        , sym_(sym) {}

    Tag tag() const { return tag_; }
    Sym sym() const { return sym_; }

    void stream(std::ostream&) const override;

private:
    Tag tag_;
    Sym sym_;
};

/*
 * Query expressions
 */

/// A `<table reference>` with something bound to it: a correlation name and its column aliases, a
/// `LATERAL`, or the `WITH ORDINALITY` of an `UNNEST`. Only built when there is something to bind -
/// the `t` of a bare `FROM t` is an Id and nothing more.
///
/// It is an Expr, not a part of Select, because a correlation name binds to a single table
/// reference: in `a AS x JOIN b AS y`, each side carries its own, and the Join sees two of these.
class TableRef : public Expr, public fe::VLA<TableRef> {
public:
    using VLA_Types = std::tuple<Sym>;

    TableRef(Loc loc, bool lateral, AST<Expr> expr, bool ordinality, Sym as)
        : Expr(loc)
        , lateral_(lateral)
        , expr_(expr)
        , ordinality_(ordinality)
        , as_(as) {}

    bool lateral() const { return lateral_; }
    const Expr* expr() const { return expr_.get(); }
    bool ordinality() const { return ordinality_; } ///< `WITH ORDINALITY` of an `UNNEST`.
    Sym as() const { return as_; }
    auto cols() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
    bool lateral_;
    AST<Expr> expr_;
    bool ordinality_;
    Sym as_;
};

class Join : public Expr {
public:
    using On    = AST<Expr>;
    using Using = fe::View<Sym>;
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

    Join(Loc loc, AST<Expr> lhs, Tag tag, AST<Expr> rhs, Spec spec)
        : Expr(loc)
        , lhs_(lhs)
        , tag_(tag)
        , rhs_(rhs)
        , spec_(spec) {}

    const Expr* lhs() const { return lhs_.get(); }
    Tag tag() const { return tag_; }
    const Expr* rhs() const { return rhs_.get(); }
    const auto& spec() const { return spec_; }

    void stream(std::ostream&) const override;

private:
    AST<Expr> lhs_;
    Tag tag_;
    AST<Expr> rhs_;
    Spec spec_;
};

class Select : public Expr, public fe::VLA<Select> {
public:
    class Elem : public Node, public fe::VLA<Elem> {
    public:
        using VLA_Types = std::tuple<Sym>;

        Elem(Loc loc, AST<Expr> expr)
            : Node(loc)
            , expr_(expr) {}

        const Expr* expr() const { return expr_.get(); }
        auto syms() const { return vla<0>(); }

        void stream(std::ostream&) const override;

    private:
        AST<Expr> expr_;
    };

    /// One entry of the `WINDOW` clause: `<name> AS (<window>)`.
    class WindowDef : public Node {
    public:
        WindowDef(Loc loc, Sym sym, AST<Window> window)
            : Node(loc)
            , sym_(sym)
            , window_(window) {}

        Sym sym() const { return sym_; }
        const Window* window() const { return window_.get(); }

        void stream(std::ostream&) const override;

    private:
        Sym sym_;
        AST<Window> window_;
    };

    using VLA_Types = std::tuple<AST<Elem>, AST<Expr>, AST<Expr>, AST<WindowDef>>;

    Select(Loc loc, bool all, AST<Expr> where, AST<Expr> having)
        : Expr(loc)
        , all_(all)
        , where_(where)
        , having_(having) {}

    bool all() const { return all_; }
    bool distinct() const { return !all_; }
    auto elems() const { return vla<0>(); }
    auto froms() const { return vla<1>(); } ///< A table reference each; may be empty, as `SELECT 1` has no `FROM`.
    const Expr* where() const { return where_.get(); }
    auto groups() const { return vla<2>(); }
    const Expr* having() const { return having_.get(); }
    auto windows() const { return vla<3>(); }

    void stream(std::ostream&) const override;

private:
    bool all_;
    AST<Expr> where_;
    AST<Expr> having_;
};

/// `lhs UNION|INTERSECT|EXCEPT [ALL] rhs`
class SetOp : public Expr {
public:
    enum Tag { Union, Intersect, Except };

    SetOp(Loc loc, AST<Expr> lhs, Tag tag, bool all, AST<Expr> rhs)
        : Expr(loc)
        , lhs_(lhs)
        , tag_(tag)
        , all_(all)
        , rhs_(rhs) {}

    const Expr* lhs() const { return lhs_.get(); }
    Tag tag() const { return tag_; }
    bool all() const { return all_; }
    const Expr* rhs() const { return rhs_.get(); }

    void stream(std::ostream&) const override;

private:
    AST<Expr> lhs_;
    Tag tag_;
    bool all_;
    AST<Expr> rhs_;
};

/// A query body wrapped in its `WITH` clause and its trailing `ORDER BY` / `OFFSET` / `FETCH` /
/// `LIMIT` clauses. Only constructed when at least one of them is present.
/// `FOR UPDATE|NO KEY UPDATE|SHARE|KEY SHARE [OF <tables>] [NOWAIT|SKIP LOCKED]` - the row-locking
/// clause a query expression may end in. Not in the standard, but every dialect that has rows to
/// lock spells it this way.
class Lock : public Node, public fe::VLA<Lock> {
public:
    /// How hard to lock, weakest last - the order the standard dialects list them in.
    enum Strength { Update, No_Key_Update, Share, Key_Share };
    /// What to do about a row someone else holds: block (the default), fail, or pass it over.
    enum Wait { Block, Nowait, Skip_Locked };

    /// Each table is a possibly qualified name, so the names themselves live in the Arena, too.
    using VLA_Types = std::tuple<fe::View<Sym>>;

    Lock(Loc loc, Strength strength, Wait wait)
        : Node(loc)
        , strength_(strength)
        , wait_(wait) {}

    Strength strength() const { return strength_; }
    auto tables() const { return vla<0>(); } ///< The `OF` list; empty locks every table of the query.
    Wait wait() const { return wait_; }

    void stream(std::ostream&) const override;

private:
    Strength strength_;
    Wait wait_;
};

class Query : public Expr, public fe::VLA<Query> {
public:
    /// One common table expression: `<name> [(cols)] AS (<query>)`.
    class Cte : public Node, public fe::VLA<Cte> {
    public:
        using VLA_Types = std::tuple<Sym>;

        Cte(Loc loc, Sym sym, AST<Expr> query)
            : Node(loc)
            , sym_(sym)
            , query_(query) {}

        Sym sym() const { return sym_; }
        auto cols() const { return vla<0>(); }
        const Expr* query() const { return query_.get(); }

        void stream(std::ostream&) const override;

    private:
        Sym sym_;
        AST<Expr> query_;
    };

    using VLA_Types = std::tuple<AST<Cte>, AST<Order>, AST<Lock>>;

    Query(Loc loc, bool recursive, AST<Expr> body, AST<Expr> offset, AST<Expr> fetch, AST<Expr> limit)
        : Expr(loc)
        , recursive_(recursive)
        , body_(body)
        , offset_(offset)
        , fetch_(fetch)
        , limit_(limit) {}

    bool recursive() const { return recursive_; }
    auto ctes() const { return vla<0>(); }
    const Expr* body() const { return body_.get(); }
    auto orders() const { return vla<1>(); }
    const Expr* offset() const { return offset_.get(); }
    const Expr* fetch() const { return fetch_.get(); }
    const Expr* limit() const { return limit_.get(); }
    auto locks() const { return vla<2>(); } ///< A query may end in more than one locking clause.

    void stream(std::ostream&) const override;

private:
    bool recursive_;
    AST<Expr> body_;
    AST<Expr> offset_;
    AST<Expr> fetch_;
    AST<Expr> limit_;
};

/*
 * Insert / Update / Delete
 */

/// `INSERT INTO <table> [(cols)] <query>` where the query is usually a Values table, or
/// `INSERT INTO <table> DEFAULT VALUES`.
class Insert : public Expr, public fe::VLA<Insert> {
public:
    using VLA_Types = std::tuple<Sym, Sym>;

    Insert(Loc loc, AST<Expr> query)
        : Expr(loc)
        , query_(query) {}

    auto syms() const { return vla<0>(); }
    auto cols() const { return vla<1>(); }
    const Expr* query() const { return query_.get(); } ///< Null for `DEFAULT VALUES`.

    void stream(std::ostream&) const override;

private:
    AST<Expr> query_;
};

/// `UPDATE <table> [AS <as>] SET <assigns> [WHERE <where>]`
class Update : public Expr, public fe::VLA<Update> {
public:
    /// One `<column> = <expr>` of the `SET` clause.
    class Assign : public Node, public fe::VLA<Assign> {
    public:
        using VLA_Types = std::tuple<Sym>;

        Assign(Loc loc, AST<Expr> expr)
            : Node(loc)
            , expr_(expr) {}

        auto syms() const { return vla<0>(); } ///< The target column, possibly qualified.
        const Expr* expr() const { return expr_.get(); }

        void stream(std::ostream&) const override;

    private:
        AST<Expr> expr_;
    };

    using VLA_Types = std::tuple<Sym, AST<Assign>>;

    Update(Loc loc, Sym as, AST<Expr> where)
        : Expr(loc)
        , as_(as)
        , where_(where) {}

    auto syms() const { return vla<0>(); }
    Sym as() const { return as_; }
    auto assigns() const { return vla<1>(); }
    const Expr* where() const { return where_.get(); }

    void stream(std::ostream&) const override;

private:
    Sym as_;
    AST<Expr> where_;
};

/// `DELETE FROM <table> [AS <as>] [WHERE <where>]`
class Delete : public Expr, public fe::VLA<Delete> {
public:
    using VLA_Types = std::tuple<Sym>;

    Delete(Loc loc, Sym as, AST<Expr> where)
        : Expr(loc)
        , as_(as)
        , where_(where) {}

    auto syms() const { return vla<0>(); }
    Sym as() const { return as_; }
    const Expr* where() const { return where_.get(); }

    void stream(std::ostream&) const override;

private:
    Sym as_;
    AST<Expr> where_;
};

/// Just a dummy that does nothing and will only be constructed during parse errors.
class ErrExpr : public Expr {
public:
    ErrExpr(Loc loc)
        : Expr(loc) {}

    void stream(std::ostream&) const override;
};

/*
 * Prog
 */

/// Just a HACK to have a list of Stmt%s.
class Prog : public Node, public fe::VLA<Prog> {
public:
    using VLA_Types = std::tuple<AST<Expr>>;

    Prog(Loc loc)
        : Node(loc) {}

    auto exprs() const { return vla<0>(); }

    void stream(std::ostream&) const override;

private:
};

} // namespace sql

#ifndef DOXYGEN
// clang-format off
template<std::derived_from<sql::Node> T> struct std::formatter<T,           char> : fe::ostream_formatter {};
template<std::derived_from<sql::Node> T> struct std::formatter<sql::AST<T>, char> : fe::ostream_formatter {};
// clang-format on
#endif
