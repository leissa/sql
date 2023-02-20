#include "sql/parser.h"

#include <fstream>
#include <iostream>

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

bool Parser::accept(Tok::Tag tag) {
    if (tag != ahead().tag()) return false;
    lex();
    return true;
}

bool Parser::expect(Tok::Tag tag, const char* ctxt) {
    if (ahead().tag() == tag) {
        lex();
        return true;
    }

    err(std::string("'") + Tok::tag2str(tag) + std::string("'"), ctxt);
    return false;
}

void Parser::err(const std::string& what, const Tok& tok, const char* ctxt) {
    driver().err(tok.loc(), "expected '{}', got '{}' while parsing {}", what, tok, ctxt);
}

Sym Parser::parse_sym(const char* ctxt) {
    if (ahead().isa(Tok::Tag::M_id)) return lex().sym();
    err("identifier", ctxt);
    return driver().sym("<error>");
}

Ptr<Expr> Parser::parse_expr(const char* ctxt, int cur_prec) {
    auto track = tracker();
    auto lhs   = parse_primary_or_unary_expr(ctxt);

    while (true /*is bin op*/) {
        int l_prec = 3, r_prec = 42; // TODO get
        if (l_prec < cur_prec) break;

        auto op  = lex().tag();
        auto rhs = parse_expr("right-hand side of operator '{op}'", r_prec);
        lhs      = mk<BinExpr>(track, std::move(lhs), op, std::move(rhs));
    }

    return lhs;
}

Ptr<Expr> Parser::parse_primary_or_unary_expr(const char* ctxt) {
    auto track = tracker();
    // if (auto tok = accept(Tag.K_FALSE)) is not None: return BoolExpr(tok.loc,
    // False  ) if (auto tok = accept(Tag.K_TRUE )) is not None: return
    // BoolExpr(tok.loc, True   ) if (auto tok = accept(Tag.M_SYM  )) is not None:
    // return SymExpr (tok.loc, tok    ) if (auto tok = accept(Tag.M_LIT  )) is
    // not None: return LitExpr (tok.loc, tok.val)

    // if self.ahead.tag.is_unary():
    // op  = self.lex().tag
    // rhs = self.parse_expr("unary expression", Prec.NOT if op is Tag.K_NOT else
    // Prec.UNARY) return UnaryExpr(t.loc(), op, rhs)

    // if self.accept(Tag.D_PAREN_L):
    // expr = self.parse_expr()
    // self.expect(Tag.D_PAREN_R, "parenthesized expression")
    // return expr

    // if ctxt is not None:
    // self.err("primary or unary expression", ctxt)
    // return ErrExpr(self.ahead.loc)
    assert(false);
}

} // namespace sql
