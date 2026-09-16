#pragma once

#include <array>

#include <fe/parser.h>

#include "sql/ast.h"
#include "sql/lexer.h"

namespace sql {

/// Uses a lookahead of 2: telling `name(` (a Func call) from a plain Id, `(SELECT` from a
/// parenthesized expression list, or `NOT LIKE` from a unary `NOT` all need to peek one token ahead.
class Parser : public fe::Parser<Tok, Tok::Tag, 2, Parser> {
public:
    Parser(Driver&, const fe::Src&);

    Driver& driver() { return lexer_.driver(); } ///< fe::Parser's default diagnostics go to its Driver::error.
    AST<Prog> parse_prog();
    Lexer& lexer() { return lexer_; }

private:
    template<class T, class... Args>
    auto ast(Args&&... args) {
        return driver().ast<T>(std::forward<Args&&>(args)...);
    }

    /// @name Non-reserved words
    /// SQL's *non-reserved words* only mean something in one particular place and are plain
    /// identifiers everywhere else, so they lex as Tok::Tag::V_id and recognizing one is a Sym
    /// comparison rather than a Tok::Tag check.
    ///@{
    Sym non_key(NonKey nk) const { return non_keys_[(size_t)nk]; }
    bool isa_non_key(NonKey) const;
    bool accept_non_key(NonKey);
    void expect_non_key(NonKey, fe::Cite ctxt);
    ///@}

    /// Accepts a reserved word as an identifier, too: the standard's reserved-word list is far
    /// larger than what real-world SQL treats as reserved. A later check sorts out the illegal ones.
    Sym parse_sym(fe::Cite ctxt);
    bool isa_sym() const;              ///< Would Parser::parse_sym succeed?
    bool isa_like(size_t i = 0) const; ///< Is the token @p i ahead `LIKE`, `ILIKE`, or `SIMILAR`?
    Syms parse_name(fe::Cite ctxt);    ///< A possibly qualified name: `t`, `s.t`, `c.s.t`.

    AST<Type> parse_type(fe::Cite ctxt);
    AST<Interval> parse_interval(); ///< The `<field> [(p)] [TO <field> [(p)]]` of an `INTERVAL`.

    /// @name Statements
    ///@{
    AST<Expr> parse_stmt(); ///< One `<direct SQL statement>` - what Parser::parse_prog is a list of.
    AST<Expr> parse_create();
    AST<Expr> parse_create_table(Tracker, bool temporary);
    AST<Expr> parse_create_view(Tracker, bool replace);
    AST<Expr> parse_create_index(Tracker, bool unique);
    AST<Expr> parse_create_schema(Tracker);
    AST<Expr> parse_alter();
    AST<Expr> parse_drop();
    AST<Expr> parse_truncate();
    AST<Expr> parse_transact();
    AST<Expr> parse_insert();
    AST<Expr> parse_update();
    AST<Expr> parse_delete();
    AST<Expr> parse_prepare();
    AST<Expr> parse_execute();
    AST<Expr> parse_deallocate();
    AST<Expr> parse_show();
    AST<Expr> parse_copy();
    ///@}

    /// @name Query expressions
    /// A query expression is *not* a value expression: only a Parser::parse_query_primary starts one.
    /// @p value_ok widens that to any value expression - which is what makes `(a, b)` and
    /// `(SELECT ...)` share a single parenthesized syntax.
    ///@{
    AST<Expr> parse_query(fe::Cite ctxt, bool value_ok = true);
    AST<Expr> parse_query_term(fe::Cite ctxt, bool value_ok); ///< `INTERSECT` binds tighter than `UNION`.
    AST<Expr> parse_query_primary(fe::Cite ctxt, bool value_ok);
    AST<Expr> parse_select();
    AST<Expr> parse_values();
    AST<Expr> parse_table();
    AST<Expr> parse_group_elem(); ///< A `GROUP BY` element - `ROLLUP`, `CUBE`, `GROUPING SETS`, or an Expr.
    AST<Lock> parse_lock();       ///< One `FOR UPDATE`-style row-locking clause.
    ///@}

    /// @name Table references
    /// The `FROM` clause has a grammar of its own: a *table reference* is a chain of `JOIN`s over
    /// *table factors*, and a correlation name binds to one factor - not to the chain. Keeping this
    /// out of Parser::parse_expr is what makes `a AS x JOIN b AS y ON ...` come out right.
    ///@{
    AST<Expr> parse_table_ref();    ///< `<factor> {<join op> <factor> [ON ...|USING ...]}`
    AST<Expr> parse_table_factor(); ///< A table primary, plus the correlation name that may follow.
    AST<Expr> parse_table_primary();
    std::optional<Join::Tag> parse_join_op();
    ///@}

    /// @name Value expressions
    ///@{
    AST<Expr> parse_expr(fe::Cite ctxt, Tok::Prec = Tok::Prec::Bot);
    AST<Expr> parse_primary_or_unary_expr(fe::Cite ctxt);
    AST<Expr> parse_between(Tracker, AST<Expr>, bool negated);
    AST<Expr> parse_like(Tracker, AST<Expr>, bool negated);
    AST<Expr> parse_id_or_func(); ///< A qualified name - and, if a `(` follows, the call it introduces.
    AST<Expr> parse_func(Tracker, const Syms&);
    AST<Expr> parse_case();
    AST<Expr> parse_cast();
    AST<Expr> parse_special_func(); ///< `EXTRACT`, `SUBSTRING`, `TRIM`, `POSITION`, and `OVERLAY`.
    ///@}

    /// @name Bits and pieces
    ///@{
    AST<Order> parse_order();
    AST<Window> parse_window();
    AST<Frame> parse_frame();
    AST<Frame::Bound> parse_frame_bound();
    AST<Constraint> parse_constraint(bool table_level);
    AST<Create::Elem> parse_col_def();
    Constraint::Action parse_ref_action();
    Behavior parse_behavior();       ///< A trailing `CASCADE`/`RESTRICT`, if there is one.
    bool parse_if_exists(bool not_); ///< The non-standard but ubiquitous `IF [NOT] EXISTS` guard.

    /// Parses a parenthesized, comma-separated column name list into @p syms.
    void parse_col_list(fe::Cite ctxt, Syms& syms);
    ///@}

    /// Parses a @p sep-separated sequence of items via @p f up to - but not including - @p delim.
    /// Whatever fits nowhere in between is discarded - unless an enclosing context anchors it.
    template<class F>
    void parse_seq(fe::Cite ctxt, F f, Tok::Tag delim, Tok::Tag sep = Tok::Tag::T_comma) {
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
    void parse_list(fe::Cite ctxt, F f, Tok::Tag delim_l = Tok::Tag::D_paren_l, Tok::Tag sep = Tok::Tag::T_comma) {
        expect(delim_l, ctxt);
        auto delim_r = (Tok::Tag)((int)delim_l + 1);
        auto _       = anchor(delim_r);
        parse_seq(ctxt, f, delim_r, sep);
        expect(delim_r, "closing delimiter of a {}", ctxt);
    }

    Lexer lexer_;
    Sym sym_error_;                                 ///< Stands in for a Sym that failed to parse.
    const std::array<Sym, Num_Non_Keys>& non_keys_; ///< The Driver's table; see Parser::non_key.

    friend class fe::Parser<Tok, Tok::Tag, 2, Parser>;
};

} // namespace sql
