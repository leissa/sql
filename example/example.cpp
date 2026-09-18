#include <iostream>

#include <sql/sql.h>

int main(int argc, char** argv) {
    auto query = argc > 1 ? std::string(argv[1]) : std::string("SELECT a, b FROM t WHERE a > 42;");
    auto res   = sql::parse(query);

    if (!res) {
        res.report(std::cerr);
        return EXIT_FAILURE;
    }

    std::cout << "parsed " << res.stmts().size() << " statement(s)\n";

    for (auto stmt : res.stmts()) {
        if (auto select = stmt->isa<sql::Select>()) {
            std::cout << "  SELECT of " << select->elems().size() << " element(s) from " << select->froms().size()
                      << " table reference(s)\n";
            if (auto where = select->where()) std::cout << "    WHERE " << *where << '\n';
        } else {
            std::cout << "  " << *stmt << '\n';
        }
    }

    std::cout << "\nand back as SQL:\n" << res << '\n';
    return EXIT_SUCCESS;
}
