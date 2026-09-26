#include "sql/parser.h"

#include "fe/term.h"

using namespace std::literals;

// clang-format off
/// @name Token Families
/// Each of these lists a whole family of Tok::Tag%s as `case` labels. The leading `Tok::Tag::` comes from
/// the use site, so a family reads exactly like a single label there: `case Tok::Tag::C_FIELD:`.
///@{
/// The datetime fields an interval qualifier is made of.
#define C_FIELD              \
                   K_YEAR:   \
    case Tok::Tag::K_MONTH:  \
    case Tok::Tag::K_DAY:    \
    case Tok::Tag::K_HOUR:   \
    case Tok::Tag::K_MINUTE: \
    case Tok::Tag::K_SECOND

/// The datetime types - exactly those that may carry a typed literal like `DATE '2024-01-01'`.
#define C_TYPED_VAL             \
                   K_DATE:      \
    case Tok::Tag::K_TIME:      \
    case Tok::Tag::K_TIMESTAMP: \
    case Tok::Tag::K_INTERVAL

/// A type spelled with a reserved word - as opposed to a NamedType like `text` or `jsonb`.
#define C_TYPE                  \
                   C_TYPED_VAL: \
    case Tok::Tag::K_INT:       \
    case Tok::Tag::K_INTEGER:   \
    case Tok::Tag::K_SMALLINT:  \
    case Tok::Tag::K_BIGINT:    \
    case Tok::Tag::K_BOOLEAN:   \
    case Tok::Tag::K_REAL:      \
    case Tok::Tag::K_DOUBLE:    \
    case Tok::Tag::K_FLOAT:     \
    case Tok::Tag::K_BLOB:      \
    case Tok::Tag::K_CLOB:      \
    case Tok::Tag::K_NUMERIC:   \
    case Tok::Tag::K_DECIMAL:   \
    case Tok::Tag::K_DEC:       \
    case Tok::Tag::K_CHAR:      \
    case Tok::Tag::K_CHARACTER: \
    case Tok::Tag::K_VARCHAR:   \
    case Tok::Tag::K_BINARY:    \
    case Tok::Tag::K_VARBINARY

/// A value that is nothing but its Tok::Tag - `*` included, as that is what `COUNT(*)` counts.
#define C_VAL                 \
                   T_mul:     \
    case Tok::Tag::K_TRUE:    \
    case Tok::Tag::K_FALSE:   \
    case Tok::Tag::K_UNKNOWN: \
    case Tok::Tag::K_DEFAULT: \
    case Tok::Tag::K_NULL

/// Only these may carry a quantifier: `a = ANY (...)` is a comparison, `a IN (...)` is not.
#define C_COMP           \
                   T_eq: \
    case Tok::Tag::T_ne: \
    case Tok::Tag::T_ue: \
    case Tok::Tag::T_l:  \
    case Tok::Tag::T_le: \
    case Tok::Tag::T_g:  \
    case Tok::Tag::T_ge

/// The quantifier of a quantified comparison.
#define C_QUANT           \
                   K_ALL: \
    case Tok::Tag::K_ANY: \
    case Tok::Tag::K_SOME

/// `SIMILAR` only differs from `LIKE` by its `TO`; Parser::parse_like handles both.
#define C_LIKE             \
                   K_LIKE: \
    case Tok::Tag::K_SIMILAR

/// The functions the standard spells with keyword-separated arguments rather than commas.
#define C_SPECIAL_FUNC          \
                   K_EXTRACT:   \
    case Tok::Tag::K_SUBSTRING: \
    case Tok::Tag::K_TRIM:      \
    case Tok::Tag::K_POSITION:  \
    case Tok::Tag::K_OVERLAY

/// Which end of a `TRIM` to trim.
#define C_TRIM_SPEC            \
                   K_LEADING:  \
    case Tok::Tag::K_TRAILING: \
    case Tok::Tag::K_BOTH

/// The unit a window frame counts in.
#define C_FRAME_UNIT        \
                   K_ROWS:  \
    case Tok::Tag::K_RANGE: \
    case Tok::Tag::K_GROUPS

/// Does a `CREATE TABLE` element start a table-level constraint rather than a column definition?
#define C_TABLE_CONSTRAINT       \
                   K_CONSTRAINT: \
    case Tok::Tag::K_PRIMARY:    \
    case Tok::Tag::K_UNIQUE:     \
    case Tok::Tag::K_FOREIGN:    \
    case Tok::Tag::K_CHECK

/// As above, plus what only makes sense on a single column.
#define C_COL_CONSTRAINT               \
                   C_TABLE_CONSTRAINT: \
    case Tok::Tag::K_REFERENCES:       \
    case Tok::Tag::K_DEFAULT

/// What a query expression can start with. In the `FROM` clause this is what tells a derived table
/// from a merely parenthesized table reference - a `(` followed by any of these opens a query.
#define C_QUERY              \
                   K_SELECT: \
    case Tok::Tag::K_VALUES: \
    case Tok::Tag::K_TABLE:  \
    case Tok::Tag::K_WITH

/// One `<SQL transaction statement>` - all of them go to Parser::parse_transact.
#define C_TRANSACT              \
                   K_BEGIN:     \
    case Tok::Tag::K_START:     \
    case Tok::Tag::K_COMMIT:    \
    case Tok::Tag::K_ROLLBACK:  \
    case Tok::Tag::K_SAVEPOINT: \
    case Tok::Tag::K_RELEASE
///@}
// clang-format on

/// Turns such a family into a predicate - a `case` label is of no use outside of a `switch`.
#define ISA(tag, family)                        \
    ([&] {                                      \
        switch (tag) {                          \
            case Tok::Tag::family: return true; \
            default: return false;              \
        }                                       \
    }())

namespace sql {

using enum NonKey;

/// The spelling of each non-reserved word, indexed by NonKey - for diagnostics.
static constexpr std::string_view Non_Key_Strs[] = {
#define CODE(t, str) str##sv,
    SQL_NON_KEY(CODE)
#undef CODE
};

Parser::Parser(Driver& driver, const fe::Src& src)
    : lexer_(driver, src)
    , sym_error_(driver.sym_error())
    , non_keys_(driver.non_keys()) {
    init();
}

/*
 * misc
 */

bool Parser::isa_non_key(NonKey nk) const { return ahead().isa(Tok::Tag::V_id) && ahead().sym() == non_key(nk); }

bool Parser::accept_non_key(NonKey nk) {
    if (!isa_non_key(nk)) return false;
    lex();
    return true;
}

void Parser::expect_non_key(NonKey nk, fe::Cite ctxt) {
    if (!accept_non_key(nk)) syntax_err(fe::format_cite("`{}`", Non_Key_Strs[(size_t)nk]), ctxt);
}

bool Parser::isa_sym() const { return ahead().isa(Tok::Tag::V_id) || ahead().isa_key(); }

/// Is the token @p i ahead the operator of a Like expression - `LIKE`, `ILIKE`, or `SIMILAR`?
bool Parser::isa_like(size_t i) const {
    if (ISA(ahead(i).tag(), C_LIKE)) return true;
    return ahead(i).isa(Tok::Tag::V_id) && ahead(i).sym() == non_key(N_ILIKE);
}

Sym Parser::parse_sym(fe::Cite ctxt) {
    if (ahead().isa(Tok::Tag::V_id)) return lex().sym();
    if (ahead().isa_key()) return driver().sym(to_lower(Tok::tag2str(lex().tag())));
    syntax_err("identifier", ctxt);
    return sym_error_;
}

Syms Parser::parse_name(fe::Cite ctxt) {
    Syms syms;
    syms.emplace_back(parse_sym(ctxt));
    while (accept(Tok::Tag::T_dot))
        syms.emplace_back(parse_sym(ctxt));
    return syms;
}

void Parser::parse_col_list(fe::Cite ctxt, Syms& syms) {
    parse_list(ctxt, [&]() { syms.emplace_back(parse_sym("column name")); });
}

Behavior Parser::parse_behavior() {
    if (accept_non_key(N_CASCADE)) return Behavior::Cascade;
    if (accept_non_key(N_RESTRICT)) return Behavior::Restrict;
    return Behavior::None;
}

bool Parser::parse_if_exists(bool not_) {
    if (!isa_non_key(N_IF)) return false;
    lex();
    if (not_) expect(Tok::Tag::K_NOT, "`IF NOT EXISTS` clause");
    expect(Tok::Tag::K_EXISTS, "`IF EXISTS` clause");
    return true;
}

/*
 * Prog
 */

AST<Prog> Parser::parse_prog() {
    auto track = tracker();
    ASTs<Expr> exprs;

    while (!ahead().isa(Tok::Tag::EoF)) {
        // A stray `;` is an empty statement - skip it instead of inventing a statement for it.
        if (accept(Tok::Tag::T_semicolon)) continue;

        // The `;` closes the statement no matter how badly it went, so nothing nested may swallow it.
        auto _ = anchor(Tok::Tag::T_semicolon);
        exprs.emplace_back(parse_stmt());
        // Whatever is left before the `;` is bogus; discarding it also prevents an endless loop.
        recover([](Tok::Tag tag) { return tag != Tok::Tag::T_semicolon && tag != Tok::Tag::EoF; }, "statement list");
        expect(Tok::Tag::T_semicolon, "statement list");
    }

    eat(Tok::Tag::EoF);
    return ast<Prog>(track, exprs);
}

/// One `<direct SQL statement>`: a schema, data, or transaction statement - or a query expression.
/// Note that this is *not* a value expression: `1 + 2` is a perfectly good Expr but no statement.
AST<Expr> Parser::parse_stmt() {
    // clang-format off
    switch (ahead().tag()) {
        case Tok::Tag::K_CREATE:   return parse_create();
        case Tok::Tag::K_ALTER:    return parse_alter();
        case Tok::Tag::K_DROP:     return parse_drop();
        case Tok::Tag::K_TRUNCATE: return parse_truncate();
        case Tok::Tag::K_INSERT:   return parse_insert();
        case Tok::Tag::K_UPDATE:   return parse_update();
        case Tok::Tag::K_DELETE:   return parse_delete();
        case Tok::Tag::C_TRANSACT: return parse_transact();
        case Tok::Tag::K_PREPARE:  return parse_prepare();
        case Tok::Tag::K_EXECUTE:  return parse_execute();
        case Tok::Tag::K_DEALLOCATE: return parse_deallocate();
        case Tok::Tag::K_DESCRIBE: return parse_show();
        default:
            if (isa_non_key(N_SHOW)) return parse_show();
            if (isa_non_key(N_COPY)) return parse_copy();
            return parse_query("statement", false);
    }
    // clang-format on
}

/*
 * Type
 */

AST<Interval> Parser::parse_interval() {
    auto track = tracker();

    auto parse_args = [&](ASTs<Expr>& args) {
        if (ahead().isa(Tok::Tag::D_paren_l))
            parse_list("precision of an interval qualifier",
                       [&]() { args.emplace_back(parse_expr("precision of an interval qualifier")); });
    };

    auto from = lex().tag();
    ASTs<Expr> from_args, to_args;
    parse_args(from_args);

    auto to = Tok::Tag::Nil;
    if (accept(Tok::Tag::K_TO)) {
        if (ISA(ahead().tag(), C_FIELD)) {
            to = lex().tag();
            parse_args(to_args);
        } else {
            syntax_err("datetime field", "`TO` of an interval qualifier");
        }
    }

    return ast<Interval>(track, from, to, from_args, to_args);
}

AST<Type> Parser::parse_type(fe::Cite ctxt) {
    auto track = tracker();

    // Optional trailing `NOT NULL` - a plain `NULL` explicitly spells out the default.
    auto parse_not_null = [&]() {
        if (accept(Tok::Tag::K_NOT)) {
            expect(Tok::Tag::K_NULL, "`NOT NULL` constraint");
            return true;
        }
        accept(Tok::Tag::K_NULL);
        return false;
    };

    // Optional parenthesized arguments: a length, or a precision/scale pair.
    auto parse_args = [&](ASTs<Expr>& args) {
        if (ahead().isa(Tok::Tag::D_paren_l))
            parse_list("type argument list", [&]() { args.emplace_back(parse_expr("argument of a type")); });
    };

    switch (ahead().tag()) {
        case Tok::Tag::C_TYPE: {
            auto tag = lex().tag();
            if (accept(Tok::Tag::K_LARGE)) {
                parse_sym("`LARGE OBJECT` type"); // OBJECT is not a reserved word
                // `CHARACTER LARGE OBJECT` and `BINARY LARGE OBJECT` just spell out CLOB and BLOB.
                tag = tag == Tok::Tag::K_BINARY ? Tok::Tag::K_BLOB : Tok::Tag::K_CLOB;
            }
            accept(Tok::Tag::K_PRECISION); // DOUBLE PRECISION
            auto varying = (bool)accept(Tok::Tag::K_VARYING);
            ASTs<Expr> args;
            parse_args(args);

            AST<Interval> interval;
            if (tag == Tok::Tag::K_INTERVAL && ISA(ahead().tag(), C_FIELD)) interval = parse_interval();

            auto zone = Tok::Tag::Nil;
            if (ahead().isa(Tok::Tag::K_WITH) || ahead().isa(Tok::Tag::K_WITHOUT)) {
                zone = lex().tag();
                expect(Tok::Tag::K_TIME, "`TIME ZONE` of a type");
                expect_non_key(N_ZONE, "`TIME ZONE` of a type");
            }

            return ast<SimpleType>(track, tag, varying, zone, interval, parse_not_null(), args);
        }
        default: break;
    }

    // Anything not a reserved word - `text`, `uuid`, `jsonb`, ... - is a type name just the same.
    if (ahead().isa(Tok::Tag::V_id)) {
        auto sym = lex().sym();
        ASTs<Expr> args;
        parse_args(args);
        return ast<NamedType>(track, sym, parse_not_null(), args);
    }

    if (!ctxt.empty()) {
        syntax_err("type", ctxt);
        return nullptr; // Error Type
    }

    return nullptr;
}

/*
 * Expr
 */

/// `expr [NOT] BETWEEN lo AND hi`.
/// Both bounds parse above Tok::Prec::And so that the separating `AND` terminates the lower one
/// instead of being swallowed as a conjunction.
AST<Expr> Parser::parse_between(Tracker track, AST<Expr> lhs, bool negated) {
    eat(Tok::Tag::K_BETWEEN);
    auto lo = parse_expr("lower bound of a `BETWEEN` expression", Tok::Prec::Not);
    expect(Tok::Tag::K_AND, "`BETWEEN` expression");
    auto hi = parse_expr("upper bound of a `BETWEEN` expression", Tok::Prec::Not);
    return ast<Between>(track, lhs, lo, hi, negated);
}

/// `expr [NOT] LIKE pattern [ESCAPE escape]`, with `ILIKE` or `SIMILAR TO` in place of `LIKE`.
AST<Expr> Parser::parse_like(Tracker track, AST<Expr> lhs, bool negated) {
    // `ILIKE` is no reserved word, so it arrives as a plain identifier rather than as a Tok::Tag.
    auto tag = isa_non_key(N_ILIKE) ? Tok::Tag::K_ILIKE : ahead().tag();
    lex();
    if (tag == Tok::Tag::K_SIMILAR) expect(Tok::Tag::K_TO, "`SIMILAR TO` expression");

    // Above Tok::Prec::Comp: `a LIKE b = c` groups as `(a LIKE b) = c`, not `a LIKE (b = c)`.
    auto prec    = (Tok::Prec)((int)Tok::Prec::Comp + 1);
    auto pattern = parse_expr("pattern of a `LIKE` expression", prec);
    AST<Expr> escape;
    if (accept(Tok::Tag::K_ESCAPE)) escape = parse_expr("`ESCAPE` clause of a `LIKE` expression", prec);

    return ast<Like>(track, lhs, pattern, escape, negated, tag);
}

AST<Expr> Parser::parse_expr(fe::Cite ctxt, Tok::Prec cur_prec) {
    auto track = tracker();
    auto lhs   = parse_primary_or_unary_expr(ctxt);

    // All binary operators here are left-associative, so the right-hand side must bind strictly tighter.
    auto next_prec = [](Tok::Prec prec) { return (Tok::Prec)((int)prec + 1); };

    while (true) {
        if (ahead().isa(Tok::Tag::K_NOT)) {
            // `ILIKE` has no Tok::Prec of its own - having no Tok::Tag either, it ranks with `LIKE`.
            auto prec = isa_like(1) ? Tok::bin_prec(Tok::Tag::K_LIKE) : Tok::bin_prec(ahead(1).tag());
            if (!prec || *prec < cur_prec) break;
            eat(Tok::Tag::K_NOT);

            if (ahead().isa(Tok::Tag::K_BETWEEN)) {
                lhs = parse_between(track, lhs, true);
            } else if (isa_like()) {
                lhs = parse_like(track, lhs, true);
            } else {
                auto tag = lex().tag();
                auto rhs = parse_expr("right-hand side of binary expression with `NOT` in front of operator",
                                      next_prec(*prec));
                lhs      = ast<BinExprWithPreTag>(track, lhs, Tok::Tag::K_NOT, tag, rhs);
            }
        } else if (ahead().isa(Tok::Tag::K_BETWEEN)) {
            if (*Tok::bin_prec(Tok::Tag::K_BETWEEN) < cur_prec) break;
            lhs = parse_between(track, lhs, false);
        } else if (isa_like()) {
            if (*Tok::bin_prec(Tok::Tag::K_LIKE) < cur_prec) break;
            lhs = parse_like(track, lhs, false);
        } else if (ahead().isa(Tok::Tag::K_COLLATE)) {
            // Postfix and tighter than anything else, so it never needs to look at what follows.
            if (Tok::Prec::Unary < cur_prec) break;
            eat(Tok::Tag::K_COLLATE);
            auto name = parse_name("collation name");
            lhs       = ast<Collate>(track, lhs, name);
        } else if (ahead().isa(Tok::Tag::K_IS)) {
            if (*Tok::bin_prec(Tok::Tag::K_IS) < cur_prec) break;
            eat(Tok::Tag::K_IS);
            bool negated = (bool)accept(Tok::Tag::K_NOT);

            auto tag = negated ? Tok::Tag::K_IS_NOT : Tok::Tag::K_IS;
            if (accept(Tok::Tag::K_DISTINCT)) {
                expect(Tok::Tag::K_FROM, "`IS DISTINCT FROM` expression");
                tag = negated ? Tok::Tag::K_IS_NOT_DISTINCT_FROM : Tok::Tag::K_IS_DISTINCT_FROM;
            }

            auto rhs = parse_expr("right-hand side of an `IS` expression", next_prec(Tok::Prec::Comp));
            lhs      = ast<BinExpr>(track, lhs, tag, rhs);
        } else if (auto prec = Tok::bin_prec(ahead().tag())) {
            if (*prec < cur_prec) break;

            auto op = lex().tag();

            // A quantifier turns a comparison into `a > ALL (subquery)`; the `(` tells it from a
            // column that merely happens to be named after the reserved word.
            if (ISA(op, C_COMP) && ISA(ahead().tag(), C_QUANT) && ahead(1).isa(Tok::Tag::D_paren_l)) {
                auto quant = lex().tag();
                auto rhs   = parse_expr("subquery of a quantified comparison", next_prec(*prec));
                lhs        = ast<QuantExpr>(track, lhs, op, quant, rhs);
            } else {
                auto rhs = parse_expr("right-hand side of binary expression", next_prec(*prec));
                lhs      = ast<BinExpr>(track, lhs, op, rhs);
            }
        } else {
            break;
        }
    }

    return lhs;
}

AST<Expr> Parser::parse_primary_or_unary_expr(fe::Cite ctxt) {
    switch (ahead().tag()) {
        case Tok::Tag::K_CASE: return parse_case();
        case Tok::Tag::K_CAST: return parse_cast();
        case Tok::Tag::V_int: {
            auto tok = lex();
            return ast<IntVal>(tok.loc(), tok.u64());
        }
        case Tok::Tag::V_real: {
            auto tok = lex();
            return ast<RealVal>(tok.loc(), tok.sym());
        }
        case Tok::Tag::V_str: {
            auto tok = lex();
            return ast<StrVal>(tok.loc(), tok.sym());
        }
        case Tok::Tag::V_param: {
            auto tok = lex();
            return ast<Param>(tok.loc(), tok.sym());
        }
        case Tok::Tag::C_VAL: {
            auto tok = lex();
            return ast<SimpleVal>(tok.loc(), tok.tag());
        }
        case Tok::Tag::C_SPECIAL_FUNC:
            if (ahead(1).isa(Tok::Tag::D_paren_l)) return parse_special_func();
            break;
        default: break;
    }

    auto track = tracker();

    // A typed literal: `DATE '2024-01-01'`, `INTERVAL '1-2' YEAR TO MONTH`. The string tells it
    // from the very same word used as a type name or as a column reference.
    if (ISA(ahead().tag(), C_TYPED_VAL) && ahead(1).isa(Tok::Tag::V_str)) {
        auto tag = lex().tag();
        auto str = lex().sym();
        AST<Interval> interval;
        if (tag == Tok::Tag::K_INTERVAL && ISA(ahead().tag(), C_FIELD)) interval = parse_interval();
        return ast<TypedVal>(track, tag, str, interval);
    }

    // `ARRAY[a, b]`; a `(` instead makes it the ordinary call `array(a, b)`.
    if (ahead().isa(Tok::Tag::K_ARRAY) && ahead(1).isa(Tok::Tag::D_brckt_l)) {
        eat(Tok::Tag::K_ARRAY);
        ASTs<Expr> args;
        parse_list(
            "array value constructor", [&]() { args.emplace_back(parse_expr("element of an array")); },
            Tok::Tag::D_brckt_l);
        return ast<Array>(track, args);
    }

    // Before the call below, so that `NOT (...)` stays an operator rather than a function.
    if (auto prec = Tok::un_prec(ahead().tag())) {
        auto op = lex().tag();
        return ast<UnExpr>(track, op, parse_expr("operand of unary expression", *prec));
    }

    // `EXISTS (subquery)` - a unary operator whose operand happens to be parenthesized.
    if (ahead().isa(Tok::Tag::K_EXISTS)) {
        auto op = lex().tag();
        return ast<UnExpr>(track, op, parse_expr("operand of `EXISTS` expression", Tok::Prec::Unary));
    }

    // A reserved word directly followed by a `.` can only be the qualifier of a reference - no clause
    // keyword is ever followed by one - so `at.movie_id` resolves even though `AT` is reserved.
    // The same goes for a `(`: whatever the name, `name(...)` is a call.
    if (ahead().isa(Tok::Tag::V_id)
        || (isa_sym() && (ahead(1).isa(Tok::Tag::T_dot) || ahead(1).isa(Tok::Tag::D_paren_l))))
        return parse_id_or_func();

    if (ahead().isa(Tok::Tag::D_paren_l)) {
        ASTs<Expr> args;
        parse_list("parenthesized expression list",
                   [&]() { args.emplace_back(parse_query("parenthesized expression")); });

        // Parentheses around a query are structural - they are what makes it a subquery. Around a
        // single scalar expression they are pure grouping, and keeping a node for them would make
        // them pile up with every dump/re-parse round trip.
        if (args.size() == 1) {
            const auto* arg = args.front().get();
            if (!arg->isa<Select>() && !arg->isa<SetOp>() && !arg->isa<Query>() && !arg->isa<Values>()
                && !arg->isa<Table>())
                return args.front();
        }

        return ast<ParenExprList>(track, args);
    }

    if (!ctxt.empty()) {
        syntax_err("primary or unary expression", ctxt);
        return ast<ErrExpr>(curr_);
    }
    fe::unreachable();
}

AST<Expr> Parser::parse_id_or_func() {
    auto track = tracker();

    bool asterisk = false;
    Syms syms;
    syms.emplace_back(parse_sym("identifier"));

    while (accept(Tok::Tag::T_dot)) {
        if (accept(Tok::Tag::T_mul)) {
            asterisk = true;
            break;
        }
        syms.emplace_back(parse_sym("identifer chain"));
    }

    auto expr
        = !asterisk && ahead().isa(Tok::Tag::D_paren_l) ? parse_func(track, syms) : ast<Id>(track, asterisk, syms);

    // `a[i]`, and `a[i][j]` for the nested ones. Binding it here rather than in the operator loop is
    // what keeps it tighter than the unary operators: `- a[1]` negates the element, not the array.
    while (ahead().isa(Tok::Tag::D_brckt_l)) {
        eat(Tok::Tag::D_brckt_l);
        auto _     = anchor(Tok::Tag::D_brckt_r);
        auto index = parse_expr("index of a subscript");
        expect(Tok::Tag::D_brckt_r, "closing delimiter of a subscript");
        expr = ast<Subscript>(track, expr, index);
    }

    return expr;
}

/// The argument list of a call plus whatever trailing clauses turn it into an ordered-set aggregate
/// or a window function. The name has already been parsed by Parser::parse_id_or_func.
AST<Expr> Parser::parse_func(Tracker track, const Syms& syms) {
    bool distinct = false;
    ASTs<Expr> args;
    {
        expect(Tok::Tag::D_paren_l, "function argument list");
        auto _   = anchor(Tok::Tag::D_paren_r);
        distinct = (bool)accept(Tok::Tag::K_DISTINCT);
        if (!distinct) accept(Tok::Tag::K_ALL);

        parse_seq(
            "function argument list", [&]() { args.emplace_back(parse_expr("argument of function")); },
            Tok::Tag::D_paren_r);
        expect(Tok::Tag::D_paren_r, "closing delimiter of a function argument list");
    }

    ASTs<Order> withins;
    if (accept(Tok::Tag::K_WITHIN)) {
        expect(Tok::Tag::K_GROUP, "`WITHIN GROUP` clause");
        expect(Tok::Tag::D_paren_l, "`WITHIN GROUP` clause");
        auto _ = anchor(Tok::Tag::D_paren_r);
        expect(Tok::Tag::K_ORDER, "`WITHIN GROUP` clause");
        expect(Tok::Tag::K_BY, "`WITHIN GROUP` clause");
        do
            withins.emplace_back(parse_order());
        while (accept(Tok::Tag::T_comma));
        expect(Tok::Tag::D_paren_r, "closing delimiter of a `WITHIN GROUP` clause");
    }

    AST<Expr> filter;
    if (accept(Tok::Tag::K_FILTER)) {
        expect(Tok::Tag::D_paren_l, "`FILTER` clause");
        auto _ = anchor(Tok::Tag::D_paren_r);
        expect(Tok::Tag::K_WHERE, "`FILTER` clause");
        filter = parse_expr("search condition of a `FILTER` clause");
        expect(Tok::Tag::D_paren_r, "closing delimiter of a `FILTER` clause");
    }

    AST<Window> over;
    if (accept(Tok::Tag::K_OVER)) over = parse_window();

    return ast<Func>(track, distinct, filter, over, syms, args, withins);
}

AST<Expr> Parser::parse_special_func() {
    auto track = tracker();
    auto tag   = lex().tag();
    // Above Tok::Prec::Comp, so the `IN` of a `POSITION` is a separator and not an operator.
    auto tight = (Tok::Prec)((int)Tok::Prec::Comp + 1);

    expect(Tok::Tag::D_paren_l, "`{}` expression", Tok::tag2str(tag));
    auto _ = anchor(Tok::Tag::D_paren_r);

    auto close = [&](auto&& node) {
        expect(Tok::Tag::D_paren_r, "closing delimiter of a `{}` expression", Tok::tag2str(tag));
        return node;
    };

    switch (tag) {
        case Tok::Tag::K_EXTRACT: {
            auto field = parse_sym("field of an `EXTRACT` expression");
            expect(Tok::Tag::K_FROM, "`EXTRACT` expression");
            auto expr = parse_expr("operand of an `EXTRACT` expression");
            return close(ast<Extract>(track, field, expr));
        }
        case Tok::Tag::K_POSITION: {
            auto needle = parse_expr("needle of a `POSITION` expression", tight);
            expect(Tok::Tag::K_IN, "`POSITION` expression");
            auto haystack = parse_expr("haystack of a `POSITION` expression");
            return close(ast<Position>(track, needle, haystack));
        }
        case Tok::Tag::K_OVERLAY: {
            auto expr = parse_expr("operand of an `OVERLAY` expression");
            expect_non_key(N_PLACING, "`OVERLAY` expression");
            auto placing = parse_expr("replacement of an `OVERLAY` expression");
            expect(Tok::Tag::K_FROM, "`OVERLAY` expression");
            auto from = parse_expr("start of an `OVERLAY` expression");
            AST<Expr> four;
            if (accept(Tok::Tag::K_FOR)) four = parse_expr("length of an `OVERLAY` expression");
            return close(ast<Overlay>(track, expr, placing, from, four));
        }
        case Tok::Tag::K_TRIM: {
            auto spec = Tok::Tag::Nil;
            if (ISA(ahead().tag(), C_TRIM_SPEC)) spec = lex().tag();

            AST<Expr> chars, expr;
            if (accept(Tok::Tag::K_FROM)) {
                expr = parse_expr("operand of a `TRIM` expression");
            } else {
                // Without a `FROM` there is nothing to trim away - what was parsed *is* the operand.
                auto first = parse_expr("operand of a `TRIM` expression");
                if (accept(Tok::Tag::K_FROM)) {
                    chars = first;
                    expr  = parse_expr("operand of a `TRIM` expression");
                } else {
                    expr = first;
                }
            }
            return close(ast<Trim>(track, spec, chars, expr));
        }
        case Tok::Tag::K_SUBSTRING: {
            auto expr = parse_expr("operand of a `SUBSTRING` expression");
            if (accept(Tok::Tag::K_FROM)) {
                auto from = parse_expr("start of a `SUBSTRING` expression");
                AST<Expr> four;
                if (accept(Tok::Tag::K_FOR)) four = parse_expr("length of a `SUBSTRING` expression");
                return close(ast<Substring>(track, expr, from, four));
            }

            // The comma-separated `SUBSTRING(x, 1, 2)` is just an ordinary call.
            ASTs<Expr> args;
            args.emplace_back(expr);
            while (accept(Tok::Tag::T_comma))
                args.emplace_back(parse_expr("argument of a `SUBSTRING` expression"));

            Syms syms;
            syms.emplace_back(driver().sym(to_lower(Tok::tag2str(tag))));
            return close(ast<Func>(track, false, AST<Expr>{}, AST<Window>{}, syms, args, ASTs<Order>{}));
        }
        default: fe::unreachable();
    }
}

AST<Expr> Parser::parse_cast() {
    auto track = tracker();
    eat(Tok::Tag::K_CAST);

    expect(Tok::Tag::D_paren_l, "`CAST` expression");
    auto _    = anchor(Tok::Tag::D_paren_r);
    auto expr = parse_expr("operand of a `CAST` expression");
    expect(Tok::Tag::K_AS, "`CAST` expression");
    auto type = parse_type("target type of a `CAST` expression");
    expect(Tok::Tag::D_paren_r, "closing delimiter of a `CAST` expression");

    return ast<Cast>(track, expr, type);
}

AST<Expr> Parser::parse_case() {
    auto track = tracker();
    eat(Tok::Tag::K_CASE);
    auto _ = anchor(Tok::Tag::K_END);

    // `CASE WHEN ...` is the searched form and has no operand.
    AST<Expr> operand;
    if (!ahead().isa(Tok::Tag::K_WHEN)) operand = parse_expr("operand of a `CASE` expression");

    ASTs<CaseExpr::When> whens;
    do {
        auto when_track = tracker();
        eat(Tok::Tag::K_WHEN);
        auto cond = parse_expr("condition of a `WHEN` clause");
        expect(Tok::Tag::K_THEN, "`WHEN` clause of a `CASE` expression");
        auto then = parse_expr("result of a `WHEN` clause");
        whens.emplace_back(ast<CaseExpr::When>(when_track, cond, then));
    } while (ahead().isa(Tok::Tag::K_WHEN));

    AST<Expr> elze;
    if (accept(Tok::Tag::K_ELSE)) elze = parse_expr("`ELSE` clause of a `CASE` expression");
    expect(Tok::Tag::K_END, "`CASE` expression");

    return ast<CaseExpr>(track, operand, elze, whens);
}

/*
 * Constraint
 */

Constraint::Action Parser::parse_ref_action() {
    if (accept(Tok::Tag::K_NO)) {
        expect_non_key(N_ACTION, "`NO ACTION` referential action");
        return Constraint::No_Action;
    }
    if (accept_non_key(N_RESTRICT)) return Constraint::Restrict;
    if (accept_non_key(N_CASCADE)) return Constraint::Cascade;
    if (accept(Tok::Tag::K_SET)) {
        if (accept(Tok::Tag::K_NULL)) return Constraint::Set_Null;
        expect(Tok::Tag::K_DEFAULT, "`SET DEFAULT` referential action");
        return Constraint::Set_Default;
    }

    syntax_err("referential action", "`ON DELETE`/`ON UPDATE` clause");
    return Constraint::Action_None;
}

/// A column- or table-level constraint; @p table_level ones carry their own column list.
AST<Constraint> Parser::parse_constraint(bool table_level) {
    auto track = tracker();
    Sym name;
    if (accept(Tok::Tag::K_CONSTRAINT)) name = parse_sym("constraint name");

    auto tag = Constraint::Primary_Key;
    Syms cols;
    Syms table;
    Syms ref_cols;
    AST<Expr> expr;

    if (accept(Tok::Tag::K_PRIMARY)) {
        tag = Constraint::Primary_Key;
        accept_non_key(N_KEY); // KEY is not reserved
        if (table_level) parse_col_list("primary key column list", cols);
    } else if (accept(Tok::Tag::K_UNIQUE)) {
        tag = Constraint::Unique;
        if (table_level) parse_col_list("unique column list", cols);
    } else if (accept(Tok::Tag::K_FOREIGN)) {
        tag = Constraint::Foreign_Key;
        accept_non_key(N_KEY);
        parse_col_list("foreign key column list", cols);
        expect(Tok::Tag::K_REFERENCES, "`FOREIGN KEY` constraint");
        table = parse_name("referenced table name");
        if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("referenced column list", ref_cols);
    } else if (accept(Tok::Tag::K_REFERENCES)) {
        tag   = Constraint::References;
        table = parse_name("referenced table name");
        if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("referenced column list", ref_cols);
    } else if (accept(Tok::Tag::K_CHECK)) {
        tag = Constraint::Check;
        expect(Tok::Tag::D_paren_l, "`CHECK` constraint");
        auto _ = anchor(Tok::Tag::D_paren_r);
        expr   = parse_expr("search condition of a `CHECK` constraint");
        expect(Tok::Tag::D_paren_r, "closing delimiter of a `CHECK` constraint");
    } else if (accept(Tok::Tag::K_DEFAULT)) {
        tag  = Constraint::Default;
        expr = parse_expr("default value");
    } else {
        syntax_err("constraint", fe::Cite(table_level ? "table constraint" : "column constraint"));
    }

    // The referential actions of a foreign key, in either order.
    auto on_delete = Constraint::Action_None, on_update = Constraint::Action_None;
    if (tag == Constraint::References || tag == Constraint::Foreign_Key) {
        while (accept(Tok::Tag::K_ON)) {
            if (accept(Tok::Tag::K_DELETE)) {
                on_delete = parse_ref_action();
            } else {
                expect(Tok::Tag::K_UPDATE, "referential triggered action");
                on_update = parse_ref_action();
            }
        }
    }

    return ast<Constraint>(track, name, tag, expr, on_delete, on_update, cols, table, ref_cols);
}

/*
 * DDL
 */

AST<Create::Elem> Parser::parse_col_def() {
    auto track = tracker();
    auto sym   = parse_sym("column name");
    auto type  = parse_type("column type");
    ASTs<Constraint> constraints;
    while (ISA(ahead().tag(), C_COL_CONSTRAINT))
        constraints.emplace_back(parse_constraint(false));
    return ast<Create::Elem>(track, sym, type, constraints);
}

AST<Expr> Parser::parse_create() {
    auto track = tracker();
    eat(Tok::Tag::K_CREATE);

    bool replace = false;
    if (accept(Tok::Tag::K_OR)) {
        expect_non_key(N_REPLACE, "`CREATE OR REPLACE`");
        replace = true;
    }

    // `GLOBAL`/`LOCAL` only ever qualify a `TEMPORARY`, and nothing downstream cares which.
    if (ahead().isa(Tok::Tag::K_GLOBAL) || ahead().isa(Tok::Tag::K_LOCAL)) lex();
    bool temporary = accept_non_key(N_TEMPORARY);

    if (accept_non_key(N_VIEW)) return parse_create_view(track, replace);
    if (accept_non_key(N_INDEX)) return parse_create_index(track, false);
    if (accept_non_key(N_SCHEMA)) return parse_create_schema(track);
    if (accept(Tok::Tag::K_UNIQUE)) {
        expect_non_key(N_INDEX, "`CREATE UNIQUE INDEX` expression");
        return parse_create_index(track, true);
    }

    expect(Tok::Tag::K_TABLE, "`CREATE` expression");
    return parse_create_table(track, temporary);
}

AST<Expr> Parser::parse_create_table(Tracker track, bool temporary) {
    bool if_not_exists = parse_if_exists(true);
    auto syms          = parse_name("table name");

    ASTs<Create::Elem> elems;
    ASTs<Constraint> constraints;

    // `CREATE TABLE t AS <query>` takes its shape from the query instead of from a table element list.
    AST<Expr> query;
    if (accept(Tok::Tag::K_AS)) {
        query = parse_query("source of a `CREATE TABLE` expression", false);
    } else {
        parse_list("table element list", [&]() {
            if (ISA(ahead().tag(), C_TABLE_CONSTRAINT))
                constraints.emplace_back(parse_constraint(true));
            else
                elems.emplace_back(parse_col_def());
        });
    }

    return ast<Create>(track, temporary, if_not_exists, query, syms, elems, constraints);
}

AST<Expr> Parser::parse_create_view(Tracker track, bool replace) {
    auto syms = parse_name("view name");
    Syms cols;
    if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("column name list of a `CREATE VIEW` expression", cols);
    expect(Tok::Tag::K_AS, "`CREATE VIEW` expression");
    auto query = parse_query("body of a `CREATE VIEW` expression", false);

    // `WITH [CASCADED|LOCAL] CHECK OPTION` - K_CHECK records the unqualified form.
    auto check = Tok::Tag::Nil;
    if (accept(Tok::Tag::K_WITH)) {
        check = Tok::Tag::K_CHECK;
        if (ahead().isa(Tok::Tag::K_CASCADED) || ahead().isa(Tok::Tag::K_LOCAL)) check = lex().tag();
        expect(Tok::Tag::K_CHECK, "`WITH CHECK OPTION` of a `CREATE VIEW` expression");
        expect_non_key(N_OPTION, "`WITH CHECK OPTION` of a `CREATE VIEW` expression");
    }

    return ast<CreateView>(track, replace, query, check, syms, cols);
}

AST<Expr> Parser::parse_create_index(Tracker track, bool unique) {
    bool if_not_exists = parse_if_exists(true);
    auto sym           = parse_sym("index name");
    expect(Tok::Tag::K_ON, "`CREATE INDEX` expression");
    auto table = parse_name("table name");

    ASTs<Order> cols;
    parse_list("index column list", [&]() { cols.emplace_back(parse_order()); });

    return ast<CreateIndex>(track, sym, unique, if_not_exists, table, cols);
}

AST<Expr> Parser::parse_create_schema(Tracker track) {
    bool if_not_exists = parse_if_exists(true);
    auto syms          = parse_name("schema name");
    return ast<CreateSchema>(track, if_not_exists, syms);
}

AST<Expr> Parser::parse_alter() {
    auto track = tracker();
    eat(Tok::Tag::K_ALTER);
    expect(Tok::Tag::K_TABLE, "`ALTER` expression");
    bool if_table_exists = parse_if_exists(false);
    auto table           = parse_name("table name");

    auto tag = Alter::Add_Column;
    Sym sym, sym2;
    AST<Create::Elem> elem;
    AST<Constraint> constraint;
    AST<Type> type;
    AST<Expr> expr;
    auto behavior  = Behavior::None;
    bool if_exists = false;

    if (accept_non_key(N_ADD)) {
        if (ISA(ahead().tag(), C_TABLE_CONSTRAINT)) {
            tag        = Alter::Add_Constraint;
            constraint = parse_constraint(true);
        } else {
            accept(Tok::Tag::K_COLUMN);
            tag  = Alter::Add_Column;
            elem = parse_col_def();
        }
    } else if (accept(Tok::Tag::K_DROP)) {
        if (accept(Tok::Tag::K_CONSTRAINT)) {
            tag       = Alter::Drop_Constraint;
            if_exists = parse_if_exists(false);
            sym       = parse_sym("constraint name");
        } else {
            accept(Tok::Tag::K_COLUMN);
            tag       = Alter::Drop_Column;
            if_exists = parse_if_exists(false);
            sym       = parse_sym("column name");
        }
        behavior = parse_behavior();
    } else if (accept(Tok::Tag::K_ALTER)) {
        accept(Tok::Tag::K_COLUMN);
        sym = parse_sym("column name");

        if (accept(Tok::Tag::K_SET)) {
            if (accept(Tok::Tag::K_DEFAULT)) {
                tag  = Alter::Set_Default;
                expr = parse_expr("default value");
            } else if (accept(Tok::Tag::K_NOT)) {
                expect(Tok::Tag::K_NULL, "`SET NOT NULL` of an `ALTER TABLE` expression");
                tag = Alter::Set_Not_Null;
            } else {
                expect_non_key(N_DATA, "`SET DATA TYPE` of an `ALTER TABLE` expression");
                expect_non_key(N_TYPE, "`SET DATA TYPE` of an `ALTER TABLE` expression");
                tag  = Alter::Set_Data_Type;
                type = parse_type("target type of an `ALTER TABLE` expression");
            }
        } else {
            expect(Tok::Tag::K_DROP, "column alteration of an `ALTER TABLE` expression");
            if (accept(Tok::Tag::K_NOT)) {
                expect(Tok::Tag::K_NULL, "`DROP NOT NULL` of an `ALTER TABLE` expression");
                tag = Alter::Drop_Not_Null;
            } else {
                expect(Tok::Tag::K_DEFAULT, "`DROP DEFAULT` of an `ALTER TABLE` expression");
                tag = Alter::Drop_Default;
            }
        }
    } else if (accept_non_key(N_RENAME)) {
        if (accept(Tok::Tag::K_TO)) {
            tag = Alter::Rename_Table;
            sym = parse_sym("new table name");
        } else {
            accept(Tok::Tag::K_COLUMN);
            tag = Alter::Rename_Column;
            sym = parse_sym("column name");
            expect(Tok::Tag::K_TO, "`RENAME COLUMN` of an `ALTER TABLE` expression");
            sym2 = parse_sym("new column name");
        }
    } else {
        syntax_err("`ADD`, `DROP`, `ALTER`, or `RENAME`", "`ALTER TABLE` expression");
    }

    return ast<Alter>(track, tag, sym, sym2, elem, constraint, type, expr, behavior, if_table_exists, if_exists, table);
}

AST<Expr> Parser::parse_drop() {
    auto track = tracker();
    eat(Tok::Tag::K_DROP);

    auto tag = Drop::Table;
    if (accept(Tok::Tag::K_TABLE))
        tag = Drop::Table;
    else if (accept_non_key(N_VIEW))
        tag = Drop::View;
    else if (accept_non_key(N_INDEX))
        tag = Drop::Index;
    else if (accept_non_key(N_SCHEMA))
        tag = Drop::Schema;
    else
        expect(Tok::Tag::K_TABLE, "`DROP` expression");

    bool if_exists = parse_if_exists(false);
    auto syms      = parse_name("name of a `DROP` expression");
    return ast<Drop>(track, tag, if_exists, parse_behavior(), syms);
}

AST<Expr> Parser::parse_truncate() {
    auto track = tracker();
    eat(Tok::Tag::K_TRUNCATE);
    accept(Tok::Tag::K_TABLE);
    auto syms = parse_name("table name");
    return ast<Truncate>(track, syms);
}

AST<Expr> Parser::parse_transact() {
    auto track = tracker();

    // `WORK` and `TRANSACTION` are noise words - `BEGIN`, `BEGIN WORK`, and `START TRANSACTION`
    // are one and the same statement.
    auto noise = [&]() {
        if (!accept_non_key(N_TRANSACTION)) accept_non_key(N_WORK);
    };

    switch (ahead().tag()) {
        case Tok::Tag::K_BEGIN:
        case Tok::Tag::K_START:
            lex();
            noise();
            return ast<Transact>(track, Transact::Start, Sym());
        case Tok::Tag::K_COMMIT:
        case Tok::Tag::K_ROLLBACK: {
            bool rollback = ahead().isa(Tok::Tag::K_ROLLBACK);
            lex();
            noise();
            if (rollback && accept(Tok::Tag::K_TO)) {
                accept(Tok::Tag::K_SAVEPOINT);
                auto sym = parse_sym("savepoint name");
                return ast<Transact>(track, Transact::Rollback_To, sym);
            }
            return ast<Transact>(track, rollback ? Transact::Rollback : Transact::Commit, Sym());
        }
        case Tok::Tag::K_SAVEPOINT: {
            lex();
            auto sym = parse_sym("savepoint name");
            return ast<Transact>(track, Transact::Savepoint, sym);
        }
        case Tok::Tag::K_RELEASE: {
            lex();
            accept(Tok::Tag::K_SAVEPOINT);
            auto sym = parse_sym("savepoint name");
            return ast<Transact>(track, Transact::Release, sym);
        }
        default: fe::unreachable();
    }
}

/*
 * Insert / Update / Delete
 */

AST<Expr> Parser::parse_insert() {
    auto track = tracker();
    eat(Tok::Tag::K_INSERT);
    expect(Tok::Tag::K_INTO, "`INSERT` expression");
    auto syms = parse_name("table name");

    // A parenthesized list here is the column list; without it the source follows directly.
    Syms cols;
    if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("insert column list", cols);

    // `DEFAULT VALUES` inserts one row of nothing but defaults - hence no source at all.
    AST<Expr> query;
    if (accept(Tok::Tag::K_DEFAULT))
        expect(Tok::Tag::K_VALUES, "`DEFAULT VALUES` of an `INSERT` expression");
    else
        query = parse_query("source of an `INSERT` expression", false);

    return ast<Insert>(track, query, syms, cols);
}

AST<Expr> Parser::parse_update() {
    auto track = tracker();
    eat(Tok::Tag::K_UPDATE);
    auto syms = parse_name("table name");
    Sym as;
    if (accept(Tok::Tag::K_AS)) as = parse_sym("`AS` clause");
    expect(Tok::Tag::K_SET, "`UPDATE` expression");

    ASTs<Update::Assign> assigns;
    do {
        auto assign_track = tracker();
        auto col          = parse_name("column name of a `SET` clause");
        expect(Tok::Tag::T_eq, "assignment of a `SET` clause");
        auto expr = parse_expr("value of a `SET` clause");
        assigns.emplace_back(ast<Update::Assign>(assign_track, expr, col));
    } while (accept(Tok::Tag::T_comma));

    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("`WHERE` expression") : nullptr;
    return ast<Update>(track, as, where, syms, assigns);
}

AST<Expr> Parser::parse_delete() {
    auto track = tracker();
    eat(Tok::Tag::K_DELETE);
    expect(Tok::Tag::K_FROM, "`DELETE` expression");
    auto syms = parse_name("table name");
    Sym as;
    if (accept(Tok::Tag::K_AS)) as = parse_sym("`AS` clause");

    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("`WHERE` expression") : nullptr;
    return ast<Delete>(track, as, where, syms);
}

/*
 * Prepare / Execute / Deallocate / Show / Copy
 */

/// `PREPARE <name> [(types)] AS <stmt>` - or `PREPARE <name> FROM '<stmt>'`, which leaves the
/// statement as a string for whoever prepares it to parse.
AST<Expr> Parser::parse_prepare() {
    auto track = tracker();
    eat(Tok::Tag::K_PREPARE);
    auto sym = parse_sym("name of a `PREPARE` expression");

    ASTs<Type> types;
    if (ahead().isa(Tok::Tag::D_paren_l))
        parse_list("parameter type list of a `PREPARE` expression",
                   [&]() { types.emplace_back(parse_type("parameter type of a `PREPARE` expression")); });

    AST<Expr> stmt;
    Sym str;
    if (accept(Tok::Tag::K_FROM)) {
        if (ahead().isa(Tok::Tag::V_str))
            str = lex().sym();
        else
            syntax_err("string literal", "`FROM` clause of a `PREPARE` expression");
    } else {
        expect(Tok::Tag::K_AS, "`PREPARE` expression");
        stmt = parse_stmt();
    }

    return ast<Prepare>(track, sym, stmt, str, types);
}

AST<Expr> Parser::parse_execute() {
    auto track = tracker();
    eat(Tok::Tag::K_EXECUTE);
    auto sym = parse_sym("name of an `EXECUTE` expression");

    ASTs<Expr> args;
    bool paren = ahead().isa(Tok::Tag::D_paren_l);
    if (paren)
        parse_list("argument list of an `EXECUTE` expression",
                   [&]() { args.emplace_back(parse_expr("argument of an `EXECUTE` expression")); });

    return ast<Execute>(track, sym, paren, args);
}

AST<Expr> Parser::parse_deallocate() {
    auto track = tracker();
    eat(Tok::Tag::K_DEALLOCATE);
    accept(Tok::Tag::K_PREPARE);
    // `ALL` deallocates every prepared statement; no name of its own, hence the empty Sym.
    auto sym = accept(Tok::Tag::K_ALL) ? Sym() : parse_sym("name of a `DEALLOCATE` expression");
    return ast<Deallocate>(track, sym);
}

/// `SHOW TABLES`, `SHOW COLUMNS <table>`, and the `DESCRIBE <table>` that means the latter.
AST<Expr> Parser::parse_show() {
    auto track = tracker();

    if (accept(Tok::Tag::K_DESCRIBE)) return ast<Show>(track, Show::Columns, parse_name("table name"));

    expect_non_key(N_SHOW, "`SHOW` expression");
    if (accept_non_key(N_TABLES)) return ast<Show>(track, Show::Tables, Syms{});

    expect_non_key(N_COLUMNS, "`SHOW` expression");
    return ast<Show>(track, Show::Columns, parse_name("table name"));
}

/// `COPY <table> [(cols)] FROM <file> [(options)] [WHERE <where>]` reads, and
/// `COPY {<table> [(cols)] | (<query>)} TO <file> [(options)]` writes.
AST<Expr> Parser::parse_copy() {
    auto track = tracker();
    expect_non_key(N_COPY, "`COPY` expression");

    // A `(` here opens the query a `COPY ... TO` may take in place of a table.
    Syms syms, cols;
    AST<Expr> query;
    if (ahead().isa(Tok::Tag::D_paren_l)) {
        expect(Tok::Tag::D_paren_l, "`COPY` expression");
        auto _ = anchor(Tok::Tag::D_paren_r);
        query  = parse_query("source of a `COPY` expression", false);
        expect(Tok::Tag::D_paren_r, "closing delimiter of a `COPY` expression");
    } else {
        syms = parse_name("table name");
        if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("column list of a `COPY` expression", cols);
    }

    bool from = true;
    if (accept(Tok::Tag::K_FROM))
        from = true;
    else if (accept(Tok::Tag::K_TO))
        from = false;
    else
        syntax_err("`FROM` or `TO`", "`COPY` expression");

    // Without a file name the data travels through the standard input or output instead.
    Sym file;
    if (ahead().isa(Tok::Tag::V_str))
        file = lex().sym();
    else if (!accept_non_key(from ? N_STDIN : N_STDOUT))
        syntax_err("file name", "`COPY` expression");

    ASTs<Copy::Option> options;
    // The `WITH` is noise - the option list stands on its own just as well.
    if (ahead().isa(Tok::Tag::K_WITH) && ahead(1).isa(Tok::Tag::D_paren_l)) lex();
    if (ahead().isa(Tok::Tag::D_paren_l)) {
        parse_list("option list of a `COPY` expression", [&]() {
            auto opt_track = tracker();
            auto sym       = parse_sym("option of a `COPY` expression");

            // A format is named rather than computed, so a reserved word stands for itself here.
            Sym name;
            AST<Expr> val;
            if (isa_sym() && (ahead(1).isa(Tok::Tag::T_comma) || ahead(1).isa(Tok::Tag::D_paren_r)))
                name = parse_sym("argument of a `COPY` option");
            else if (!ahead().isa(Tok::Tag::T_comma) && !ahead().isa(Tok::Tag::D_paren_r))
                val = parse_expr("argument of a `COPY` option");

            options.emplace_back(ast<Copy::Option>(opt_track, sym, name, val));
        });
    }

    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("`WHERE` expression") : nullptr;
    return ast<Copy>(track, from, query, file, where, syms, cols, options);
}

/*
 * Select
 */

AST<Expr> Parser::parse_group_elem() {
    auto track = tracker();

    auto tag = Grouping::Rollup;
    if (accept(Tok::Tag::K_ROLLUP)) {
        tag = Grouping::Rollup;
    } else if (accept(Tok::Tag::K_CUBE)) {
        tag = Grouping::Cube;
    } else if (ahead().isa(Tok::Tag::K_GROUPING) && !ahead(1).isa(Tok::Tag::D_paren_l)) {
        // A `GROUPING(x)` is the ordinary function - only `GROUPING SETS` is a grouping element.
        eat(Tok::Tag::K_GROUPING);
        expect_non_key(N_SETS, "`GROUPING SETS` element");
        tag = Grouping::Sets;
    } else if (ahead().isa(Tok::Tag::D_paren_l) && ahead(1).isa(Tok::Tag::D_paren_r)) {
        eat(Tok::Tag::D_paren_l);
        eat(Tok::Tag::D_paren_r);
        return ast<Grouping>(track, Grouping::Empty, ASTs<Expr>{}); // the empty grouping set
    } else {
        return parse_expr("`GROUP BY` expression");
    }

    ASTs<Expr> args;
    parse_list("grouping element list", [&]() { args.emplace_back(parse_group_elem()); });
    return ast<Grouping>(track, tag, args);
}

AST<Expr> Parser::parse_select() {
    auto track = tracker();
    eat(Tok::Tag::K_SELECT);

    bool all = true;
    if (accept(Tok::Tag::K_ALL)) { /* do nothing */
    } else if (accept(Tok::Tag::K_DISTINCT)) {
        all = false;
    }

    ASTs<Select::Elem> elems;
    if (accept(Tok::Tag::T_mul)) {
        /* do nothing */
    } else {
        // `FROM` terminates the select list - and is what the enclosing SELECT expects next.
        auto _ = anchor(Tok::Tag::K_FROM);
        do {
            auto track = tracker();
            auto expr  = parse_expr("elem of a `SELECT` expression");
            Syms syms;

            if (accept(Tok::Tag::K_AS)) {
                if (ahead().isa(Tok::Tag::D_paren_l)) {
                    parse_list("column name list of `AS` clause",
                               [&]() { syms.emplace_back(parse_sym("column name within `AS` clause")); });
                } else {
                    syms.emplace_back(parse_sym("column name within `AS` clause "));
                }
            } else if (ahead().isa(Tok::Tag::V_id) && !isa_non_key(N_LIMIT)) {
                syms.emplace_back(lex().sym()); // an alias may drop the `AS`
            }
            elems.emplace_back(ast<Select::Elem>(track, expr, syms));
        } while (accept(Tok::Tag::T_comma));
    }

    // A `SELECT` needs no `FROM`: `SELECT 1` computes its value out of thin air.
    ASTs<Expr> froms;
    if (accept(Tok::Tag::K_FROM)) {
        do
            froms.emplace_back(parse_table_ref());
        while (accept(Tok::Tag::T_comma));
    }

    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("`WHERE` expression") : nullptr;

    ASTs<Expr> groups;
    if (accept(Tok::Tag::K_GROUP)) {
        expect(Tok::Tag::K_BY, "`GROUP` within `SELECT` expression");
        do
            groups.emplace_back(parse_group_elem());
        while (accept(Tok::Tag::T_comma));
    }

    auto having = accept(Tok::Tag::K_HAVING) ? parse_expr("`HAVING` expression") : nullptr;

    ASTs<Select::WindowDef> windows;
    if (accept(Tok::Tag::K_WINDOW)) {
        do {
            auto win_track = tracker();
            auto sym       = parse_sym("window name");
            expect(Tok::Tag::K_AS, "`WINDOW` clause");
            auto window = parse_window();
            windows.emplace_back(ast<Select::WindowDef>(win_track, sym, window));
        } while (accept(Tok::Tag::T_comma));
    }

    return ast<Select>(track, all, where, having, elems, froms, groups, windows);
}

/*
 * Table references
 */

std::optional<Join::Tag> Parser::parse_join_op() {
    int tag    = 0;
    bool inner = false;
    if (accept(Tok::Tag::K_CROSS)) {
        tag = Join::Cross;
    } else {
        tag = accept(Tok::Tag::K_NATURAL) ? Join::Natural : 0;

        // clang-format off
        if      (accept(Tok::Tag::K_INNER)) inner = true;
        else if (accept(Tok::Tag::K_LEFT )) tag |= Join::Left;
        else if (accept(Tok::Tag::K_RIGHT)) tag |= Join::Right;
        else if (accept(Tok::Tag::K_FULL) ) tag |= Join::Full;
        // clang-format on
        if (tag & Join::Left || tag & Join::Right) accept(Tok::Tag::K_OUTER); // or Join::Full
    }

    if (tag || inner)
        expect(Tok::Tag::K_JOIN, "`JOIN` operator");
    else if (accept(Tok::Tag::K_JOIN))
        return Join::Inner;
    else
        return {};

    return (Join::Tag)tag;
}

/// One entry of the `FROM` list: a chain of `JOIN`s over table factors, left-associative.
AST<Expr> Parser::parse_table_ref() {
    auto track = tracker();
    auto lhs   = parse_table_factor();

    while (auto tag = parse_join_op()) {
        auto rhs = parse_table_factor();

        Join::Spec spec;
        if (accept(Tok::Tag::K_ON)) {
            spec = parse_expr("search condition for an `ON` clause of a `JOIN` specification");
        } else if (accept(Tok::Tag::K_USING)) {
            Syms syms;
            parse_col_list("join column list for a `USING` clause of a `JOIN` specification", syms);
            spec = driver().copy(syms);
        }

        lhs = ast<Join>(track, lhs, *tag, rhs, spec);
    }

    return lhs;
}

/// A table primary and whatever binds to it: `LATERAL`, `WITH ORDINALITY`, and a correlation name.
/// Binding these here rather than to the whole `FROM` entry is what lets each side of a `JOIN` carry
/// its own - `a AS x JOIN b AS y ON ...`.
AST<Expr> Parser::parse_table_factor() {
    auto track   = tracker();
    bool lateral = (bool)accept(Tok::Tag::K_LATERAL);
    auto expr    = parse_table_primary();

    bool ordinality = false;
    if (ahead().isa(Tok::Tag::K_WITH) && ahead(1).isa(Tok::Tag::V_id) && ahead(1).sym() == non_key(N_ORDINALITY)) {
        lex();
        lex();
        ordinality = true;
    }

    // A correlation name may follow with or without `AS`; without it, only a plain identifier
    // qualifies - a reserved word there would swallow the clause that follows. `LIMIT` is no
    // reserved word but starts a clause all the same, so it does not qualify either.
    Sym as;
    Syms cols;
    if (accept(Tok::Tag::K_AS))
        as = parse_sym("`AS` clause");
    else if (ahead().isa(Tok::Tag::V_id) && !isa_non_key(N_LIMIT))
        as = lex().sym();
    if (as && ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("column name list of a `FROM` clause", cols);

    // Nothing bound to it? Then the primary is the table reference, and no node stands in between.
    if (!lateral && !ordinality && !as) return expr;
    return ast<TableRef>(track, lateral, expr, ordinality, as, cols);
}

/// A table name, a function call such as `unnest(a)`, a derived table, or a parenthesized table
/// reference. The last two both start with a `(`: what follows tells them apart, as a derived table
/// is a query and nothing else is.
AST<Expr> Parser::parse_table_primary() {
    if (ahead().isa(Tok::Tag::D_paren_l) && !ISA(ahead(1).tag(), C_QUERY)) {
        // `( <table reference> )` - these parentheses only group, so they leave no node behind.
        eat(Tok::Tag::D_paren_l);
        auto _    = anchor(Tok::Tag::D_paren_r);
        auto expr = parse_table_ref();
        expect(Tok::Tag::D_paren_r, "closing delimiter of a parenthesized table reference");
        return expr;
    }

    return parse_expr("table reference of a `FROM` clause");
}

/*
 * Order / Window
 */

AST<Order> Parser::parse_order() {
    auto track = tracker();
    auto expr  = parse_expr("sort key of an `ORDER BY` clause");

    // ASC and DESC are not reserved words.
    bool desc = false;
    if (isa_non_key(N_ASC) || isa_non_key(N_DESC)) desc = lex().sym() == non_key(N_DESC);

    auto nulls = Order::Nulls_None;
    if (accept_non_key(N_NULLS)) {
        if (accept_non_key(N_FIRST)) {
            nulls = Order::Nulls_First;
        } else {
            expect_non_key(N_LAST, "`NULLS` of a sort specification");
            nulls = Order::Nulls_Last;
        }
    }

    return ast<Order>(track, expr, desc, nulls);
}

AST<Window> Parser::parse_window() {
    auto track = tracker();

    // `OVER w` names a window defined in the `WINDOW` clause and takes no parentheses.
    if (!ahead().isa(Tok::Tag::D_paren_l)) {
        auto sym = parse_sym("window name");
        return ast<Window>(track, sym, false, AST<Frame>{}, ASTs<Expr>{}, ASTs<Order>{});
    }

    eat(Tok::Tag::D_paren_l);
    auto _ = anchor(Tok::Tag::D_paren_r);

    // Every clause below starts with a reserved word, so a leading identifier can only be the name
    // of the window this one refines.
    Sym name;
    if (ahead().isa(Tok::Tag::V_id)) name = lex().sym();

    ASTs<Expr> partitions;
    if (accept(Tok::Tag::K_PARTITION)) {
        expect(Tok::Tag::K_BY, "`PARTITION` of a window specification");
        do
            partitions.emplace_back(parse_expr("`PARTITION BY` expression"));
        while (accept(Tok::Tag::T_comma));
    }

    ASTs<Order> orders;
    if (accept(Tok::Tag::K_ORDER)) {
        expect(Tok::Tag::K_BY, "`ORDER` of a window specification");
        do
            orders.emplace_back(parse_order());
        while (accept(Tok::Tag::T_comma));
    }

    AST<Frame> frame;
    if (ISA(ahead().tag(), C_FRAME_UNIT)) frame = parse_frame();

    expect(Tok::Tag::D_paren_r, "closing delimiter of a window specification");
    return ast<Window>(track, name, true, frame, partitions, orders);
}

AST<Frame::Bound> Parser::parse_frame_bound() {
    auto track = tracker();

    if (accept_non_key(N_UNBOUNDED)) {
        if (accept_non_key(N_PRECEDING))
            return ast<Frame::Bound>(track, Frame::Bound::Unbounded_Preceding, AST<Expr>{});
        expect_non_key(N_FOLLOWING, "unbounded window frame bound");
        return ast<Frame::Bound>(track, Frame::Bound::Unbounded_Following, AST<Expr>{});
    }

    if (accept(Tok::Tag::K_CURRENT)) {
        expect(Tok::Tag::K_ROW, "`CURRENT ROW` window frame bound");
        return ast<Frame::Bound>(track, Frame::Bound::Current_Row, AST<Expr>{});
    }

    // Above Tok::Prec::And, so the `AND` of a `BETWEEN` terminates the offset instead of joining it.
    auto expr = parse_expr("offset of a window frame bound", Tok::Prec::Not);
    if (accept_non_key(N_PRECEDING)) return ast<Frame::Bound>(track, Frame::Bound::Preceding, expr);
    expect_non_key(N_FOLLOWING, "window frame bound");
    return ast<Frame::Bound>(track, Frame::Bound::Following, expr);
}

AST<Frame> Parser::parse_frame() {
    auto track = tracker();
    auto unit  = lex().tag();

    AST<Frame::Bound> lo, hi;
    if (accept(Tok::Tag::K_BETWEEN)) {
        lo = parse_frame_bound();
        expect(Tok::Tag::K_AND, "window frame");
        hi = parse_frame_bound();
    } else {
        lo = parse_frame_bound();
    }

    auto exclude = Frame::Exclude_None;
    if (accept_non_key(N_EXCLUDE)) {
        if (accept(Tok::Tag::K_CURRENT)) {
            expect(Tok::Tag::K_ROW, "`EXCLUDE CURRENT ROW` of a window frame");
            exclude = Frame::Exclude_Current_Row;
        } else if (accept(Tok::Tag::K_GROUP)) {
            exclude = Frame::Exclude_Group;
        } else if (accept_non_key(N_TIES)) {
            exclude = Frame::Exclude_Ties;
        } else if (accept(Tok::Tag::K_NO)) {
            expect_non_key(N_OTHERS, "`EXCLUDE NO OTHERS` of a window frame");
            exclude = Frame::Exclude_No_Others;
        } else {
            syntax_err("`CURRENT ROW`, `GROUP`, `TIES`, or `NO OTHERS`", "`EXCLUDE` of a window frame");
        }
    }

    return ast<Frame>(track, unit, lo, hi, exclude);
}

/*
 * Query
 */

AST<Expr> Parser::parse_values() {
    auto track = tracker();
    eat(Tok::Tag::K_VALUES);

    ASTs<Expr> rows;
    do {
        auto row_track = tracker();
        if (ahead().isa(Tok::Tag::D_paren_l)) {
            ASTs<Expr> args;
            parse_list("row of a `VALUES` clause",
                       [&]() { args.emplace_back(parse_expr("value of a `VALUES` clause")); });
            rows.emplace_back(ast<ParenExprList>(row_track, args));
        } else {
            // A one-column row needs no parentheses of its own.
            rows.emplace_back(parse_expr("value of a `VALUES` clause"));
        }
    } while (accept(Tok::Tag::T_comma));

    return ast<Values>(track, rows);
}

AST<Expr> Parser::parse_table() {
    auto track = tracker();
    eat(Tok::Tag::K_TABLE);
    return ast<Table>(track, parse_name("table name"));
}

AST<Expr> Parser::parse_query_primary(fe::Cite ctxt, bool value_ok) {
    switch (ahead().tag()) {
        case Tok::Tag::K_SELECT: return parse_select();
        case Tok::Tag::K_VALUES: return parse_values();
        case Tok::Tag::K_TABLE: return parse_table();
        default: break;
    }

    // Every value expression is fair game where one is allowed - and a parenthesized one is where
    // `(SELECT ...)` and `(a, b)` meet, so that case goes through Parser::parse_primary_or_unary_expr.
    if (value_ok) return parse_expr(ctxt);

    if (ahead().isa(Tok::Tag::D_paren_l)) {
        auto track = tracker();
        ASTs<Expr> args;
        parse_list(fe::Cite(ctxt), [&]() { args.emplace_back(parse_query(ctxt, false)); });
        return ast<ParenExprList>(track, args);
    }

    syntax_err("query expression", ctxt);
    return ast<ErrExpr>(curr_);
}

AST<Expr> Parser::parse_query_term(fe::Cite ctxt, bool value_ok) {
    auto track = tracker();
    auto lhs   = parse_query_primary(ctxt, value_ok);

    while (ahead().isa(Tok::Tag::K_INTERSECT)) {
        eat(Tok::Tag::K_INTERSECT);
        bool all = (bool)accept(Tok::Tag::K_ALL);
        if (!all) accept(Tok::Tag::K_DISTINCT);
        auto rhs = parse_query_primary("right-hand side of an `INTERSECT` expression", value_ok);
        lhs      = ast<SetOp>(track, lhs, SetOp::Intersect, all, rhs);
    }

    return lhs;
}

AST<Expr> Parser::parse_query(fe::Cite ctxt, bool value_ok) {
    auto track = tracker();

    bool recursive = false;
    ASTs<Query::Cte> ctes;
    if (accept(Tok::Tag::K_WITH)) {
        recursive = (bool)accept(Tok::Tag::K_RECURSIVE);
        do {
            auto cte_track = tracker();
            auto sym       = parse_sym("name of a common table expression");
            Syms cols;
            if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("column name list of a common table expression", cols);
            expect(Tok::Tag::K_AS, "common table expression");
            expect(Tok::Tag::D_paren_l, "common table expression");
            auto _     = anchor(Tok::Tag::D_paren_r);
            auto query = parse_query("body of a common table expression", false);
            expect(Tok::Tag::D_paren_r, "closing delimiter of a common table expression");
            ctes.emplace_back(ast<Query::Cte>(cte_track, sym, query, cols));
        } while (accept(Tok::Tag::T_comma));
    }

    auto body = parse_query_term(ctxt, value_ok);

    while (ahead().isa(Tok::Tag::K_UNION) || ahead().isa(Tok::Tag::K_EXCEPT)) {
        auto tag = lex().isa(Tok::Tag::K_UNION) ? SetOp::Union : SetOp::Except;
        bool all = (bool)accept(Tok::Tag::K_ALL);
        if (!all) accept(Tok::Tag::K_DISTINCT);
        auto rhs = parse_query_term("right-hand side of a `UNION` or `EXCEPT` expression", value_ok);
        body     = ast<SetOp>(track, body, tag, all, rhs);
    }

    ASTs<Order> orders;
    if (accept(Tok::Tag::K_ORDER)) {
        expect(Tok::Tag::K_BY, "`ORDER` within a query expression");
        do
            orders.emplace_back(parse_order());
        while (accept(Tok::Tag::T_comma));
    }

    // `OFFSET`/`FETCH` is what the standard says, `LIMIT` what every dialect grew instead. A loop
    // takes them in whatever order and combination they turn up in.
    AST<Expr> offset, fetch, limit;
    while (true) {
        if (!offset && accept(Tok::Tag::K_OFFSET)) {
            offset = parse_expr("`OFFSET` clause");
            if (!accept(Tok::Tag::K_ROW)) accept(Tok::Tag::K_ROWS);
        } else if (!fetch && accept(Tok::Tag::K_FETCH)) {
            if (isa_non_key(N_FIRST) || isa_non_key(N_NEXT)) lex();
            fetch = parse_expr("`FETCH` clause");
            if (!accept(Tok::Tag::K_ROW)) accept(Tok::Tag::K_ROWS);
            expect(Tok::Tag::K_ONLY, "`FETCH` clause");
        } else if (!limit && accept_non_key(N_LIMIT)) {
            // `LIMIT ALL` is the explicit way of spelling out no limit at all.
            limit = ahead().isa(Tok::Tag::K_ALL) ? ast<SimpleVal>(lex().loc(), Tok::Tag::K_ALL)
                                                 : parse_expr("`LIMIT` clause");
        } else {
            break;
        }
    }

    ASTs<Lock> locks;
    while (ahead().isa(Tok::Tag::K_FOR))
        locks.emplace_back(parse_lock());

    if (ctes.empty() && orders.empty() && !offset && !fetch && !limit && locks.empty()) return body;
    return ast<Query>(track, recursive, body, offset, fetch, limit, ctes, orders, locks);
}

/// `FOR UPDATE|NO KEY UPDATE|SHARE|KEY SHARE [OF <tables>] [NOWAIT|SKIP LOCKED]`.
/// Of the words involved only `FOR`, `NO`, `UPDATE`, and `OF` are reserved; the rest are plain
/// identifiers that mean something here and nowhere else.
AST<Lock> Parser::parse_lock() {
    auto track = tracker();
    eat(Tok::Tag::K_FOR);

    auto strength = Lock::Update;
    if (accept(Tok::Tag::K_NO)) {
        expect_non_key(N_KEY, "`FOR NO KEY UPDATE` clause");
        expect(Tok::Tag::K_UPDATE, "`FOR NO KEY UPDATE` clause");
        strength = Lock::No_Key_Update;
    } else if (accept_non_key(N_KEY)) {
        expect_non_key(N_SHARE, "`FOR KEY SHARE` clause");
        strength = Lock::Key_Share;
    } else if (accept_non_key(N_SHARE)) {
        strength = Lock::Share;
    } else {
        expect(Tok::Tag::K_UPDATE, "`FOR` clause of a query expression");
    }

    fe::Vector<fe::View<Sym>> tables;
    if (accept(Tok::Tag::K_OF)) {
        do
            tables.emplace_back(driver().copy(parse_name("table name of an `OF` clause")));
        while (accept(Tok::Tag::T_comma));
    }

    auto wait = Lock::Block;
    if (accept_non_key(N_NOWAIT)) {
        wait = Lock::Nowait;
    } else if (accept_non_key(N_SKIP)) {
        expect_non_key(N_LOCKED, "`SKIP LOCKED` of a `FOR` clause");
        wait = Lock::Skip_Locked;
    }

    return ast<Lock>(track, strength, wait, tables);
}

} // namespace sql
