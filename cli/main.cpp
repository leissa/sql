#include "sql/parser.h"

#include <cstring>
#include <iostream>
#include <fstream>

#include <lyra/lyra.hpp>

using namespace std::literals;

int main(int argc, char** argv) {
    try {
        // TODO put version number into cmake magic
        static const auto version = "libsql command-line utility version 0.1\n";
        bool show_help = false;
        bool show_version = false;
        std::string input;

        // clang-format off
        auto cli = lyra::cli()
            | lyra::help(show_help)
            | lyra::opt(show_version       )["-v"]["--version"]("Display version info and exit.")
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

        std::ifstream ifs(input);
        if (!ifs) {
            //errln("error: cannot read file '{}'", input);
            return EXIT_FAILURE;
        }

        sql::Driver driver;
        sql::Ptr<sql::Expr> expr;
        if (input == "-") {
            sql::Parser parser(driver, driver.symtab.add("<stdin>"s), std::cin);
            expr = parser.parse();
        } else {
            std::ifstream ifs(input);
            sql::Parser parser(driver, driver.symtab.add(std::move(input)), ifs);
            expr = parser.parse();
        }

        //if (num_errors != 0) {
            //std::cerr << num_errors << " error(s) encountered" << std::endl;
            //return EXIT_FAILURE;
        //}
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "error: unknown exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
