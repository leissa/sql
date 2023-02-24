#pragma once

#include "sql/ast.h"
#include "sql/lexer.h"

namespace sql {

class Parser {
public:
    Parser(Driver&, Sym filename, std::istream& stream);

    Driver& driver() { return lexer_.driver(); }
    Ptr<Prog> parse_prog();

private:
    /// Trick to easily keep track of Loc%ations.
    class Tracker {
    public:
        Tracker(Parser& parser, const Pos& pos)
            : parser_(parser)
            , pos_(pos) {}

        operator Loc() const { return {parser_.prev_.file, pos_, parser_.prev_.finis}; }

    private:
        Parser& parser_;
        Pos pos_;
    };

    Sym parse_sym(std::string_view ctxt);

    Ptr<Expr> parse_expr(std::string_view ctxt, Tok::Prec = Tok::Prec::Bot);
    Ptr<Expr> parse_primary_or_unary_expr(std::string_view ctxt);
    Ptr<Expr> parse_id();
    Ptr<Expr> parse_select();

    std::optional<Join::Tag> parse_join_op();
    Ptr<Table> parse_table(std::string_view ctxt);
    Ptr<Table> parse_primary_or_unary_table(std::string_view ctxt);

    template<class F>
    void parse_list(F f, Tok::Tag delim, Tok::Tag sep = Tok::Tag::T_comma) {
        if (!ahead().isa(delim)) {
            do { f(); } while (accept(sep) && !ahead().isa(delim));
        }
    }

    template<class F>
    void parse_list(std::string ctxt, F f, Tok::Tag delim_l = Tok::Tag::D_paren_l, Tok::Tag sep = Tok::Tag::T_comma) {
        expect(delim_l, ctxt);
        auto delim_r = (Tok::Tag)((int)delim_l + 1);
        parse_list(f, delim_r, sep);
        expect(delim_r, std::string("closing delimiter of a ") + ctxt);
    }

    /// Factory method to build a Tracker.
    Tracker tracker() { return Tracker(*this, ahead().loc().begin); }

    /// Invoke Lexer to retrieve next Tok%en.
    Tok lex();

    /// Get lookahead.
    Tok ahead() const { return ahead_; }

    /// If Parser::ahead() is a @p tag, Parser::lex(), and return `true`.
    std::optional<Tok> accept(Tok::Tag tag);

    /// Parser::lex Parser::ahead() which must be a @p tag.
    /// Issue Parser::err%or with @p ctxt otherwise.
    bool expect(Tok::Tag tag, std::string_view ctxt);

    /// Consume Parser::ahead which must be a @p tag; `assert`s otherwise.
    Tok eat([[maybe_unused]] Tok::Tag tag) {
        assert(tag == ahead().tag() && "internal parser error");
        return lex();
    }

    /// Issue an error message of the form:
    /// `expected <what>, got '<tok>' while parsing <ctxt>`
    void err(const std::string& what, const Tok& tok, std::string_view ctxt);

    /// Same above but uses Parser::ahead() as Tok%en.
    void err(const std::string& what, std::string_view ctxt) { err(what, ahead(), ctxt); }

    Lexer lexer_;
    Loc prev_;
    Tok ahead_;
    Sym error_;
};

} // namespace sql
