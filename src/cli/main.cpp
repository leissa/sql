#include <cstring>

#include <iostream>

#include <lyra/lyra.hpp>

#include "sql/parser.h"

using namespace std::literals;

int main(int argc, char** argv) {
    try {
        // TODO put version number into cmake magic
        static const auto version = "libsql command-line utility version 0.1\n";
        bool show_help            = false;
        bool show_version         = false;
        bool dump                 = false;
        std::string input;

        // clang-format off
        auto cli = lyra::cli()
            | lyra::help(show_help)
            | lyra::opt(show_version       )["-v"]["--version"]("Display version info and exit.")
            | lyra::opt(dump               )["-d"]["--dump"   ]("Dumps the SQL statement again.")
            | lyra::arg(input,       "file")                   ("Input file.")
            ;
        // clang-format on

        if (auto result = cli.parse({argc, argv}); !result) throw std::invalid_argument(result.message());

        if (show_help) {
            std::cout << cli << std::endl;
            std::cout << "Use \"-\" as <file> to output to stdout." << std::endl;
            return EXIT_SUCCESS;
        }

        if (show_version) {
            std::cerr << version;
            return EXIT_SUCCESS;
        }

        if (input.empty()) throw std::invalid_argument("error: no input given");

        sql::Driver driver;

        const fe::Src* src;
        if (input == "-") {
            src = driver.src().add("<stdin>", fe::SrcMap::slurp(std::cin)).first;
        } else {
            src = driver.src().add(input).first;
            if (!src) {
                std::cerr << "error: cannot read file '" << input << "'" << std::endl;
                return EXIT_FAILURE;
            }
        }

        fe::Error err(driver);
        sql::Parser parser(driver, err, *src);
        auto prog = parser.parse_prog();

        if (dump) prog->dump();

        if (err.report() != 0) return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "error: unknown exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
