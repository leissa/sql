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
 * Interval
 */

/// The `<field> [(p)] [TO <field> [(p)]]` tail shared by the `INTERVAL` type and the `INTERVAL`
/// literal: `INTERVAL '1-2' YEAR TO MONTH`, `CAST(x AS INTERVAL DAY(3) TO SECOND(6))`.
class Interval : public Node {
public:
    Interval(Loc loc, Tok::Tag from, ASTs<Expr>&& from_args, Tok::Tag to, ASTs<Expr>&& to_args)
        : Node(loc)
        , from_(from)
        , from_args_(std::move(from_args))
        , to_(to)
        , to_args_(std::move(to_args)) {}

    Tok::Tag from() const { return from_; }
    const auto& from_args() const { return from_args_; }
    Tok::Tag to() const { return to_; } ///< Tok::Tag::Nil if there is no `TO` field.
    const auto& to_args() const { return to_args_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag from_;
    ASTs<Expr> from_args_;
    Tok::Tag to_;
    ASTs<Expr> to_args_;
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
class SimpleType : public Type {
public:
    SimpleType(Loc loc,
               Tok::Tag tag,
               bool varying,
               ASTs<Expr>&& args,
               Tok::Tag zone,
               AST<Interval>&& interval,
               bool not_null)
        : Type(loc, not_null)
        , tag_(tag)
        , varying_(varying)
        , args_(std::move(args))
        , zone_(zone)
        , interval_(std::move(interval)) {}

    Tok::Tag tag() const { return tag_; }
    bool varying() const { return varying_; }
    const auto& args() const { return args_; }
    /// Tok::Tag::K_WITH or Tok::Tag::K_WITHOUT for a `[WITHOUT] TIME ZONE`; Tok::Tag::Nil otherwise.
    Tok::Tag zone() const { return zone_; }
    const Interval* interval() const { return interval_.get(); } ///< Qualifier of an `INTERVAL` type.

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    bool varying_;
    ASTs<Expr> args_;
    Tok::Tag zone_;
    AST<Interval> interval_;
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
 * Order / Window
 */

/// One sort key: `<expr> [ASC|DESC] [NULLS FIRST|NULLS LAST]`.
/// Used by `ORDER BY`, by a window specification, by `WITHIN GROUP`, and by `CREATE INDEX`.
class Order : public Node {
public:
    enum Nulls { Nulls_None, Nulls_First, Nulls_Last };

    Order(Loc loc, AST<Expr>&& expr, bool desc, Nulls nulls)
        : Node(loc)
        , expr_(std::move(expr))
        , desc_(desc)
        , nulls_(nulls) {}

    const Expr* expr() const { return expr_.get(); }
    bool desc() const { return desc_; }
    Nulls nulls() const { return nulls_; }

    std::ostream& stream(std::ostream&) const override;

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

        Bound(Loc loc, Tag tag, AST<Expr>&& expr)
            : Node(loc)
            , tag_(tag)
            , expr_(std::move(expr)) {}

        Tag tag() const { return tag_; }
        const Expr* expr() const { return expr_.get(); }

        std::ostream& stream(std::ostream&) const override;

    private:
        Tag tag_;
        AST<Expr> expr_;
    };

    enum Exclude { Exclude_None, Exclude_Current_Row, Exclude_Group, Exclude_Ties, Exclude_No_Others };

    Frame(Loc loc, Tok::Tag unit, AST<Bound>&& lo, AST<Bound>&& hi, Exclude exclude)
        : Node(loc)
        , unit_(unit)
        , lo_(std::move(lo))
        , hi_(std::move(hi))
        , exclude_(exclude) {}

    Tok::Tag unit() const { return unit_; } ///< Tok::Tag::K_ROWS, Tok::Tag::K_RANGE, or Tok::Tag::K_GROUPS.
    const Bound* lo() const { return lo_.get(); }
    const Bound* hi() const { return hi_.get(); } ///< Only set for the `BETWEEN lo AND hi` form.
    Exclude exclude() const { return exclude_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag unit_;
    AST<Bound> lo_;
    AST<Bound> hi_;
    Exclude exclude_;
};

/// A window specification: the `(...)` of an `OVER` clause or of a `WINDOW` definition.
/// A bare Window::name with nothing else refers to a window defined in the `WINDOW` clause.
class Window : public Node {
public:
    Window(Loc loc, Sym name, bool paren, ASTs<Expr>&& partitions, ASTs<Order>&& orders, AST<Frame>&& frame)
        : Node(loc)
        , name_(name)
        , paren_(paren)
        , partitions_(std::move(partitions))
        , orders_(std::move(orders))
        , frame_(std::move(frame)) {}

    Sym name() const { return name_; }    ///< An existing window this one refines; may be empty.
    bool paren() const { return paren_; } ///< Tells the parenthesized `OVER (w)` from the bare `OVER w`.
    const auto& partitions() const { return partitions_; }
    const auto& orders() const { return orders_; }
    const Frame* frame() const { return frame_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym name_;
    bool paren_;
    ASTs<Expr> partitions_;
    ASTs<Order> orders_;
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

    /// A referential action of an `ON DELETE`/`ON UPDATE` clause.
    enum Action { Action_None, No_Action, Restrict, Cascade, Set_Null, Set_Default };

    Constraint(Loc loc,
               Sym name,
               Tag tag,
               Syms&& cols,
               Syms&& table,
               Syms&& ref_cols,
               AST<Expr>&& expr,
               Action on_delete,
               Action on_update)
        : Node(loc)
        , name_(name)
        , tag_(tag)
        , cols_(std::move(cols))
        , table_(std::move(table))
        , ref_cols_(std::move(ref_cols))
        , expr_(std::move(expr))
        , on_delete_(on_delete)
        , on_update_(on_update) {}

    Sym name() const { return name_; } ///< From a leading `CONSTRAINT <name>`; may be empty.
    Tag tag() const { return tag_; }
    const auto& cols() const { return cols_; }
    const auto& table() const { return table_; } ///< The referenced table, possibly qualified.
    const auto& ref_cols() const { return ref_cols_; }
    const Expr* expr() const { return expr_.get(); }
    Action on_delete() const { return on_delete_; }
    Action on_update() const { return on_update_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym name_;
    Tag tag_;
    Syms cols_;
    Syms table_;
    Syms ref_cols_;
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

    std::ostream& stream(std::ostream&) const override;

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

    std::ostream& stream(std::ostream&) const override;

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

    std::ostream& stream(std::ostream&) const override;

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

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
};

/// A typed literal: `DATE '2024-01-01'`, `TIMESTAMP '...'`, `INTERVAL '1-2' YEAR TO MONTH`.
class TypedVal : public Val {
public:
    TypedVal(Loc loc, Tok::Tag tag, Sym sym, AST<Interval>&& interval)
        : Val(loc)
        , tag_(tag)
        , sym_(sym)
        , interval_(std::move(interval)) {}

    Tok::Tag tag() const { return tag_; }                        ///< `DATE`, `TIME`, `TIMESTAMP`, or `INTERVAL`.
    Sym sym() const { return sym_; }                             ///< The *unquoted* body of the literal.
    const Interval* interval() const { return interval_.get(); } ///< `INTERVAL` only; may be null.

    std::ostream& stream(std::ostream&) const override;

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

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
};

/*
 * Expr
 */

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
/// The trailing clauses are what turn one into an ordered-set aggregate or a window function.
class Func : public Expr {
public:
    Func(Loc loc,
         Syms&& syms,
         bool distinct,
         ASTs<Expr>&& args,
         ASTs<Order>&& withins,
         AST<Expr>&& filter,
         AST<Window>&& over)
        : Expr(loc)
        , syms_(std::move(syms))
        , distinct_(distinct)
        , args_(std::move(args))
        , withins_(std::move(withins))
        , filter_(std::move(filter))
        , over_(std::move(over)) {}

    const auto& syms() const { return syms_; } ///< The function name, possibly schema-qualified.
    bool distinct() const { return distinct_; }
    const auto& args() const { return args_; }
    const auto& withins() const { return withins_; }     ///< `WITHIN GROUP (ORDER BY ...)`
    const Expr* filter() const { return filter_.get(); } ///< `FILTER (WHERE ...)`
    const Window* over() const { return over_.get(); }   ///< `OVER (...)`

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
    bool distinct_;
    ASTs<Expr> args_;
    ASTs<Order> withins_;
    AST<Expr> filter_;
    AST<Window> over_;
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

/// `expr [NOT] LIKE pattern [ESCAPE escape]` - or `SIMILAR TO` instead of `LIKE`.
class Like : public Expr {
public:
    Like(Loc loc, AST<Expr>&& expr, AST<Expr>&& pattern, AST<Expr>&& escape, bool negated, bool similar)
        : Expr(loc)
        , expr_(std::move(expr))
        , pattern_(std::move(pattern))
        , escape_(std::move(escape))
        , negated_(negated)
        , similar_(similar) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* pattern() const { return pattern_.get(); }
    const Expr* escape() const { return escape_.get(); } ///< May be null.
    bool negated() const { return negated_; }
    bool similar() const { return similar_; } ///< `SIMILAR TO` rather than `LIKE`.

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> pattern_;
    AST<Expr> escape_;
    bool negated_;
    bool similar_;
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

/// `expr COLLATE <collation>`
class Collate : public Expr {
public:
    Collate(Loc loc, AST<Expr>&& expr, Syms&& syms)
        : Expr(loc)
        , expr_(std::move(expr))
        , syms_(std::move(syms)) {}

    const Expr* expr() const { return expr_.get(); }
    const auto& syms() const { return syms_; }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    Syms syms_;
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

/// `EXTRACT(<field> FROM expr)` - the field is kept as a Sym, so vendor fields like `epoch` work too.
class Extract : public Expr {
public:
    Extract(Loc loc, Sym field, AST<Expr>&& expr)
        : Expr(loc)
        , field_(field)
        , expr_(std::move(expr)) {}

    Sym field() const { return field_; }
    const Expr* expr() const { return expr_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Sym field_;
    AST<Expr> expr_;
};

/// `SUBSTRING(expr FROM start [FOR len])`.
/// The comma-separated `SUBSTRING(x, 1, 2)` is an ordinary Func instead.
class Substring : public Expr {
public:
    Substring(Loc loc, AST<Expr>&& expr, AST<Expr>&& from, AST<Expr>&& four)
        : Expr(loc)
        , expr_(std::move(expr))
        , from_(std::move(from))
        , four_(std::move(four)) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* from() const { return from_.get(); }
    const Expr* four() const { return four_.get(); } ///< The `FOR` length; may be null.

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> from_;
    AST<Expr> four_;
};

/// `TRIM([[LEADING|TRAILING|BOTH] [chars] FROM] expr)`
class Trim : public Expr {
public:
    Trim(Loc loc, Tok::Tag tag, AST<Expr>&& chars, AST<Expr>&& expr)
        : Expr(loc)
        , tag_(tag)
        , chars_(std::move(chars))
        , expr_(std::move(expr)) {}

    /// Tok::Tag::K_LEADING, Tok::Tag::K_TRAILING, Tok::Tag::K_BOTH, or Tok::Tag::Nil.
    Tok::Tag tag() const { return tag_; }
    const Expr* chars() const { return chars_.get(); } ///< What to trim; may be null.
    const Expr* expr() const { return expr_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag tag_;
    AST<Expr> chars_;
    AST<Expr> expr_;
};

/// `POSITION(needle IN haystack)`
class Position : public Expr {
public:
    Position(Loc loc, AST<Expr>&& needle, AST<Expr>&& haystack)
        : Expr(loc)
        , needle_(std::move(needle))
        , haystack_(std::move(haystack)) {}

    const Expr* needle() const { return needle_.get(); }
    const Expr* haystack() const { return haystack_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> needle_;
    AST<Expr> haystack_;
};

/// `OVERLAY(expr PLACING placing FROM from [FOR len])`
class Overlay : public Expr {
public:
    Overlay(Loc loc, AST<Expr>&& expr, AST<Expr>&& placing, AST<Expr>&& from, AST<Expr>&& four)
        : Expr(loc)
        , expr_(std::move(expr))
        , placing_(std::move(placing))
        , from_(std::move(from))
        , four_(std::move(four)) {}

    const Expr* expr() const { return expr_.get(); }
    const Expr* placing() const { return placing_.get(); }
    const Expr* from() const { return from_.get(); }
    const Expr* four() const { return four_.get(); } ///< The `FOR` length; may be null.

    std::ostream& stream(std::ostream&) const override;

private:
    AST<Expr> expr_;
    AST<Expr> placing_;
    AST<Expr> from_;
    AST<Expr> four_;
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

/// A quantified comparison: `lhs = ANY (subquery)`, `lhs > ALL (subquery)`.
class QuantExpr : public BinExpr {
public:
    QuantExpr(Loc loc, AST<Expr>&& lhs, Tok::Tag tag, Tok::Tag quant, AST<Expr>&& rhs)
        : BinExpr(loc, std::move(lhs), tag, std::move(rhs))
        , quant_(quant) {}

    /// Tok::Tag::K_ALL, Tok::Tag::K_ANY, or Tok::Tag::K_SOME.
    Tok::Tag quant() const { return quant_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tok::Tag quant_;
};

/*
 * Query
 */

/// A grouping element beyond a plain expression: `ROLLUP (a, b)`, `CUBE (a, b)`,
/// `GROUPING SETS ((a), ())`, or the empty grouping set `()`.
class Grouping : public Expr {
public:
    enum Tag { Rollup, Cube, Sets, Empty };

    Grouping(Loc loc, Tag tag, ASTs<Expr>&& args)
        : Expr(loc)
        , tag_(tag)
        , args_(std::move(args)) {}

    Tag tag() const { return tag_; }
    const auto& args() const { return args_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tag tag_;
    ASTs<Expr> args_;
};

/// A `VALUES` table: `VALUES (1, 'a'), (2, 'b')`. Stands on its own as a query, and is what an
/// `INSERT` without a source query carries.
class Values : public Expr {
public:
    Values(Loc loc, ASTs<Expr>&& rows)
        : Expr(loc)
        , rows_(std::move(rows)) {}

    const auto& rows() const { return rows_; }

    std::ostream& stream(std::ostream&) const override;

private:
    ASTs<Expr> rows_;
};

/// `TABLE <name>` - the explicit-table shorthand for `SELECT * FROM <name>`.
class Table : public Expr {
public:
    Table(Loc loc, Syms&& syms)
        : Expr(loc)
        , syms_(std::move(syms)) {}

    const auto& syms() const { return syms_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
};

/*
 * DDL
 */

/// `CREATE [GLOBAL|LOCAL TEMPORARY] TABLE [IF NOT EXISTS] <name> (<elems>)`, or
/// `CREATE TABLE <name> [(cols)] AS <query>`.
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

    Create(Loc loc,
           Syms&& syms,
           bool temporary,
           bool if_not_exists,
           ASTs<Elem>&& elems,
           ASTs<Constraint>&& constraints,
           AST<Expr>&& query)
        : Expr(loc)
        , syms_(std::move(syms))
        , temporary_(temporary)
        , if_not_exists_(if_not_exists)
        , elems_(std::move(elems))
        , constraints_(std::move(constraints))
        , query_(std::move(query)) {}

    const auto& syms() const { return syms_; }
    bool temporary() const { return temporary_; }
    bool if_not_exists() const { return if_not_exists_; }
    const auto& elems() const { return elems_; }
    const auto& constraints() const { return constraints_; } ///< Table-level constraints.
    const Expr* query() const { return query_.get(); }       ///< `CREATE TABLE ... AS <query>`; may be null.

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
    bool temporary_;
    bool if_not_exists_;
    ASTs<Elem> elems_;
    ASTs<Constraint> constraints_;
    AST<Expr> query_;
};

/// `CREATE [OR REPLACE] VIEW <name> [(cols)] AS <query> [WITH [CASCADED|LOCAL] CHECK OPTION]`
class CreateView : public Expr {
public:
    CreateView(Loc loc, Syms&& syms, bool replace, Syms&& cols, AST<Expr>&& query, Tok::Tag check)
        : Expr(loc)
        , syms_(std::move(syms))
        , replace_(replace)
        , cols_(std::move(cols))
        , query_(std::move(query))
        , check_(check) {}

    const auto& syms() const { return syms_; }
    bool replace() const { return replace_; }
    const auto& cols() const { return cols_; }
    const Expr* query() const { return query_.get(); }
    /// `WITH CHECK OPTION`: Tok::Tag::K_CASCADED, Tok::Tag::K_LOCAL, Tok::Tag::K_CHECK (unqualified),
    /// or Tok::Tag::Nil for no check option at all.
    Tok::Tag check() const { return check_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
    bool replace_;
    Syms cols_;
    AST<Expr> query_;
    Tok::Tag check_;
};

/// `CREATE [UNIQUE] INDEX [IF NOT EXISTS] <name> ON <table> (<cols>)`
class CreateIndex : public Expr {
public:
    CreateIndex(Loc loc, Sym sym, bool unique, bool if_not_exists, Syms&& table, ASTs<Order>&& cols)
        : Expr(loc)
        , sym_(sym)
        , unique_(unique)
        , if_not_exists_(if_not_exists)
        , table_(std::move(table))
        , cols_(std::move(cols)) {}

    Sym sym() const { return sym_; }
    bool unique() const { return unique_; }
    bool if_not_exists() const { return if_not_exists_; }
    const auto& table() const { return table_; }
    const auto& cols() const { return cols_; } ///< Index keys, each with its own `ASC`/`DESC`.

    std::ostream& stream(std::ostream&) const override;

private:
    Sym sym_;
    bool unique_;
    bool if_not_exists_;
    Syms table_;
    ASTs<Order> cols_;
};

/// `CREATE SCHEMA [IF NOT EXISTS] <name>`
class CreateSchema : public Expr {
public:
    CreateSchema(Loc loc, Syms&& syms, bool if_not_exists)
        : Expr(loc)
        , syms_(std::move(syms))
        , if_not_exists_(if_not_exists) {}

    const auto& syms() const { return syms_; }
    bool if_not_exists() const { return if_not_exists_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
    bool if_not_exists_;
};

/// `ALTER TABLE <table> <action>` - the standard allows exactly one action per statement.
/// As with Constraint, one node covers every flavor and Alter::tag says which fields carry meaning.
class Alter : public Expr {
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

    Alter(Loc loc,
          Syms&& table,
          Tag tag,
          Sym sym,
          Sym sym2,
          AST<Create::Elem>&& elem,
          AST<Constraint>&& constraint,
          AST<Type>&& type,
          AST<Expr>&& expr,
          Behavior behavior)
        : Expr(loc)
        , table_(std::move(table))
        , tag_(tag)
        , sym_(sym)
        , sym2_(sym2)
        , elem_(std::move(elem))
        , constraint_(std::move(constraint))
        , type_(std::move(type))
        , expr_(std::move(expr))
        , behavior_(behavior) {}

    const auto& table() const { return table_; }
    Tag tag() const { return tag_; }
    Sym sym() const { return sym_; }   ///< The column, constraint, or new table name.
    Sym sym2() const { return sym2_; } ///< The new column name of an Alter::Rename_Column.
    const Create::Elem* elem() const { return elem_.get(); }
    const Constraint* constraint() const { return constraint_.get(); }
    const Type* type() const { return type_.get(); }
    const Expr* expr() const { return expr_.get(); }
    Behavior behavior() const { return behavior_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms table_;
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
class Drop : public Expr {
public:
    enum Tag { Table, View, Index, Schema };

    Drop(Loc loc, Tag tag, Syms&& syms, bool if_exists, Behavior behavior)
        : Expr(loc)
        , tag_(tag)
        , syms_(std::move(syms))
        , if_exists_(if_exists)
        , behavior_(behavior) {}

    Tag tag() const { return tag_; }
    const auto& syms() const { return syms_; }
    bool if_exists() const { return if_exists_; }
    Behavior behavior() const { return behavior_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Tag tag_;
    Syms syms_;
    bool if_exists_;
    Behavior behavior_;
};

/// `TRUNCATE TABLE <name>`
class Truncate : public Expr {
public:
    Truncate(Loc loc, Syms&& syms)
        : Expr(loc)
        , syms_(std::move(syms)) {}

    const auto& syms() const { return syms_; }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
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

    std::ostream& stream(std::ostream&) const override;

private:
    Tag tag_;
    Sym sym_;
};

/*
 * Query expressions
 */

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
        From(Loc loc, bool lateral, AST<Expr>&& expr, bool ordinality, Sym as, Syms&& cols)
            : Node(loc)
            , lateral_(lateral)
            , expr_(std::move(expr))
            , ordinality_(ordinality)
            , as_(as)
            , cols_(std::move(cols)) {}

        bool lateral() const { return lateral_; }
        const Expr* expr() const { return expr_.get(); }
        bool ordinality() const { return ordinality_; } ///< `WITH ORDINALITY` of an `UNNEST`.
        Sym as() const { return as_; }
        const auto& cols() const { return cols_; }

        std::ostream& stream(std::ostream&) const override;

    private:
        bool lateral_;
        AST<Expr> expr_;
        bool ordinality_;
        Sym as_;
        Syms cols_;
    };

    /// One entry of the `WINDOW` clause: `<name> AS (<window>)`.
    class WindowDef : public Node {
    public:
        WindowDef(Loc loc, Sym sym, AST<Window>&& window)
            : Node(loc)
            , sym_(sym)
            , window_(std::move(window)) {}

        Sym sym() const { return sym_; }
        const Window* window() const { return window_.get(); }

        std::ostream& stream(std::ostream&) const override;

    private:
        Sym sym_;
        AST<Window> window_;
    };

    Select(Loc loc,
           bool all,
           ASTs<Elem>&& elems,
           ASTs<From>&& froms,
           AST<Expr>&& where,
           ASTs<Expr>&& groups,
           AST<Expr>&& having,
           ASTs<WindowDef>&& windows)
        : Expr(loc)
        , all_(all)
        , elems_(std::move(elems))
        , froms_(std::move(froms))
        , where_(std::move(where))
        , groups_(std::move(groups))
        , having_(std::move(having))
        , windows_(std::move(windows)) {}

    bool all() const { return all_; }
    bool distinct() const { return !all_; }
    const auto& elems() const { return elems_; }
    const auto& froms() const { return froms_; } ///< May be empty: `SELECT 1` has no `FROM`.
    const Expr* where() const { return where_.get(); }
    const auto& groups() const { return groups_; }
    const Expr* having() const { return having_.get(); }
    const auto& windows() const { return windows_; }

    std::ostream& stream(std::ostream&) const override;

private:
    bool all_;
    ASTs<Elem> elems_;
    ASTs<From> froms_;
    AST<Expr> where_;
    ASTs<Expr> groups_;
    AST<Expr> having_;
    ASTs<WindowDef> windows_;
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

/// A query body wrapped in its `WITH` clause and its trailing `ORDER BY` / `OFFSET` / `FETCH` /
/// `LIMIT` clauses. Only constructed when at least one of them is present.
class Query : public Expr {
public:
    /// One common table expression: `<name> [(cols)] AS (<query>)`.
    class Cte : public Node {
    public:
        Cte(Loc loc, Sym sym, Syms&& cols, AST<Expr>&& query)
            : Node(loc)
            , sym_(sym)
            , cols_(std::move(cols))
            , query_(std::move(query)) {}

        Sym sym() const { return sym_; }
        const auto& cols() const { return cols_; }
        const Expr* query() const { return query_.get(); }

        std::ostream& stream(std::ostream&) const override;

    private:
        Sym sym_;
        Syms cols_;
        AST<Expr> query_;
    };

    Query(Loc loc,
          bool recursive,
          ASTs<Cte>&& ctes,
          AST<Expr>&& body,
          ASTs<Order>&& orders,
          AST<Expr>&& offset,
          AST<Expr>&& fetch,
          AST<Expr>&& limit)
        : Expr(loc)
        , recursive_(recursive)
        , ctes_(std::move(ctes))
        , body_(std::move(body))
        , orders_(std::move(orders))
        , offset_(std::move(offset))
        , fetch_(std::move(fetch))
        , limit_(std::move(limit)) {}

    bool recursive() const { return recursive_; }
    const auto& ctes() const { return ctes_; }
    const Expr* body() const { return body_.get(); }
    const auto& orders() const { return orders_; }
    const Expr* offset() const { return offset_.get(); }
    const Expr* fetch() const { return fetch_.get(); }
    const Expr* limit() const { return limit_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    bool recursive_;
    ASTs<Cte> ctes_;
    AST<Expr> body_;
    ASTs<Order> orders_;
    AST<Expr> offset_;
    AST<Expr> fetch_;
    AST<Expr> limit_;
};

/*
 * Insert / Update / Delete
 */

/// `INSERT INTO <table> [(cols)] <query>` where the query is usually a Values table, or
/// `INSERT INTO <table> DEFAULT VALUES`.
class Insert : public Expr {
public:
    Insert(Loc loc, Syms&& syms, Syms&& cols, AST<Expr>&& query)
        : Expr(loc)
        , syms_(std::move(syms))
        , cols_(std::move(cols))
        , query_(std::move(query)) {}

    const auto& syms() const { return syms_; }
    const auto& cols() const { return cols_; }
    const Expr* query() const { return query_.get(); } ///< Null for `DEFAULT VALUES`.

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
    Syms cols_;
    AST<Expr> query_;
};

/// `UPDATE <table> [AS <as>] SET <assigns> [WHERE <where>]`
class Update : public Expr {
public:
    /// One `<column> = <expr>` of the `SET` clause.
    class Assign : public Node {
    public:
        Assign(Loc loc, Syms&& syms, AST<Expr>&& expr)
            : Node(loc)
            , syms_(std::move(syms))
            , expr_(std::move(expr)) {}

        const auto& syms() const { return syms_; } ///< The target column, possibly qualified.
        const Expr* expr() const { return expr_.get(); }

        std::ostream& stream(std::ostream&) const override;

    private:
        Syms syms_;
        AST<Expr> expr_;
    };

    Update(Loc loc, Syms&& syms, Sym as, ASTs<Assign>&& assigns, AST<Expr>&& where)
        : Expr(loc)
        , syms_(std::move(syms))
        , as_(as)
        , assigns_(std::move(assigns))
        , where_(std::move(where)) {}

    const auto& syms() const { return syms_; }
    Sym as() const { return as_; }
    const auto& assigns() const { return assigns_; }
    const Expr* where() const { return where_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
    Sym as_;
    ASTs<Assign> assigns_;
    AST<Expr> where_;
};

/// `DELETE FROM <table> [AS <as>] [WHERE <where>]`
class Delete : public Expr {
public:
    Delete(Loc loc, Syms&& syms, Sym as, AST<Expr>&& where)
        : Expr(loc)
        , syms_(std::move(syms))
        , as_(as)
        , where_(std::move(where)) {}

    const auto& syms() const { return syms_; }
    Sym as() const { return as_; }
    const Expr* where() const { return where_.get(); }

    std::ostream& stream(std::ostream&) const override;

private:
    Syms syms_;
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
