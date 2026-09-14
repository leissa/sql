/// Times the parser over a corpus of `.sql` files.
///
/// Three modes, because they answer different questions:
/// * `--each` (the default) builds a Driver and a Parser per file, which is what an embedding that
///   parses one query at a time pays. Over `test/job/`, whose files hold a single query each, this
///   is the per-query cost - setup included.
/// * `--once` concatenates the whole corpus into one source and parses it with a single Parser.
///   Setup is paid once, so what is left is parsing throughput.
/// * `--lex` runs the Lexer over that same single source and throws the tokens away, which splits
///   lexing off from the parsing on top of it.
///
/// The difference between the two is the per-Parser setup: a Lexer interns every reserved word into
/// its Driver's SymPool when it is built.
///
///     ./build/bin/bench test/job/*.sql
///     ./build/bin/bench --once --iters 20 test/job/*.sql test/tpch/*.sql
///     ./build/bin/bench --lex test/job/*.sql
#include <chrono>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <print>
#include <string>
#include <vector>

#include <fe/src.h>

#include "sql/parser.h"

using namespace sql;

namespace {

/// Parses @p srcs, each with a Parser of its own, and returns the number of statements seen.
size_t parse(Driver& driver, const std::vector<const fe::Src*>& srcs) {
    size_t stmts = 0;
    for (const auto* src : srcs) {
        Parser parser(driver, *src);
        stmts += parser.parse_prog()->exprs().size();
    }
    return stmts;
}

} // namespace

int main(int argc, char** argv) {
    bool once = false, lex = false;
    int iters = 10;
    std::vector<std::string> paths;

    for (int i = 1; i != argc; ++i) {
        auto arg = std::string_view(argv[i]);
        if (arg == "--once") {
            once = true;
        } else if (arg == "--lex") {
            once = lex = true;
        } else if (arg == "--each") {
            once = lex = false;
        } else if (arg == "--iters" && i + 1 != argc) {
            iters = std::atoi(argv[++i]);
        } else if (arg.starts_with("-")) {
            std::println(std::cerr, "usage: {} [--each|--once|--lex] [--iters <n>] <file.sql>...", argv[0]);
            return EXIT_FAILURE;
        } else {
            paths.emplace_back(arg);
        }
    }

    if (paths.empty() || iters <= 0) {
        std::println(std::cerr, "usage: {} [--each|--once|--lex] [--iters <n>] <file.sql>...", argv[0]);
        return EXIT_FAILURE;
    }

    // Read every file up front: the benchmark times the parser, not the file system.
    std::vector<std::string> bufs;
    size_t bytes = 0;
    for (const auto& path : paths) {
        fe::Driver reader; // just to borrow its SrcMap for the read
        const auto* src = reader.src().add(path).first;
        if (!src) {
            std::println(std::cerr, "error: cannot read file `{}`", path);
            return EXIT_FAILURE;
        }
        bufs.emplace_back(src->buf());
        bytes += bufs.back().size();
    }

    std::string all;
    if (once)
        for (const auto& buf : bufs) {
            all += buf;
            all += '\n';
        }

    // Name every source up front too - formatting one is no part of what is being measured.
    std::vector<std::string> names;
    for (size_t i = 0; i != bufs.size(); ++i)
        names.emplace_back(std::format("<bench-{}>", i));

    size_t stmts = 0, toks = 0;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i != iters; ++i) {
        // A fresh Driver each round, so neither the arena nor the SymPool grows across iterations.
        Driver driver;
        std::vector<const fe::Src*> srcs;

        if (once)
            srcs.emplace_back(driver.src().add("<bench>", all).first);
        else
            for (size_t j = 0; j != bufs.size(); ++j)
                srcs.emplace_back(driver.src().add(names[j], bufs[j]).first);

        if (lex) {
            Lexer lexer(driver, *srcs.front());
            for (toks = 0; !lexer.lex().isa(Tok::Tag::EoF);)
                ++toks;
        } else {
            stmts = parse(driver, srcs);
        }

        if (!driver.error().ok()) {
            std::println(std::cerr, "error: the corpus does not parse cleanly");
            driver.error().report();
            return EXIT_FAILURE;
        }
    }

    auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count();
    auto sec = double(ns) / 1e9;

    auto mode = lex ? "--lex" : once ? "--once" : "--each";

    if (lex) {
        std::println("{:<6} {} files, {} tokens, {} bytes, {} iterations", mode, paths.size(), toks, bytes, iters);
        std::println("{:<6} {:8.1f} ns/token  {:8.1f} MB/s  {:8.3f} s total", "", double(ns) / (iters * toks),
                     double(bytes) * iters / sec / 1e6, sec);
    } else {
        std::println("{:<6} {} files, {} statements, {} bytes, {} iterations", mode, paths.size(), stmts, bytes, iters);
        std::println("{:<6} {:8.2f} us/statement  {:8.1f} MB/s  {:8.3f} s total", "",
                     double(ns) / (iters * stmts) / 1e3, double(bytes) * iters / sec / 1e6, sec);
    }
    return EXIT_SUCCESS;
}
