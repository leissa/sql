#include <format>
#include <iostream>

#include <fe/cli.h>
#include <fe/term.h>

#include "sql/parser.h"

int main(int argc, char** argv) {
    try {
        // fe::CodeDiag renders a diagnostic when it is *recorded*, so decide on color up front.
        fe::term::resolve_mode();

        sql::Driver driver; // outlives the handler below: it writes into the Driver's Diag

        // TODO put version number into cmake magic
        bool show_help = false, show_version = false, dump = false;
        std::string input;

        auto loc_style = [&](const std::string& t) -> std::string {
            // clang-format off
            if (false) {}
            else if (t == "full"  ) driver.diag().loc_style = fe::Loc::Style::Full;
            else if (t == "rowcol") driver.diag().loc_style = fe::Loc::Style::RowCol;
            else if (t == "row"   ) driver.diag().loc_style = fe::Loc::Style::Row;
            else if (t == "msvc"  ) driver.diag().loc_style = fe::Loc::Style::MSVC;
            else return std::format("'{}' is not a location style", t);
            // clang-format on
            return {};
        };

        // clang-format off
        auto cli = fe::Cli("sql", "libsql command-line utility.")
            .help(show_help)
            .opt(show_version           ,          "-v", "--version"   , "Display version info and exit.")
            .opt(dump                   ,          "-d", "--dump"      , "Dumps the SQL statement again.")
            .grp("Diagnostics")
            .opt(loc_style              , "style", ""  , "--loc-style" , "How a diagnostic spells out a source location: full (path:row:col-row:col), rowcol (path:row:col), row (path:row), or msvc (path(row,col)).")
            .opt(driver.diag().no_snippet,         ""  , "--no-snippet", "Does not render the offending source line and caret underneath a diagnostic.")
            .opt(driver.diag().gutter   , "width", ""  , "--gutter"    , "Width of a diagnostic's line-number column.")
            .opt(driver.diag().max_rows , "num"  , ""  , "--max-rows"  , "Maximum number of rows a diagnostic's snippet renders before eliding its middle; 0 elides nothing.")
            .opt(driver.diag().max_errors,"num"  , ""  , "--max-errors", "Maximum number of errors to report before dropping the rest; 0 reports all of them.")
            .opt(driver.diag().werror   ,          ""  , "--werror"    , "Treats warnings as errors.")
            .arg(input, "file", "Input file.")
            .epilog("Use \"-\" as <file> to output to stdout.");
        // clang-format on

        if (auto err = cli.parse(argc, argv)) throw std::invalid_argument(*err);

        if (show_help) {
            std::cout << cli;
            return EXIT_SUCCESS;
        }

        if (show_version) {
            std::cerr << "libsql command-line utility version 0.1\n";
            return EXIT_SUCCESS;
        }

        if (input.empty()) throw std::invalid_argument("no input given");

        const fe::Src* src;
        if (input == "-") {
            src = driver.src().add("<stdin>", fe::SrcMap::slurp(std::cin)).first;
        } else {
            src = driver.src().add(input).first;
            if (!src) {
                // Not an fe::Error - there is no Loc to point at - but cite the file the same way.
                auto msg = [&] { return std::format("error: cannot read file `{}`", input); };
                std::cerr << driver.diag().render(msg) << std::endl;
                return EXIT_FAILURE;
            }
        }

        sql::Parser parser(driver, *src);
        auto prog = parser.parse_prog();

        if (dump) prog->dump();

        if (driver.error().report() != 0) return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "error: unknown exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
