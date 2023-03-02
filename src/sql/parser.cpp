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
    Ptrs<Expr> exprs;

    while (!ahead().isa(Tok::Tag::M_eof)) {
        auto expr = parse_expr("program");
        if (expr->isa<ErrExpr>()) lex(); // consume one token to prevent endless loop
        exprs.emplace_back(std::move(expr));
        expect(Tok::Tag::T_semicolon, "expression list");
    }

    eat(Tok::Tag::M_eof);
    return mk<Prog>(track, std::move(exprs));
}

Sym Parser::parse_sym(std::string_view ctxt) {
    if (ahead().isa(Tok::Tag::M_id)) return lex().sym();
    err("identifier", ctxt);
    return error_;
}

/*
 * Type
 */
Ptr<Type> Parser::parse_type(std::string_view ctxt) {
    switch (ahead().tag()) {
        case Tok::Tag::K_INT:
        case Tok::Tag::K_INTEGER:
        case Tok::Tag::K_SMALLINT:
        case Tok::Tag::K_BIGINT:
        case Tok::Tag::K_BOOLEAN:
        case Tok::Tag::K_DATE: {
            auto tok = lex();
            return mk<SimpleType>(tok.loc(), tok.tag(), false);
        }
        case Tok::Tag::K_NUMERIC: assert(false && "TODO");
        case Tok::Tag::K_DECIMAL: assert(false && "TODO");
        case Tok::Tag::K_DEC: assert(false && "TODO");
        case Tok::Tag::K_TIME: assert(false && "TODO");
        case Tok::Tag::K_TIMESTAMP: assert(false && "TODO");
        default: break;
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

    if (tag || inner) {
        expect(Tok::Tag::K_JOIN, "JOIN operator");
    } else if (accept(Tok::Tag::K_JOIN)) {
        return Join::Inner;
    } else {
        return {};
    }

    return (Join::Tag)tag;
}

Ptr<Expr> Parser::parse_expr(std::string_view ctxt, Tok::Prec cur_prec) {
    auto track = tracker();
    auto lhs   = parse_primary_or_unary_expr(ctxt);

    while (true) {
        if (auto prec = Tok::bin_prec(ahead().tag())) {
            if (*prec < cur_prec) break;

            auto op  = lex().tag();
            auto rhs = parse_expr("right-hand side of binary expression", *prec);
            lhs      = mk<BinExpr>(track, std::move(lhs), op, std::move(rhs));
        } else if (auto tag = parse_join_op()) {
            auto rhs = parse_expr("right-hand side of JOIN operator", Tok::Prec::Join);

            Join::Spec spec;
            if (accept(Tok::Tag::K_ON)) {
                spec = parse_expr("search condition for an ON clause of a JOIN specification");
            } else if (accept(Tok::Tag::K_USING)) {
                Syms syms;
                parse_list("join column list for a USING clause of a JOIN specification",
                        [&]() { syms.emplace_back(parse_sym("colunm name list")); });
                spec = std::move(syms);
            }

            lhs = mk<Join>(track, std::move(lhs), *tag, std::move(rhs), std::move(spec));
        } else {
            break;
        }
    }




    return lhs;
}

Ptr<Expr> Parser::parse_primary_or_unary_expr(std::string_view ctxt) {
    switch (ahead().tag()) {
        case Tok::Tag::M_id: return parse_id();
        case Tok::Tag::K_CREATE: return parse_create();
        case Tok::Tag::K_SELECT: return parse_select();
        case Tok::Tag::V_int: {
            auto tok = lex();
            return mk<IntVal>(tok.loc(), tok.u64());
        }
        case Tok::Tag::K_TRUE:
        case Tok::Tag::K_FALSE:
        case Tok::Tag::K_UNKNOWN:
        case Tok::Tag::K_NULL: {
            auto tok = lex();
            return mk<SimpleVal>(tok.loc(), tok.tag());
        }
        default: break;
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

Ptr<Expr> Parser::parse_id() {
    auto track = tracker();
    assert(ahead().isa(Tok::Tag::M_id));

    bool asterisk = false;
    Syms syms;
    syms.emplace_back(lex().sym());

    while (accept(Tok::Tag::T_dot)) {
        if (accept(Tok::Tag::T_mul)) {
            asterisk = true;
            break;
        }
        syms.emplace_back(parse_sym("identifer chain"));
    }

    return mk<Id>(track, std::move(syms), asterisk);
}

Ptr<Expr> Parser::parse_create() {
    auto track = tracker();
    eat(Tok::Tag::K_CREATE);

    expect(Tok::Tag::K_TABLE, "CREATE expression");
    auto sym = parse_sym("table name");
    Ptrs<Create::Elem> elems;
    parse_list("table element list", [&]() {
        auto track = tracker();
        auto sym   = parse_sym("column name");
        auto type  = parse_type("column type");
        elems.emplace_back(mk<Create::Elem>(track, sym, std::move(type)));
    });
    return mk<Create>(track, sym, std::move(elems));
}

Ptr<Expr> Parser::parse_select() {
    auto track = tracker();
    eat(Tok::Tag::K_SELECT);

    bool all = true;
    if (accept(Tok::Tag::K_ALL)) { /* do nothing */
    } else if (accept(Tok::Tag::K_DISTINCT)) {
        all = false;
    }

    Ptrs<Select::Elem> elems;
    if (accept(Tok::Tag::T_mul)) {
        /* do nothing */
    } else {
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
            elems.emplace_back(mk<Select::Elem>(track, std::move(expr), std::move(syms)));
        } while (accept(Tok::Tag::T_comma));
    }

    expect(Tok::Tag::K_FROM, "SELECT expression");
    Ptrs<Expr> froms;
    do { froms.emplace_back(parse_expr("FROM clause")); } while (accept(Tok::Tag::T_comma));
    auto where  = accept(Tok::Tag::K_WHERE) ? parse_expr("WHERE expression") : nullptr;
    auto group  = accept(Tok::Tag::K_GROUP)
                    ? (expect(Tok::Tag::K_BY, "GROUP within SELECT expression"), parse_expr("GROUP expression"))
                    : nullptr;
    auto having = accept(Tok::Tag::K_HAVING) ? parse_expr("HAVING expression") : nullptr;

    return mk<Select>(track, all, std::move(elems), std::move(froms), std::move(where), std::move(group),
                      std::move(having));
}

} // namespace sql
