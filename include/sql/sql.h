#pragma once

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "sql/parser.h"

namespace sql {

/// What sql::parse yields: the AST, and the diagnostics the Parser recorded on the way.
/// Owns the Driver - and with it the Arena the AST lives in and the SrcMap its Loc%s point into -
/// so the AST stays valid exactly as long as this Result does.
class Result {
public:
    Result(Result&&)            = default;
    Result& operator=(Result&&) = default;

    /// Did everything parse without an error?
    explicit operator bool() const { return driver_->error().ok(); }

    /// @name AST
    ///@{
    AST<Prog> prog() const { return prog_; }
    auto stmts() const { return prog_->exprs(); } ///< The statements, one per `;`.
    ///@}

    /// @name Diagnostics
    /// The Parser recovers, so a Result with errors still has an AST - just one with holes in it.
    ///@{
    const std::vector<fe::Error::Msg>& errors() const { return driver_->error().msgs(); }
    size_t num_errors() const { return driver_->error().num_errors(); }
    size_t num_warnings() const { return driver_->error().num_warnings(); }

    /// Streams every diagnostic to @p os and claims it.
    /// @returns the number of errors that were reported.
    size_t report(std::ostream& os = std::cerr) { return driver_->error().report(os); }
    ///@}

    Driver& driver() { return *driver_; } ///< For the knobs in Driver::diag, or to keep parsing into it.

    /// Streams the AST back out as SQL.
    friend std::ostream& operator<<(std::ostream& os, const Result& res) { return os << res.prog_; }

private:
    explicit Result(std::unique_ptr<Driver>&& driver)
        : driver_(std::move(driver)) {}

    std::unique_ptr<Driver> driver_; ///< fe::Driver is not movable, so a movable Result holds it here.
    AST<Prog> prog_;

    friend Result parse(std::string, std::filesystem::path);
};

/// @name Parse
/// @p name only labels the input in a diagnostic.
///@{
Result parse(std::string sql, std::filesystem::path name = "<input>");
Result parse(std::istream&, std::filesystem::path name = "<stdin>");

/// @throws std::filesystem::filesystem_error if @p path cannot be read.
Result parse_file(const std::filesystem::path& path);
///@}

} // namespace sql

#ifndef DOXYGEN
template<>
struct std::formatter<sql::Result, char> : fe::ostream_formatter {};
#endif
