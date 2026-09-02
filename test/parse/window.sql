-- An `OVER` clause turns a call into a window function.
SELECT row_number() OVER () FROM t;
SELECT rank() OVER (PARTITION BY a, b ORDER BY c DESC) FROM t;
SELECT sum(x) OVER (ORDER BY a ROWS BETWEEN 1 PRECEDING AND 1 FOLLOWING) FROM t;
SELECT sum(x) OVER (RANGE UNBOUNDED PRECEDING) FROM t;
SELECT sum(x) OVER (GROUPS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING EXCLUDE TIES) FROM t;
SELECT sum(x) OVER (ROWS CURRENT ROW EXCLUDE NO OTHERS) FROM t;

-- A named window, defined once in the `WINDOW` clause and refined per use.
SELECT rank() OVER w, sum(x) OVER (w ORDER BY b) FROM t WINDOW w AS (PARTITION BY a);
SELECT count(*) OVER w FROM t WINDOW w AS (), v AS (w ORDER BY a);

-- `FILTER` and `WITHIN GROUP`, alone and together with a window.
SELECT count(*) FILTER (WHERE x > 0) FROM t;
SELECT percentile_cont(0.5) WITHIN GROUP (ORDER BY x, y DESC) FROM t;
SELECT sum(x) FILTER (WHERE x > 0) OVER (PARTITION BY a) FROM t;

-- Grouping elements beyond a plain expression.
SELECT a, sum(b) FROM t GROUP BY ROLLUP (a, b);
SELECT a, sum(b) FROM t GROUP BY CUBE (a, b), c;
SELECT a, sum(b) FROM t GROUP BY GROUPING SETS ((a, b), (a), ());
SELECT sum(b) FROM t GROUP BY ();
SELECT grouping(a) FROM t GROUP BY ROLLUP (a);
