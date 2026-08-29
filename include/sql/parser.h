#pragma once

#include <fe/parser.h>

#include "sql/ast.h"
#include "sql/lexer.h"

namespace sql {

/// Uses a lookahead of 2: telling `name(` (a Func call) from a plain Id, `(SELECT` from a
/// parenthesized expression list, or `NOT LIKE` from a unary `NOT` all need to peek one token ahead.
class Parser : public fe::Parser<Tok, Tok::Tag, 2, Parser> {
public:
    Parser(Driver&, fe::Error&, const fe::Src&);

    Driver& driver() { return lexer_.driver(); }
    AST<Prog> parse_prog();
    Lexer& lexer() { return lexer_; }

private:
    template<class T, class... Args>
    auto ast(Args&&... args) {
        return driver().ast<T>(std::forward<Args&&>(args)...);
    }

    /// Accepts a reserved word as an identifier, too: the standard's reserved-word list is far
    /// larger than what real-world SQL treats as reserved. A later check sorts out the illegal ones.
    Sym parse_sym(std::string_view ctxt);
    bool isa_sym() const; ///< Would Parser::parse_sym succeed?

    AST<Type> parse_type(std::string_view ctxt);

    AST<Expr> parse_query(std::string_view ctxt);      ///< Set operations plus `ORDER BY`/`OFFSET`/`FETCH`.
    AST<Expr> parse_query_term(std::string_view ctxt); ///< `INTERSECT` binds tighter than `UNION`/`EXCEPT`.
    AST<Expr> parse_expr(std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    AST<Expr> parse_primary_or_unary_expr(std::string_view ctxt);
    AST<Expr> parse_between(Tracker, AST<Expr>&&, bool negated);
    AST<Expr> parse_id();
    AST<Expr> parse_create();
    AST<Expr> parse_drop();
    AST<Expr> parse_select();
    AST<Expr> parse_insert();
    AST<Expr> parse_update();
    AST<Expr> parse_delete();
    AST<Expr> parse_func();
    AST<Expr> parse_case();
    AST<Expr> parse_cast();
    AST<Select::From> parse_from();
    AST<Constraint> parse_constraint(bool table_level);
    std::optional<Join::Tag> parse_join_op();

    /// Parses a parenthesized, comma-separated column name list into @p syms.
    void parse_col_list(std::string ctxt, Syms& syms);

    /// Parses a @p sep-separated sequence of items via @p f up to - but not including - @p delim.
    /// Whatever fits nowhere in between is discarded - unless an enclosing context anchors it.
    template<class F>
    void parse_seq(std::string_view ctxt, F f, Tok::Tag delim, Tok::Tag sep = Tok::Tag::T_comma) {
        if (!ahead().isa(delim)) {
            do {
                f();
                recover([delim, sep](Tok::Tag tag) { return tag != delim && tag != sep && tag != Tok::Tag::EoF; },
                        ctxt);
            } while (accept(sep) && !ahead().isa(delim));
        }
    }

    /// As above, but the sequence is enclosed in @p delim_l and its matching closing delimiter.
    /// The latter stays anchored while the items are parsed and is expected once they are done.
    template<class F>
    void parse_list(std::string ctxt, F f, Tok::Tag delim_l = Tok::Tag::D_paren_l, Tok::Tag sep = Tok::Tag::T_comma) {
        expect(delim_l, ctxt);
        auto delim_r = (Tok::Tag)((int)delim_l + 1);
        auto _       = anchor(delim_r, "closing delimiter of a {}", ctxt);
        parse_seq(ctxt, f, delim_r, sep);
    }

    /// Issue an error message of the form:
    /// `expected <what>, got '<tok>' while parsing <ctxt>`
    void err(const std::string& what, const Tok& tok, std::string_view ctxt);

    /// Same above but uses Parser::ahead() as Tok%en.
    void err(const std::string& what, std::string_view ctxt) { err(what, ahead(), ctxt); }

    void syntax_err(Tok::Tag tag, std::string_view ctxt) {
        std::string msg("'");
        msg.append(Tok::str(tag)).append("'");
        err(msg, ctxt);
    }

    /// Parser::recover discarded @p tok because no enclosing context was waiting for it.
    void unanchored_err(Tok tok, std::string_view ctxt) {
        err_.error(tok.loc(), "ignoring unexpected '{}' while parsing {}", tok, ctxt);
    }

    Lexer lexer_;
    fe::Error& err_;
    Sym error_;
    /// `KEY`, `ASC`, `DESC`, `FIRST`, and `NEXT` are *non-reserved* words: they lex as identifiers,
    /// so recognizing them within `PRIMARY KEY`, `ORDER BY`, and `FETCH` takes a symbol comparison.
    Sym key_;
    Sym asc_;
    Sym desc_;
    Sym first_;
    Sym next_;

    friend class fe::Parser<Tok, Tok::Tag, 2, Parser>;
};

} // namespace sql
