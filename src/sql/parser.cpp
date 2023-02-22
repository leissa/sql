#include "sql/parser.h"

#include <fstream>
#include <iostream>

using namespace std::literals;

namespace sql {

Parser::Parser(Driver& driver, Sym filename, std::istream& stream)
    : lexer_(driver, filename, stream)
    , prev_(lexer_.loc())
    , ahead_(lexer_.lex())
    , error_(driver.sym("<error>"s)) {}

Tok Parser::lex() {
    auto result = ahead();
    ahead_      = lexer_.lex();
    return result;
}

std::optional<Tok> Parser::accept(Tok::Tag tag) {
    if (tag != ahead().tag()) return std::nullopt;
    return lex();
}

bool Parser::expect(Tok::Tag tag, std::string_view ctxt) {
    if (ahead().tag() == tag) {
        lex();
        return true;
    }

    auto what = "'"s;
    what += Tok::str(tag);
    what += '\'';
    err(what, ctxt);
    return false;
}

void Parser::err(const std::string& what, const Tok& tok, std::string_view ctxt) {
    driver().err(tok.loc(), "expected {}, got '{}' while parsing {}", what, tok, ctxt);
}

/*
 * misc
 */

Ptr<Prog> Parser::parse_prog() {
    auto track = tracker();
    std::deque<Ptr<Stmt>> stmts;

    while (!ahead().isa(Tok::Tag::M_eof)) {
        auto stmt = parse_stmt("program");
        if (stmt->isa<ErrStmt>()) lex(); // consume one token to prevent endless loop
        stmts.emplace_back(std::move(stmt));
        expect(Tok::Tag::T_semicolon, "statment list");
    }

    eat(Tok::Tag::M_eof);
    return mk<Prog>(track, std::move(stmts));
}

Sym Parser::parse_sym(std::string_view ctxt) {
    if (ahead().isa(Tok::Tag::M_id)) return lex().sym();
    err("identifier", ctxt);
    return error_;
}

/*
 * Stmt
 */

Ptr<Stmt> Parser::parse_stmt(std::string_view ctxt) {
    switch (ahead().tag()) {
        case Tok::Tag::K_SELECT: return parse_select_stmt();
        default: break;
    }

    if (!ctxt.empty()) {
        err("statement", ctxt);
        return mk<ErrStmt>(prev_);
    }
    unreachable();
}

Ptr<Stmt> Parser::parse_select_stmt() {
    auto track = tracker();
    eat(Tok::Tag::K_SELECT);

    bool all = true;
    if (accept(Tok::Tag::K_ALL)) { /* do nothing */
    } else if (accept(Tok::Tag::K_DISTINCT)) {
        all = false;
    }

    std::deque<Ptr<Select::Elem>> elems;
    if (accept(Tok::Tag::T_mul)) {
        /* do nothing */
    } else {
        do {
            auto track = tracker();
            auto expr  = parse_expr("elem of a SELECT statement");
            std::deque<Sym> syms;

            if (accept(Tok::Tag::K_AS)) {
                if (ahead().isa(Tok::Tag::D_paren_l)) {
                    parse_list("column name list of AS clause",
                               [&]() { syms.emplace_back(parse_sym("column name within AS clause")); });
                } else {
                    syms.emplace_back(parse_sym("column name within AS clause "));
                }
            }
            elems.emplace_back(mk<Select::Elem>(track, std::move(expr), std::move(syms)));
        } while (accept(Tok::Tag::T_comma));
    }

    expect(Tok::Tag::K_FROM, "SELECT statement");
    auto from  = parse_table("FROM ");
    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("WHERE expression") : nullptr;
    auto group = accept(Tok::Tag::K_GROUP)
                   ? (expect(Tok::Tag::K_BY, "GROUP within SELECT statement"), parse_expr("GROUP expression"))
                   : nullptr;
    auto having = accept(Tok::Tag::K_HAVING) ? parse_expr("HAVING expression") : nullptr;
    // clang-format off
    if (accept(Tok::Tag::K_ROLLUP))   assert(false && "TODO");
    if (accept(Tok::Tag::K_GROUPING)) assert(false && "TODO");
    if (accept(Tok::Tag::K_CUBE))     assert(false && "TODO");
    // clang-format on

    return mk<Select>(track, all, std::move(elems), std::move(from), std::move(where), std::move(group), std::move(having));
}

/*
 * Expr
 */

Ptr<Expr> Parser::parse_expr(std::string_view ctxt, Tok::Prec cur_prec) {
    auto track = tracker();
    auto lhs   = parse_primary_or_unary_expr(ctxt);

    while (auto prec = Tok::bin_prec(ahead().tag())) {
        if (*prec < cur_prec) break;

        auto op  = lex().tag();
        auto rhs = parse_expr("right-hand side of binary expression", *prec);
        lhs      = mk<BinExpr>(track, std::move(lhs), op, std::move(rhs));
    }

    return lhs;
}

Ptr<Expr> Parser::parse_primary_or_unary_expr(std::string_view ctxt) {
    if (ahead().isa(Tok::Tag::M_id)) return parse_id_expr();
    if (auto tok = accept(Tok::Tag::V_int)) return mk<IntVal>(tok->loc(), tok->u64());

    if (ahead().isa(Tok::Tag::K_TRUE) || ahead().isa(Tok::Tag::K_FALSE) || ahead().isa(Tok::Tag::K_UNKNOWN) ||
        ahead().isa(Tok::Tag::K_NULL)) {
        auto tok = lex();
        return mk<SimpleVal>(tok.loc(), tok.tag());
    }

    auto track = tracker();
    if (auto prec = Tok::un_prec(ahead().tag())) {
        auto op = lex().tag();
        return mk<UnExpr>(track, op, parse_expr("operand of unary expression", *prec));
    }

    if (accept(Tok::Tag::D_paren_l)) {
        auto expr = parse_expr("parenthesized expression");
        expect(Tok::Tag::D_paren_r, "parenthesized expression");
        return expr;
    }

    if (!ctxt.empty()) {
        err("primary or unary expression", ctxt);
        return mk<ErrExpr>(prev_);
    }
    unreachable();
}

Ptr<IdExpr> Parser::parse_id_expr() {
    auto track = tracker();
    assert(ahead().isa(Tok::Tag::M_id));

    bool asterisk = false;
    std::deque<Sym> syms;
    syms.emplace_back(lex().sym());

    while (accept(Tok::Tag::T_dot)) {
        if (accept(Tok::Tag::T_mul)) {
            asterisk = true;
            break;
        }
        syms.emplace_back(parse_sym("identifer chain"));
    }

    return mk<IdExpr>(track, std::move(syms), asterisk);
}

/*
 * Table
 */

std::optional<Join::Tag> Parser::parse_join_op() {
    int tag = 0;
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

    if (tag || inner) {
        expect(Tok::Tag::K_JOIN, "JOIN operator");
    } else if (accept(Tok::Tag::K_JOIN)) {
        return Join::Inner;
    } else {
        return {};
    }

    return (Join::Tag)tag;
}

Ptr<Table> Parser::parse_table(std::string_view ctxt) {
    auto track = tracker();
    auto lhs   = parse_primary_or_unary_table(ctxt);

    while (auto tag = parse_join_op()) {
        auto rhs = parse_table("right-hand side of JOIN operator");
        lhs      = mk<Join>(track, std::move(lhs), *tag, std::move(rhs));
    }

    return lhs;
}

Ptr<Table> Parser::parse_primary_or_unary_table(std::string_view ctxt) {
    if (auto tok = accept(Tok::Tag::M_id)) return mk<IdTable>(tok->loc(), tok->sym());

    if (accept(Tok::Tag::D_paren_l)) {
        if (accept(Tok::Tag::K_WITH)) assert(false && "TODO: query expression");
        auto table = parse_table("parenthesized table reference");
        expect(Tok::Tag::D_paren_r, "parenthesized table reference");
        return table;
    }

    //auto track = tracker();
    switch (ahead().tag()) {
        case Tok::Tag::K_LATERAL:
        case Tok::Tag::K_UNNEST:
        case Tok::Tag::K_TABLE:
        case Tok::Tag::K_ONLY:
        case Tok::Tag::K_DELETE:
        case Tok::Tag::K_INSERT:
        case Tok::Tag::K_MERGE:
        case Tok::Tag::K_UPDATE:
        case Tok::Tag::K_FINAL:
        case Tok::Tag::K_NEW:
        case Tok::Tag::K_OLD:
            assert(false && "TODO");
        default:
            break;
    }

    if (!ctxt.empty()) {
        err("primary or unary table reference", ctxt);
        return mk<ErrTable>(prev_);
    }
    unreachable();
}

} // namespace sql
