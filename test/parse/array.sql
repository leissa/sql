-- Array subscripts bind tighter than any operator, so nothing can come between `a` and its index.
SELECT a[1] FROM t;
SELECT a[i + 1] FROM t;
SELECT a[1][2] FROM t;
SELECT t.a[1], s.t.a[1] FROM t;
SELECT f(x)[2] FROM t;
SELECT -a[1], NOT a[1] FROM t;
SELECT a[1] + b[2] FROM t WHERE a[1] = 3;
