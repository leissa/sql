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

Sym Parser::parse_sym(std::string_view ctxt) {
    if (ahead().isa(Tok::Tag::M_id)) return lex().sym();
    err("identifier", ctxt);
    return driver().sym("<error>");
}

/*
 * Stmt
 */

Ptr<Stmt> Parser::parse_stmt() {
    // TODO
    return parse_select_stmt();
}

Ptr<Stmt> Parser::parse_select_stmt() {
    auto track = tracker();
    eat(Tok::Tag::K_SELECT);

    bool all = true;
    if (accept(Tok::Tag::K_ALL)) { /* do nothing */
    } else if (accept(Tok::Tag::K_DISTINCT)) {
        all = false;
    }

    std::deque<Ptr<Select::Item>> items;
    if (accept(Tok::Tag::T_mul)) {
        /* do nothing */
    } else {
        // parse_list([&]() { return
    }

    auto select = parse_expr("select expression");
    expect(Tok::Tag::K_FROM, "select statement");
    auto from  = parse_expr("from expression");
    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("where expression") : nullptr;
    auto group = accept(Tok::Tag::K_GROUP)
                   ? (expect(Tok::Tag::K_BY, "group within select statement"), parse_expr("group expression"))
                   : nullptr;

    return mk<Select>(track, all, std::move(select), std::move(from), std::move(where), std::move(group));
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
    if (auto tok = accept(Tok::Tag::L_i)) return mk<LitExpr>(tok->loc(), tok->u64());
    if (ahead().isa(Tok::Tag::M_id)) return parse_id_expr();

    if (ahead().isa(Tok::Tag::K_TRUE) || ahead().isa(Tok::Tag::K_FALSE) || ahead().isa(Tok::Tag::K_UNKNOWN)) {
        auto tok = lex();
        return mk<TruthValueExpr>(tok.loc(), tok.tag());
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
    std::deque<Sym> syms;
    do {
        syms.emplace_back(parse_sym("identifer chain"));
    } while (accept(Tok::Tag::T_dot));
    return mk<IdExpr>(track, std::move(syms));
}

} // namespace sql
