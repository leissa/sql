#include <cctype>
#include <cstdlib>

#include <iostream>
#include <print>
#include <ranges>
#include <string>

#include "sql/ast.h"

namespace sql {

/// Streams an identifier so that it lexes back to the same Sym: the Lexer folds unquoted
/// identifiers to lower case, so anything else has to go out as a delimited identifier.
struct Ident {
    Ident(Sym sym)
        : sym(sym) {}

    Sym sym;

    friend std::ostream& operator<<(std::ostream& o, Ident ident) {
        auto sv    = *ident.sym;
        auto plain = !sv.empty() && !std::isdigit((unsigned char)sv.front());
        for (auto c : sv)
            plain &= c == '_' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');

        if (plain) return o << sv;

        o << '"';
        for (auto c : sv) {
            if (c == '"') o << '"';
            o << c;
        }
        return o << '"';
    }
};

/// Streams a string literal, doubling the delimiter the way SQL escapes it.
struct Str {
    Str(Sym sym)
        : sym(sym) {}

    Sym sym;

    friend std::ostream& operator<<(std::ostream& o, Str str) {
        o << '\'';
        for (auto c : *str.sym) {
            if (c == '\'') o << '\'';
            o << c;
        }
        return o << '\'';
    }
};

} // namespace sql

#ifndef DOXYGEN
// clang-format off
template<> struct std::formatter<sql::Ident> : fe::ostream_formatter {};
template<> struct std::formatter<sql::Str>   : fe::ostream_formatter {};
// clang-format on
#endif

namespace sql {

/// Wraps @p syms as Ident%s - use as `fe::Join` argument.
static auto idents(const Syms& syms) {
    return syms | std::views::transform([](Sym sym) { return Ident(sym); });
}

/// A possibly qualified name such as `s.t` - no parentheses, unlike parens() below.
static auto qname(const Syms& syms) { return fe::Join(idents(syms), "."); }

/// A parenthesized list of Node%s preceded by @p prefix - or nothing at all if @p range is empty.
template<std::ranges::input_range R>
static auto parens(const R& range, std::string_view prefix = "") {
    return fe::StreamFn{[&range, prefix](std::ostream& o) {
        if (!range.empty()) std::print(o, "{}({})", prefix, fe::Join(range));
    }};
}

/// Same, but for a list of identifiers such as a column list.
static auto parens(const Syms& syms, std::string_view prefix = "") {
    return fe::StreamFn{[&syms, prefix](std::ostream& o) {
        if (!syms.empty()) std::print(o, "{}({})", prefix, fe::Join(idents(syms)));
    }};
}

void Node::dump() const {
    stream(std::cout);
    std::cout << std::endl;
}

/*
 * Interval
 */

void Interval::stream(std::ostream& o) const {
    std::print(o, "{}{}", from(), parens(from_args()));
    if (to() != Tok::Tag::Nil) std::print(o, " TO {}{}", to(), parens(to_args()));
}

/*
 * Type
 */

void SimpleType::stream(std::ostream& o) const {
    // Bare `DOUBLE` is not a type name of its own - the standard spells it `DOUBLE PRECISION`.
    std::print(o, "{}{}", tag(), tag() == Tok::Tag::K_DOUBLE ? " PRECISION" : "");
    if (varying()) o << " VARYING";
    o << parens(args());
    if (interval()) std::print(o, " {}", *interval());
    if (zone() != Tok::Tag::Nil) std::print(o, " {} TIME ZONE", zone());
    if (not_null()) o << " NOT NULL";
}

void NamedType::stream(std::ostream& o) const {
    std::print(o, "{}{}", Ident(sym()), parens(args()));
    if (not_null()) o << " NOT NULL";
}

/*
 * Order / Window
 */

void Order::stream(std::ostream& o) const {
    std::print(o, "{}{}", *expr(), desc() ? " DESC" : " ASC");

    // clang-format off
    switch (nulls()) {
        case Nulls_First: o << " NULLS FIRST"; break;
        case Nulls_Last:  o << " NULLS LAST";  break;
        case Nulls_None:                       break;
    }
    // clang-format on
}

void Frame::Bound::stream(std::ostream& o) const {
    // clang-format off
    switch (tag()) {
        case Unbounded_Preceding: o << "UNBOUNDED PRECEDING";             break;
        case Unbounded_Following: o << "UNBOUNDED FOLLOWING";             break;
        case Current_Row:         o << "CURRENT ROW";                     break;
        case Preceding:           std::print(o, "{} PRECEDING", *expr()); break;
        case Following:           std::print(o, "{} FOLLOWING", *expr()); break;
    }
    // clang-format on
}

void Frame::stream(std::ostream& o) const {
    if (hi())
        std::print(o, "{} BETWEEN {} AND {}", unit(), *lo(), *hi());
    else
        std::print(o, "{} {}", unit(), *lo());

    // clang-format off
    switch (exclude()) {
        case Exclude_Current_Row: o << " EXCLUDE CURRENT ROW"; break;
        case Exclude_Group:       o << " EXCLUDE GROUP";       break;
        case Exclude_Ties:        o << " EXCLUDE TIES";        break;
        case Exclude_No_Others:   o << " EXCLUDE NO OTHERS";   break;
        case Exclude_None:                                     break;
    }
    // clang-format on
}

void Window::stream(std::ostream& o) const {
    // `OVER w` refers to a window defined elsewhere and takes no parentheses.
    if (!paren()) {
        o << Ident(name());
        return;
    }

    o << '(';
    auto sep = "";
    if (name()) {
        o << Ident(name());
        sep = " ";
    }

    if (!partitions().empty()) {
        std::print(o, "{}PARTITION BY {}", sep, fe::Join(partitions()));
        sep = " ";
    }

    if (!orders().empty()) {
        std::print(o, "{}ORDER BY {}", sep, fe::Join(orders()));
        sep = " ";
    }

    if (frame()) std::print(o, "{}{}", sep, *frame());
    o << ')';
}

/*
 * Constraint
 */

/// Streams the referential action of an `ON DELETE`/`ON UPDATE` clause, if there is one.
static void stream_action(std::ostream& o, std::string_view when, Constraint::Action action) {
    // clang-format off
    switch (action) {
        case Constraint::No_Action:   std::print(o, " ON {} NO ACTION",   when); break;
        case Constraint::Restrict:    std::print(o, " ON {} RESTRICT",    when); break;
        case Constraint::Cascade:     std::print(o, " ON {} CASCADE",     when); break;
        case Constraint::Set_Null:    std::print(o, " ON {} SET NULL",    when); break;
        case Constraint::Set_Default: std::print(o, " ON {} SET DEFAULT", when); break;
        case Constraint::Action_None:                                            break;
    }
    // clang-format on
}

void Constraint::stream(std::ostream& o) const {
    if (name()) std::print(o, "CONSTRAINT {} ", Ident(name()));

    switch (tag()) {
        case Primary_Key: std::print(o, "PRIMARY KEY{}", parens(cols(), " ")); break;
        case Unique: std::print(o, "UNIQUE{}", parens(cols(), " ")); break;
        case Check: std::print(o, "CHECK ({})", *expr()); break;
        case Default: std::print(o, "DEFAULT {}", *expr()); break;
        case References: std::print(o, "REFERENCES {}{}", qname(table()), parens(ref_cols(), " ")); break;
        case Foreign_Key:
            std::print(o, "FOREIGN KEY ({}) REFERENCES {}{}", fe::Join(idents(cols())), qname(table()),
                       parens(ref_cols(), " "));
            break;
    }

    stream_action(o, "DELETE", on_delete());
    stream_action(o, "UPDATE", on_update());
}

/*
 * Val
 */

double RealVal::f64() const { return std::strtod(std::string(*sym()).c_str(), nullptr); }

// clang-format off
void ErrExpr  ::stream(std::ostream& o) const { o << "<error expression>"; }
void SimpleVal::stream(std::ostream& o) const { o << tag(); }
void IntVal   ::stream(std::ostream& o) const { o << u64(); }
void RealVal  ::stream(std::ostream& o) const { o << *sym(); }
void StrVal   ::stream(std::ostream& o) const { o << Str(sym()); }
void Param    ::stream(std::ostream& o) const { o << *sym(); }
void Table    ::stream(std::ostream& o) const { o << "TABLE " << qname(syms()); }
void Truncate ::stream(std::ostream& o) const { o << "TRUNCATE TABLE " << qname(syms()); }
// clang-format on

void TypedVal::stream(std::ostream& o) const {
    std::print(o, "{} {}", tag(), Str(sym()));
    if (interval()) std::print(o, " {}", *interval());
}

/*
 * Expr
 */

void Id::stream(std::ostream& o) const {
    o << qname(syms());
    if (asterisk()) o << ".*";
}

void UnExpr::stream(std::ostream& o) const { std::print(o, "({} {})", tag(), *rhs()); }

void Func::stream(std::ostream& o) const {
    std::print(o, "{}({}{})", qname(syms()), distinct() ? "DISTINCT " : "", fe::Join(args()));
    if (!withins().empty()) std::print(o, " WITHIN GROUP (ORDER BY {})", fe::Join(withins()));
    if (filter()) std::print(o, " FILTER (WHERE {})", *filter());
    if (over()) std::print(o, " OVER {}", *over());
}

void Between::stream(std::ostream& o) const {
    std::print(o, "({}{} BETWEEN {} AND {})", *expr(), negated() ? " NOT" : "", *lo(), *hi());
}

void Like::stream(std::ostream& o) const {
    std::print(o, "({}{}{}{}", *expr(), negated() ? " NOT" : "", similar() ? " SIMILAR TO " : " LIKE ", *pattern());
    if (escape()) std::print(o, " ESCAPE {}", *escape());
    o << ')';
}

void Cast::stream(std::ostream& o) const { std::print(o, "CAST({} AS {})", *expr(), *type()); }
void Collate::stream(std::ostream& o) const { std::print(o, "({} COLLATE {})", *expr(), qname(syms())); }

void CaseExpr::stream(std::ostream& o) const {
    o << "CASE";
    if (operand()) std::print(o, " {}", *operand());
    for (const auto& when : whens())
        std::print(o, " {}", when);
    if (elze()) std::print(o, " ELSE {}", *elze());
    o << " END";
}

void CaseExpr::When::stream(std::ostream& o) const { std::print(o, "WHEN {} THEN {}", *cond(), *then()); }
void Extract::stream(std::ostream& o) const { std::print(o, "EXTRACT({} FROM {})", Ident(field()), *expr()); }

void Substring::stream(std::ostream& o) const {
    std::print(o, "SUBSTRING({} FROM {}", *expr(), *from());
    if (four()) std::print(o, " FOR {}", *four());
    o << ')';
}

void Trim::stream(std::ostream& o) const {
    o << "TRIM(";
    if (tag() != Tok::Tag::Nil) std::print(o, "{} ", tag());
    if (chars()) std::print(o, "{} ", *chars());
    if (tag() != Tok::Tag::Nil || chars()) o << "FROM ";
    std::print(o, "{})", *expr());
}

void Position::stream(std::ostream& o) const { std::print(o, "POSITION({} IN {})", *needle(), *haystack()); }

void Overlay::stream(std::ostream& o) const {
    std::print(o, "OVERLAY({} PLACING {} FROM {}", *expr(), *placing(), *from());
    if (four()) std::print(o, " FOR {}", *four());
    o << ')';
}

void ParenExprList::stream(std::ostream& o) const { std::print(o, "({})", fe::Join(args())); }
void BinExpr::stream(std::ostream& o) const { std::print(o, "({} {} {})", *lhs(), tag(), *rhs()); }
void QuantExpr::stream(std::ostream& o) const { std::print(o, "({} {} {} {})", *lhs(), tag(), quant(), *rhs()); }

void BinExprWithPreTag::stream(std::ostream& o) const {
    std::print(o, "({} {} {} {})", *lhs(), pretag(), tag(), *rhs());
}

void Grouping::stream(std::ostream& o) const {
    // clang-format off
    switch (tag()) {
        case Rollup: o << "ROLLUP ";        break;
        case Cube:   o << "CUBE ";          break;
        case Sets:   o << "GROUPING SETS "; break;
        case Empty:  o << "()";             return;
    }
    // clang-format on
    std::print(o, "({})", fe::Join(args()));
}

void Values::stream(std::ostream& o) const { o << "VALUES " << fe::Join(rows()); }

/*
 * DDL
 */

/// Streams a `CASCADE`/`RESTRICT` drop behavior, if there is one.
static void stream_behavior(std::ostream& o, Behavior behavior) {
    // clang-format off
    switch (behavior) {
        case Behavior::Cascade:  o << " CASCADE";  break;
        case Behavior::Restrict: o << " RESTRICT"; break;
        case Behavior::None:                       break;
    }
    // clang-format on
}

void Create::stream(std::ostream& o) const {
    std::print(o, "CREATE {}TABLE {}{}", temporary() ? "TEMPORARY " : "", if_not_exists() ? "IF NOT EXISTS " : "",
               qname(syms()));

    if (query()) {
        std::print(o, " AS {}", *query());
        return;
    }

    // The table elements and the table-level constraints form one comma-separated list.
    auto sep = elems().empty() || constraints().empty() ? "" : ", ";
    std::print(o, " ({}{}{})", fe::Join(elems()), sep, fe::Join(constraints()));
}

void Create::Elem::stream(std::ostream& o) const {
    std::print(o, "{} {}", Ident(sym()), *type());
    for (auto&& constraint : constraints())
        std::print(o, " {}", constraint);
}

void CreateView::stream(std::ostream& o) const {
    std::print(o, "CREATE {}VIEW {}{} AS {}", replace() ? "OR REPLACE " : "", qname(syms()), parens(cols(), " "),
               *query());

    if (check() != Tok::Tag::Nil) {
        o << " WITH ";
        if (check() != Tok::Tag::K_CHECK) std::print(o, "{} ", check());
        o << "CHECK OPTION";
    }
}

void CreateIndex::stream(std::ostream& o) const {
    std::print(o, "CREATE {}INDEX {}{}", unique() ? "UNIQUE " : "", if_not_exists() ? "IF NOT EXISTS " : "",
               Ident(sym()));
    std::print(o, " ON {} ({})", qname(table()), fe::Join(cols()));
}

void CreateSchema::stream(std::ostream& o) const {
    std::print(o, "CREATE SCHEMA {}{}", if_not_exists() ? "IF NOT EXISTS " : "", qname(syms()));
}

void Alter::stream(std::ostream& o) const {
    std::print(o, "ALTER TABLE {}", qname(table()));

    switch (tag()) {
        case Add_Column: std::print(o, " ADD COLUMN {}", *elem()); break;
        case Add_Constraint: std::print(o, " ADD {}", *constraint()); break;
        case Drop_Column: std::print(o, " DROP COLUMN {}", Ident(sym())); break;
        case Drop_Constraint: std::print(o, " DROP CONSTRAINT {}", Ident(sym())); break;
        case Set_Default: std::print(o, " ALTER COLUMN {} SET DEFAULT {}", Ident(sym()), *expr()); break;
        case Drop_Default: std::print(o, " ALTER COLUMN {} DROP DEFAULT", Ident(sym())); break;
        case Set_Not_Null: std::print(o, " ALTER COLUMN {} SET NOT NULL", Ident(sym())); break;
        case Drop_Not_Null: std::print(o, " ALTER COLUMN {} DROP NOT NULL", Ident(sym())); break;
        case Set_Data_Type: std::print(o, " ALTER COLUMN {} SET DATA TYPE {}", Ident(sym()), *type()); break;
        case Rename_Table: std::print(o, " RENAME TO {}", Ident(sym())); break;
        case Rename_Column: std::print(o, " RENAME COLUMN {} TO {}", Ident(sym()), Ident(sym2())); break;
    }

    stream_behavior(o, behavior()); // only a dropped column/constraint has one
}

void Drop::stream(std::ostream& o) const {
    o << "DROP ";
    // clang-format off
    switch (tag()) {
        case Table:  o << "TABLE ";  break;
        case View:   o << "VIEW ";   break;
        case Index:  o << "INDEX ";  break;
        case Schema: o << "SCHEMA "; break;
    }
    // clang-format on
    if (if_exists()) o << "IF EXISTS ";
    o << qname(syms());
    stream_behavior(o, behavior());
}

void Transact::stream(std::ostream& o) const {
    // clang-format off
    switch (tag()) {
        case Start:       o << "START TRANSACTION";                               break;
        case Commit:      o << "COMMIT";                                          break;
        case Rollback:    o << "ROLLBACK";                                        break;
        case Rollback_To: std::print(o, "ROLLBACK TO SAVEPOINT {}", Ident(sym())); break;
        case Savepoint:   std::print(o, "SAVEPOINT {}", Ident(sym()));            break;
        case Release:     std::print(o, "RELEASE SAVEPOINT {}", Ident(sym()));    break;
    }
    // clang-format on
}

/*
 * Query expressions
 */

void Join::stream(std::ostream& o) const {
    o << '(' << *lhs();

    if (tag() == Cross) {
        o << " CROSS";
    } else {
        if (tag() & Natural) o << " NATURAL";
        // Full is Left|Right, so mask first - a plain Left would match `& Full` too.
        // clang-format off
        switch (tag() & Full) {
            case Full:  o << " FULL";  break;
            case Left:  o << " LEFT";  break;
            case Right: o << " RIGHT"; break;
            default:    o << " INNER"; break;
        }
        // clang-format on
    }
    std::print(o, " JOIN {}", *rhs());

    if (auto on = std::get_if<On>(&spec()))
        std::print(o, " ON {}", *on);
    else if (auto syms = std::get_if<Using>(&spec()))
        std::print(o, "{}", parens(*syms, " USING "));
    o << ')';
}

void Select::stream(std::ostream& o) const {
    std::print(o, "SELECT {}", distinct() ? "DISTINCT " : "");

    // A `SELECT *` keeps no elements of its own - the asterisk is all there is.
    if (elems().empty())
        o << '*';
    else
        o << fe::Join(elems());

    // A `SELECT` without a `FROM` is fine - `SELECT 1` needs no table to compute its value.
    if (!froms().empty()) std::print(o, " FROM {}", fe::Join(froms()));
    if (where()) std::print(o, " WHERE {}", *where());
    if (!groups().empty()) std::print(o, " GROUP BY {}", fe::Join(groups()));
    if (having()) std::print(o, " HAVING {}", *having());
    if (!windows().empty()) std::print(o, " WINDOW {}", fe::Join(windows()));
}

void Select::Elem::stream(std::ostream& o) const {
    o << *expr();
    switch (syms().size()) {
        case 0: break;
        case 1: std::print(o, " AS {}", Ident(syms().front())); break;
        default: std::print(o, "{}", parens(syms(), " AS "));
    }
}

void Select::From::stream(std::ostream& o) const {
    std::print(o, "{}{}", lateral() ? "LATERAL " : "", *expr());
    if (ordinality()) o << " WITH ORDINALITY";
    if (as()) std::print(o, " AS {}", Ident(as()));
    o << parens(cols(), " ");
}

void Select::WindowDef::stream(std::ostream& o) const { std::print(o, "{} AS {}", Ident(sym()), *window()); }

/// Like Select and Query - and unlike the self-parenthesizing Expr%essions - this prints *without*
/// parentheses: a ParenExprList preserves the ones that actually group in the source.
void SetOp::stream(std::ostream& o) const {
    o << *lhs();

    // clang-format off
    switch (tag()) {
        case Union:     o << " UNION";     break;
        case Intersect: o << " INTERSECT"; break;
        case Except:    o << " EXCEPT";    break;
    }
    // clang-format on
    if (all()) o << " ALL";

    std::print(o, " {}", *rhs());
}

void Query::stream(std::ostream& o) const {
    if (!ctes().empty()) std::print(o, "WITH {}{} ", recursive() ? "RECURSIVE " : "", fe::Join(ctes()));

    o << *body();

    if (!orders().empty()) std::print(o, " ORDER BY {}", fe::Join(orders()));
    if (offset()) std::print(o, " OFFSET {} ROWS", *offset());
    if (fetch()) std::print(o, " FETCH NEXT {} ROWS ONLY", *fetch());
    if (limit()) std::print(o, " LIMIT {}", *limit());
}

void Query::Cte::stream(std::ostream& o) const {
    std::print(o, "{}{} AS ({})", Ident(sym()), parens(cols(), " "), *query());
}

/*
 * Insert / Update / Delete
 */

void Insert::stream(std::ostream& o) const {
    std::print(o, "INSERT INTO {}{}", qname(syms()), parens(cols(), " "));

    if (query())
        std::print(o, " {}", *query());
    else
        o << " DEFAULT VALUES";
}

void Update::stream(std::ostream& o) const {
    std::print(o, "UPDATE {}", qname(syms()));
    if (as()) std::print(o, " AS {}", Ident(as()));

    std::print(o, " SET {}", fe::Join(assigns()));
    if (where()) std::print(o, " WHERE {}", *where());
}

void Update::Assign::stream(std::ostream& o) const { std::print(o, "{} = {}", qname(syms()), *expr()); }

void Delete::stream(std::ostream& o) const {
    std::print(o, "DELETE FROM {}", qname(syms()));
    if (as()) std::print(o, " AS {}", Ident(as()));
    if (where()) std::print(o, " WHERE {}", *where());
}

/*
 * Misc
 */

void Prog::stream(std::ostream& o) const {
    for (auto sep = ""; auto&& expr : exprs()) {
        std::print(o, "{}{};", sep, expr);
        sep = "\n";
    }
}

} // namespace sql
