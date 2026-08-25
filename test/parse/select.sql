SELECT * FROM t;
SELECT ALL a FROM t;
SELECT DISTINCT a FROM t;
SELECT t.* FROM t;
SELECT a, b AS alias, c AS (x, y) FROM t;
SELECT a FROM t, u, v;

-- A correlation name, with and without the optional AS.
SELECT a FROM long_table_name AS t;
SELECT a FROM long_table_name t;

SELECT a FROM t WHERE a = 1;
SELECT a FROM t GROUP BY a, b;
SELECT a FROM t GROUP BY a HAVING COUNT(*) > 1;
SELECT a FROM t WHERE a = 1 GROUP BY b HAVING SUM(c) > 2;

-- Aggregates and plain scalar calls; DISTINCT and `*` as arguments.
SELECT COUNT(*), COUNT(DISTINCT a), COUNT(ALL a), MIN(a), MAX(b), SUM(c), AVG(d) FROM t;
SELECT lower(a), some_udf(a, b, 1) FROM t;

-- ORDER BY / OFFSET / FETCH wrap the whole query.
SELECT a FROM t ORDER BY a;
SELECT a FROM t ORDER BY a ASC, b DESC, 3;
SELECT a FROM t ORDER BY a OFFSET 10 ROWS;
SELECT a FROM t ORDER BY a OFFSET 10 ROWS FETCH NEXT 5 ROWS ONLY;
SELECT a FROM t FETCH FIRST 1 ROWS ONLY;
