#include "sql/parser.h"

#include <iostream>

using namespace std::literals;

namespace sql {

static std::string to_lower(std::string_view sv) {
    std::string res;
    for (auto c : sv)
        res += tolower(c);
    return res;
}

Parser::Parser(Driver& driver, fe::Error& err, const fe::Src& src)
    : lexer_(driver, err, src)
    , err_(err)
    , error_(driver.sym("<error>"s))
    , key_(driver.sym("key"s))
    , asc_(driver.sym("asc"s))
    , desc_(driver.sym("desc"s))
    , first_(driver.sym("first"s))
    , next_(driver.sym("next"s)) {
    init();
}

void Parser::err(const std::string& what, const Tok& tok, std::string_view ctxt) {
    err_.error(tok.loc(), "expected {}, got '{}' while parsing {}", what, tok, ctxt);
}

/*
 * misc
 */

AST<Prog> Parser::parse_prog() {
    auto track = tracker();
    ASTs<Expr> exprs;

    while (!ahead().isa(Tok::Tag::EoF)) {
        // The `;` closes the statement no matter how badly it went, so nothing nested may swallow it.
        auto _ = anchor(Tok::Tag::T_semicolon, "expression list");
        exprs.emplace_back(parse_query("program"));
        // Whatever is left before the `;` is bogus; discarding it also prevents an endless loop.
        recover([](Tok::Tag tag) { return tag != Tok::Tag::T_semicolon && tag != Tok::Tag::EoF; }, "program");
    }

    eat(Tok::Tag::EoF);
    return ast<Prog>(track, std::move(exprs));
}

bool Parser::isa_sym() const { return ahead().isa(Tok::Tag::V_id) || ahead().isa_key(); }

Sym Parser::parse_sym(std::string_view ctxt) {
    if (ahead().isa(Tok::Tag::V_id)) return lex().sym();
    if (ahead().isa_key()) return driver().sym(to_lower(Tok::str(lex().tag())));
    err("identifier", ctxt);
    return error_;
}

void Parser::parse_col_list(std::string ctxt, Syms& syms) {
    parse_list(ctxt, [&]() { syms.emplace_back(parse_sym("column name")); });
}

/*
 * Type
 */

AST<Type> Parser::parse_type(std::string_view ctxt) {
    auto track = tracker();

    // Optional trailing `NOT NULL` - a plain `NULL` explicitly spells out the default.
    auto parse_not_null = [&]() {
        if (accept(Tok::Tag::K_NOT)) {
            expect(Tok::Tag::K_NULL, "NOT NULL constraint");
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
        case Tok::Tag::K_INT:
        case Tok::Tag::K_INTEGER:
        case Tok::Tag::K_SMALLINT:
        case Tok::Tag::K_BIGINT:
        case Tok::Tag::K_BOOLEAN:
        case Tok::Tag::K_DATE:
        case Tok::Tag::K_REAL:
        case Tok::Tag::K_DOUBLE:
        case Tok::Tag::K_FLOAT:
        case Tok::Tag::K_TIME:
        case Tok::Tag::K_TIMESTAMP:
        case Tok::Tag::K_INTERVAL:
        case Tok::Tag::K_BLOB:
        case Tok::Tag::K_CLOB:
        case Tok::Tag::K_NUMERIC:
        case Tok::Tag::K_DECIMAL:
        case Tok::Tag::K_DEC:
        case Tok::Tag::K_CHAR:
        case Tok::Tag::K_CHARACTER:
        case Tok::Tag::K_VARCHAR:
        case Tok::Tag::K_BINARY:
        case Tok::Tag::K_VARBINARY: {
            auto tag = lex().tag();
            if (accept(Tok::Tag::K_LARGE)) {
                parse_sym("LARGE OBJECT type"); // OBJECT is not a reserved word
                // `CHARACTER LARGE OBJECT` and `BINARY LARGE OBJECT` just spell out CLOB and BLOB.
                tag = tag == Tok::Tag::K_BINARY ? Tok::Tag::K_BLOB : Tok::Tag::K_CLOB;
            }
            accept(Tok::Tag::K_PRECISION); // DOUBLE PRECISION
            auto varying = (bool)accept(Tok::Tag::K_VARYING);
            ASTs<Expr> args;
            parse_args(args);
            return ast<SimpleType>(track, tag, varying, std::move(args), parse_not_null());
        }
        default: break;
    }

    // Anything not a reserved word - `text`, `uuid`, `jsonb`, ... - is a type name just the same.
    if (ahead().isa(Tok::Tag::V_id)) {
        auto sym = lex().sym();
        ASTs<Expr> args;
        parse_args(args);
        return ast<NamedType>(track, sym, std::move(args), parse_not_null());
    }

    if (!ctxt.empty()) {
        err("type", ctxt);
        return nullptr; // Error Type
    }

    return nullptr;
}

/*
 * Expr
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
        expect(Tok::Tag::K_JOIN, "JOIN operator");
    else if (accept(Tok::Tag::K_JOIN))
        return Join::Inner;
    else
        return {};

    return (Join::Tag)tag;
}

/// `expr [NOT] BETWEEN lo AND hi`.
/// Both bounds parse above Tok::Prec::And so that the separating `AND` terminates the lower one
/// instead of being swallowed as a conjunction.
AST<Expr> Parser::parse_between(Tracker track, AST<Expr>&& lhs, bool negated) {
    eat(Tok::Tag::K_BETWEEN);
    auto lo = parse_expr("lower bound of a BETWEEN expression", Tok::Prec::Not);
    expect(Tok::Tag::K_AND, "BETWEEN expression");
    auto hi = parse_expr("upper bound of a BETWEEN expression", Tok::Prec::Not);
    return ast<Between>(track, std::move(lhs), std::move(lo), std::move(hi), negated);
}

AST<Expr> Parser::parse_expr(std::string_view ctxt, Tok::Prec cur_prec) {
    auto track = tracker();
    auto lhs   = parse_primary_or_unary_expr(ctxt);

    // All binary operators here are left-associative, so the right-hand side must bind strictly tighter.
    auto next_prec = [](Tok::Prec prec) { return (Tok::Prec)((int)prec + 1); };

    while (true) {
        if (ahead().isa(Tok::Tag::K_NOT)) {
            auto prec = Tok::bin_prec(ahead(1).tag());
            if (!prec || *prec < cur_prec) break;
            eat(Tok::Tag::K_NOT);

            if (ahead().isa(Tok::Tag::K_BETWEEN)) {
                lhs = parse_between(track, std::move(lhs), true);
            } else {
                auto tag = lex().tag();
                auto rhs = parse_expr("right-hand side of binary expression with NOT in front of operator",
                                      next_prec(*prec));
                lhs      = ast<BinExprWithPreTag>(track, std::move(lhs), Tok::Tag::K_NOT, tag, std::move(rhs));
            }
        } else if (ahead().isa(Tok::Tag::K_BETWEEN)) {
            if (*Tok::bin_prec(Tok::Tag::K_BETWEEN) < cur_prec) break;
            lhs = parse_between(track, std::move(lhs), false);
        } else if (ahead().isa(Tok::Tag::K_IS)) {
            if (*Tok::bin_prec(Tok::Tag::K_IS) < cur_prec) break;
            eat(Tok::Tag::K_IS);
            auto tag = accept(Tok::Tag::K_NOT) ? Tok::Tag::K_IS_NOT : Tok::Tag::K_IS;
            auto rhs = parse_expr("right-hand side of an IS expression", next_prec(Tok::Prec::Comp));
            lhs      = ast<BinExpr>(track, std::move(lhs), tag, std::move(rhs));
        } else if (auto prec = Tok::bin_prec(ahead().tag())) {
            if (*prec < cur_prec) break;

            auto op  = lex().tag();
            auto rhs = parse_expr("right-hand side of binary expression", next_prec(*prec));
            lhs      = ast<BinExpr>(track, std::move(lhs), op, std::move(rhs));
        } else if (Tok::Prec::Join < cur_prec) {
            break; // a JOIN would not bind here - don't even try to consume its operator
        } else if (auto tag = parse_join_op()) {
            auto rhs = parse_expr("right-hand side of JOIN operator", next_prec(Tok::Prec::Join));

            Join::Spec spec;
            if (accept(Tok::Tag::K_ON)) {
                // Above Tok::Prec::Join, so a following JOIN terminates the condition instead of
                // being pulled into it.
                spec = parse_expr("search condition for an ON clause of a JOIN specification",
                                  next_prec(Tok::Prec::Join));
            } else if (accept(Tok::Tag::K_USING)) {
                Syms syms;
                parse_col_list("join column list for a USING clause of a JOIN specification", syms);
                spec = std::move(syms);
            }

            lhs = ast<Join>(track, std::move(lhs), *tag, std::move(rhs), std::move(spec));
        } else {
            break;
        }
    }

    return lhs;
}

AST<Expr> Parser::parse_primary_or_unary_expr(std::string_view ctxt) {
    switch (ahead().tag()) {
        case Tok::Tag::K_CREATE: return parse_create();
        case Tok::Tag::K_DROP: return parse_drop();
        case Tok::Tag::K_SELECT: return parse_select();
        case Tok::Tag::K_INSERT: return parse_insert();
        case Tok::Tag::K_UPDATE: return parse_update();
        case Tok::Tag::K_DELETE: return parse_delete();
        case Tok::Tag::K_CASE: return parse_case();
        case Tok::Tag::K_CAST: return parse_cast();
        case Tok::Tag::V_int: {
            auto tok = lex();
            return ast<IntVal>(tok.loc(), tok.u64());
        }
        case Tok::Tag::V_str: {
            auto tok = lex();
            return ast<StrVal>(tok.loc(), tok.sym());
        }
        case Tok::Tag::T_mul:
        case Tok::Tag::K_TRUE:
        case Tok::Tag::K_FALSE:
        case Tok::Tag::K_UNKNOWN:
        case Tok::Tag::K_NULL: {
            auto tok = lex();
            return ast<SimpleVal>(tok.loc(), tok.tag());
        }
        default: break;
    }

    auto track = tracker();

    // Before the generic call below, so that `NOT (...)` stays an operator rather than a function.
    if (auto prec = Tok::un_prec(ahead().tag())) {
        auto op = lex().tag();
        return ast<UnExpr>(track, op, parse_expr("operand of unary expression", *prec));
    }

    // `EXISTS (subquery)` - a unary operator whose operand happens to be parenthesized.
    if (ahead().isa(Tok::Tag::K_EXISTS)) {
        auto op = lex().tag();
        return ast<UnExpr>(track, op, parse_expr("operand of EXISTS expression", Tok::Prec::Unary));
    }

    // Any `name(...)` is a call - aggregates and scalar functions alike, whether or not the name
    // happens to be a reserved word.
    if (isa_sym() && ahead(1).isa(Tok::Tag::D_paren_l)) return parse_func();

    // A reserved word directly followed by a `.` can only be the qualifier of a reference - no clause
    // keyword is ever followed by one - so `at.movie_id` resolves even though `AT` is reserved.
    if (ahead().isa(Tok::Tag::V_id) || (isa_sym() && ahead(1).isa(Tok::Tag::T_dot))) return parse_id();

    if (ahead().isa(Tok::Tag::D_paren_l)) {
        ASTs<Expr> args;
        parse_list("parenthesized expression list",
                   [&]() { args.emplace_back(parse_query("parenthesized expression")); });

        // Parentheses around a query are structural - they are what makes it a subquery. Around a
        // single scalar expression they are pure grouping, and keeping a node for them would make
        // them pile up with every dump/re-parse round trip.
        if (args.size() == 1) {
            const auto* arg = args.front().get();
            if (!arg->isa<Select>() && !arg->isa<SetOp>() && !arg->isa<Query>()) return std::move(args.front());
        }

        return ast<ParenExprList>(track, std::move(args));
    }

    if (!ctxt.empty()) {
        err("primary or unary expression", ctxt);
        return ast<ErrExpr>(curr_);
    }
    fe::unreachable();
}

AST<Expr> Parser::parse_id() {
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

    return ast<Id>(track, std::move(syms), asterisk);
}

/// A column- or table-level constraint; @p table_level ones carry their own column list.
AST<Constraint> Parser::parse_constraint(bool table_level) {
    auto track = tracker();
    Sym name;
    if (accept(Tok::Tag::K_CONSTRAINT)) name = parse_sym("constraint name");

    auto tag = Constraint::Primary_Key;
    Syms cols;
    Sym table;
    Syms ref_cols;
    AST<Expr> expr;

    if (accept(Tok::Tag::K_PRIMARY)) {
        tag = Constraint::Primary_Key;
        if (ahead().isa(Tok::Tag::V_id) && ahead().sym() == key_) lex(); // KEY is not reserved
        if (table_level) parse_col_list("primary key column list", cols);
    } else if (accept(Tok::Tag::K_UNIQUE)) {
        tag = Constraint::Unique;
        if (table_level) parse_col_list("unique column list", cols);
    } else if (accept(Tok::Tag::K_FOREIGN)) {
        tag = Constraint::Foreign_Key;
        if (ahead().isa(Tok::Tag::V_id) && ahead().sym() == key_) lex();
        parse_col_list("foreign key column list", cols);
        expect(Tok::Tag::K_REFERENCES, "FOREIGN KEY constraint");
        table = parse_sym("referenced table name");
        if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("referenced column list", ref_cols);
    } else if (accept(Tok::Tag::K_REFERENCES)) {
        tag   = Constraint::References;
        table = parse_sym("referenced table name");
        if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("referenced column list", ref_cols);
    } else if (accept(Tok::Tag::K_CHECK)) {
        tag = Constraint::Check;
        expect(Tok::Tag::D_paren_l, "CHECK constraint");
        auto _ = anchor(Tok::Tag::D_paren_r);
        expr   = parse_expr("search condition of a CHECK constraint");
        expect(Tok::Tag::D_paren_r, "closing delimiter of a CHECK constraint");
    } else if (accept(Tok::Tag::K_DEFAULT)) {
        tag  = Constraint::Default;
        expr = parse_expr("default value");
    } else {
        err("constraint", table_level ? "table constraint" : "column constraint");
    }

    return ast<Constraint>(track, name, tag, std::move(cols), table, std::move(ref_cols), std::move(expr));
}

/// Does a `CREATE TABLE` element start a table-level constraint rather than a column definition?
static bool isa_table_constraint(Tok::Tag tag) {
    switch (tag) {
        case Tok::Tag::K_CONSTRAINT:
        case Tok::Tag::K_PRIMARY:
        case Tok::Tag::K_UNIQUE:
        case Tok::Tag::K_FOREIGN:
        case Tok::Tag::K_CHECK: return true;
        default: return false;
    }
}

AST<Expr> Parser::parse_create() {
    auto track = tracker();
    eat(Tok::Tag::K_CREATE);

    expect(Tok::Tag::K_TABLE, "CREATE expression");
    auto sym = parse_sym("table name");
    ASTs<Create::Elem> elems;
    ASTs<Constraint> constraints;

    parse_list("table element list", [&]() {
        if (isa_table_constraint(ahead().tag())) {
            constraints.emplace_back(parse_constraint(true));
            return;
        }

        auto track = tracker();
        auto sym   = parse_sym("column name");
        auto type  = parse_type("column type");
        ASTs<Constraint> col_constraints;
        while (isa_table_constraint(ahead().tag()) || ahead().isa(Tok::Tag::K_REFERENCES)
               || ahead().isa(Tok::Tag::K_DEFAULT))
            col_constraints.emplace_back(parse_constraint(false));
        elems.emplace_back(ast<Create::Elem>(track, sym, std::move(type), std::move(col_constraints)));
    });

    return ast<Create>(track, sym, std::move(elems), std::move(constraints));
}

AST<Expr> Parser::parse_drop() {
    auto track = tracker();
    eat(Tok::Tag::K_DROP);
    expect(Tok::Tag::K_TABLE, "DROP expression");
    return ast<Drop>(track, parse_sym("table name"));
}

AST<Expr> Parser::parse_func() {
    auto track = tracker();
    auto sym   = parse_sym("function name");

    expect(Tok::Tag::D_paren_l, "function argument list");
    auto _        = anchor(Tok::Tag::D_paren_r);
    bool distinct = (bool)accept(Tok::Tag::K_DISTINCT);
    if (!distinct) accept(Tok::Tag::K_ALL);

    ASTs<Expr> args;
    parse_seq(
        "function argument list", [&]() { args.emplace_back(parse_expr("argument of function")); },
        Tok::Tag::D_paren_r);
    expect(Tok::Tag::D_paren_r, "closing delimiter of a function argument list");

    return ast<Func>(track, sym, distinct, std::move(args));
}

AST<Expr> Parser::parse_cast() {
    auto track = tracker();
    eat(Tok::Tag::K_CAST);

    expect(Tok::Tag::D_paren_l, "CAST expression");
    auto _    = anchor(Tok::Tag::D_paren_r);
    auto expr = parse_expr("operand of a CAST expression");
    expect(Tok::Tag::K_AS, "CAST expression");
    auto type = parse_type("target type of a CAST expression");
    expect(Tok::Tag::D_paren_r, "closing delimiter of a CAST expression");

    return ast<Cast>(track, std::move(expr), std::move(type));
}

AST<Expr> Parser::parse_case() {
    auto track = tracker();
    eat(Tok::Tag::K_CASE);
    auto _ = anchor(Tok::Tag::K_END);

    // `CASE WHEN ...` is the searched form and has no operand.
    AST<Expr> operand;
    if (!ahead().isa(Tok::Tag::K_WHEN)) operand = parse_expr("operand of a CASE expression");

    ASTs<CaseExpr::When> whens;
    do {
        auto when_track = tracker();
        eat(Tok::Tag::K_WHEN);
        auto cond = parse_expr("condition of a WHEN clause");
        expect(Tok::Tag::K_THEN, "WHEN clause of a CASE expression");
        auto then = parse_expr("result of a WHEN clause");
        whens.emplace_back(ast<CaseExpr::When>(when_track, std::move(cond), std::move(then)));
    } while (ahead().isa(Tok::Tag::K_WHEN));

    AST<Expr> elze;
    if (accept(Tok::Tag::K_ELSE)) elze = parse_expr("ELSE clause of a CASE expression");
    expect(Tok::Tag::K_END, "CASE expression");

    return ast<CaseExpr>(track, std::move(operand), std::move(whens), std::move(elze));
}

AST<Expr> Parser::parse_insert() {
    auto track = tracker();
    eat(Tok::Tag::K_INSERT);
    expect(Tok::Tag::K_INTO, "INSERT expression");
    auto sym = parse_sym("table name");

    // A parenthesized list here is the column list; without it the rows follow directly.
    Syms cols;
    if (ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("insert column list", cols);

    ASTs<Expr> rows;
    AST<Expr> query;
    if (accept(Tok::Tag::K_VALUES)) {
        do {
            auto row_track = tracker();
            ASTs<Expr> args;
            parse_list("row of a VALUES clause", [&]() { args.emplace_back(parse_expr("value of a VALUES clause")); });
            rows.emplace_back(ast<ParenExprList>(row_track, std::move(args)));
        } while (accept(Tok::Tag::T_comma));
    } else {
        query = parse_query("source of an INSERT expression");
    }

    return ast<Insert>(track, sym, std::move(cols), std::move(rows), std::move(query));
}

AST<Expr> Parser::parse_update() {
    auto track = tracker();
    eat(Tok::Tag::K_UPDATE);
    auto sym = parse_sym("table name");
    Sym as;
    if (accept(Tok::Tag::K_AS)) as = parse_sym("AS clause");
    expect(Tok::Tag::K_SET, "UPDATE expression");

    ASTs<Update::Assign> assigns;
    do {
        auto assign_track = tracker();
        auto col          = parse_sym("column name of a SET clause");
        expect(Tok::Tag::T_eq, "assignment of a SET clause");
        auto expr = parse_expr("value of a SET clause");
        assigns.emplace_back(ast<Update::Assign>(assign_track, col, std::move(expr)));
    } while (accept(Tok::Tag::T_comma));

    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("WHERE expression") : nullptr;
    return ast<Update>(track, sym, as, std::move(assigns), std::move(where));
}

AST<Expr> Parser::parse_delete() {
    auto track = tracker();
    eat(Tok::Tag::K_DELETE);
    expect(Tok::Tag::K_FROM, "DELETE expression");
    auto sym = parse_sym("table name");
    Sym as;
    if (accept(Tok::Tag::K_AS)) as = parse_sym("AS clause");

    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("WHERE expression") : nullptr;
    return ast<Delete>(track, sym, as, std::move(where));
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
            auto expr  = parse_expr("elem of a SELECT expression");
            Syms syms;

            if (accept(Tok::Tag::K_AS)) {
                if (ahead().isa(Tok::Tag::D_paren_l)) {
                    parse_list("column name list of AS clause",
                               [&]() { syms.emplace_back(parse_sym("column name within AS clause")); });
                } else {
                    syms.emplace_back(parse_sym("column name within AS clause "));
                }
            }
            elems.emplace_back(ast<Select::Elem>(track, std::move(expr), std::move(syms)));
        } while (accept(Tok::Tag::T_comma));
    }

    expect(Tok::Tag::K_FROM, "SELECT expression");
    ASTs<Select::From> froms;
    do {
        froms.emplace_back(parse_from());
    } while (accept(Tok::Tag::T_comma));

    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("WHERE expression") : nullptr;

    ASTs<Expr> groups;
    if (accept(Tok::Tag::K_GROUP)) {
        expect(Tok::Tag::K_BY, "GROUP within SELECT expression");
        do {
            groups.emplace_back(parse_expr("GROUP expression"));
        } while (accept(Tok::Tag::T_comma));
    }

    auto having = accept(Tok::Tag::K_HAVING) ? parse_expr("HAVING expression") : nullptr;

    return ast<Select>(track, all, std::move(elems), std::move(froms), std::move(where), std::move(groups),
                       std::move(having));
}

/// One entry of the `FROM` list. Parsing the table reference at Tok::Prec::Join lets the existing
/// Join operator chain attach here, so `a JOIN b ON c` lands in the FROM list rather than erroring out.
AST<Select::From> Parser::parse_from() {
    auto track = tracker();
    auto expr  = parse_expr("table reference of a FROM clause", Tok::Prec::Join);

    // A correlation name may follow with or without `AS`; without it, only a plain identifier
    // qualifies - a reserved word there would swallow the clause that follows.
    Sym as;
    Syms cols;
    if (accept(Tok::Tag::K_AS))
        as = parse_sym("AS clause");
    else if (ahead().isa(Tok::Tag::V_id))
        as = lex().sym();
    if (as && ahead().isa(Tok::Tag::D_paren_l)) parse_col_list("column name list of a FROM clause", cols);

    return ast<Select::From>(track, std::move(expr), as, std::move(cols));
}

/*
 * Query
 */

AST<Expr> Parser::parse_query_term(std::string_view ctxt) {
    auto track = tracker();
    auto lhs   = parse_expr(ctxt);

    while (ahead().isa(Tok::Tag::K_INTERSECT)) {
        eat(Tok::Tag::K_INTERSECT);
        bool all = (bool)accept(Tok::Tag::K_ALL);
        if (!all) accept(Tok::Tag::K_DISTINCT);
        auto rhs = parse_expr("right-hand side of an INTERSECT expression");
        lhs      = ast<SetOp>(track, std::move(lhs), SetOp::Intersect, all, std::move(rhs));
    }

    return lhs;
}

AST<Expr> Parser::parse_query(std::string_view ctxt) {
    auto track = tracker();
    auto body  = parse_query_term(ctxt);

    while (ahead().isa(Tok::Tag::K_UNION) || ahead().isa(Tok::Tag::K_EXCEPT)) {
        auto tag = lex().isa(Tok::Tag::K_UNION) ? SetOp::Union : SetOp::Except;
        bool all = (bool)accept(Tok::Tag::K_ALL);
        if (!all) accept(Tok::Tag::K_DISTINCT);
        auto rhs = parse_query_term("right-hand side of a UNION or EXCEPT expression");
        body     = ast<SetOp>(track, std::move(body), tag, all, std::move(rhs));
    }

    ASTs<Query::Order> orders;
    if (accept(Tok::Tag::K_ORDER)) {
        expect(Tok::Tag::K_BY, "ORDER within a query expression");
        do {
            auto order_track = tracker();
            auto expr        = parse_expr("sort key of an ORDER BY clause");
            bool desc        = false;
            // ASC and DESC are not reserved words.
            if (ahead().isa(Tok::Tag::V_id) && (ahead().sym() == asc_ || ahead().sym() == desc_))
                desc = lex().sym() == desc_;
            orders.emplace_back(ast<Query::Order>(order_track, std::move(expr), desc));
        } while (accept(Tok::Tag::T_comma));
    }

    AST<Expr> offset;
    if (accept(Tok::Tag::K_OFFSET)) {
        offset = parse_expr("OFFSET clause");
        if (!accept(Tok::Tag::K_ROW)) accept(Tok::Tag::K_ROWS);
    }

    AST<Expr> fetch;
    if (accept(Tok::Tag::K_FETCH)) {
        if (ahead().isa(Tok::Tag::V_id) && (ahead().sym() == first_ || ahead().sym() == next_)) lex();
        fetch = parse_expr("FETCH clause");
        if (!accept(Tok::Tag::K_ROW)) accept(Tok::Tag::K_ROWS);
        expect(Tok::Tag::K_ONLY, "FETCH clause");
    }

    if (orders.empty() && !offset && !fetch) return body;
    return ast<Query>(track, std::move(body), std::move(orders), std::move(offset), std::move(fetch));
}

} // namespace sql
