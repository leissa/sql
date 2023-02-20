#include "sql/parser.h"

#include <fstream>
#include <iostream>

using namespace std::literals;

namespace sql {

Parser::Parser(Driver& driver, Sym filename, std::istream& stream)
    : lexer_(driver, filename, stream)
    , prev_(lexer_.loc())
    , ahead_(lexer_.lex()) {}

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
    what += Tok::tag2str(tag);
    what += "'";
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

    auto select = parse_expr("select expression");
    expect(Tok::Tag::K_FROM, "select statement");
    auto from  = parse_expr("from expression");
    auto where = accept(Tok::Tag::K_WHERE) ? parse_expr("where expression") : nullptr;
    auto group = accept(Tok::Tag::K_GROUP)
                   ? (expect(Tok::Tag::K_BY, "group within select statement"), parse_expr("group expression"))
                   : nullptr;

    return mk<SelectStmt>(track, all, std::move(select), std::move(from), std::move(where), std::move(group));
}

/*
 * Expr
 */

Ptr<Expr> Parser::parse_expr(std::string_view ctxt, int cur_prec) {
    auto track = tracker();
    auto lhs   = parse_primary_or_unary_expr(ctxt);

    while (false /*true*/ /*is bin op*/) {
        int l_prec = 3, r_prec = 42; // TODO get
        if (l_prec < cur_prec) break;

        auto op  = lex().tag();
        auto rhs = parse_expr("right-hand side of operator '{op}'", r_prec);
        lhs      = mk<BinExpr>(track, std::move(lhs), op, std::move(rhs));
    }

    return lhs;
}

Ptr<Expr> Parser::parse_primary_or_unary_expr(std::string_view ctxt) {
    auto track = tracker();
    // if (auto tok = accept(Tag.K_FALSE)) is not None: return BoolExpr(tok.loc,
    // False  ) if (auto tok = accept(Tag.K_TRUE )) is not None: return
    // BoolExpr(tok.loc, True   ) if (auto tok = accept(Tag.M_SYM  )) is not None:
    // return SymExpr (tok.loc, tok    )

    if (auto tok = accept(Tok::Tag::L_i)) return mk<LitExpr>(track, tok->u64());
    if (auto tok = accept(Tok::Tag::M_id)) return mk<IdExpr>(track, tok->sym());
    // not None: return LitExpr (tok.loc, tok.val)

    // if self.ahead.tag.is_unary():
    // op  = self.lex().tag
    // rhs = self.parse_expr("unary expression", Prec.NOT if op is Tag.K_NOT else
    // Prec.UNARY) return UnaryExpr(t.loc(), op, rhs)

    // if self.accept(Tag.D_PAREN_L):
    // expr = self.parse_expr()
    // self.expect(Tag.D_PAREN_R, "parenthesized expression")
    // return expr

    if (!ctxt.empty()) {
        err("primary or unary expression", ctxt);
        return {};
    }
    unreachable();
}

} // namespace sql
