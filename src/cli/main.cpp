#include <format>
#include <iostream>

#include <fe/cli.h>
#include <fe/term.h>

#include "sql/parser.h"

int main(int argc, char** argv) {
    try {
        // fe::CodeDiag renders a diagnostic when it is *recorded*, so decide on color up front.
        fe::term::resolve_mode();

        // TODO put version number into cmake magic
        bool show_help = false, show_version = false, dump = false;
        std::string input;
        auto cli = fe::Cli("sql", "libsql command-line utility.")
                       .help(show_help)
                       .opt(show_version, "-v", "--version", "Display version info and exit.")
                       .opt(dump, "-d", "--dump", "Dumps the SQL statement again.")
                       .arg(input, "file", "Input file.")
                       .epilog("Use \"-\" as <file> to output to stdout.");

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

        sql::Driver driver;

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
