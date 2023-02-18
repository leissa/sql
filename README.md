# SQL

A small SQL parser.

## Building

If you have a [GitHub account setup with SSH](https://docs.github.com/en/authentication/connecting-to-github-with-ssh), just do this:
```
git clone --recurse-submodules git@github.com:leissa/sql.git
```
Otherwise, clone via HTTPS:
```
git clone --recurse-submodules https://github.com/leissa/sql.git
```
Then, build with:
```
cd sql
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j $(nproc)
```
For a `Release` build simply use `-DCMAKE_BUILD_TYPE=Release`.
