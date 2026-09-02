-- Numeric literals: integers, fractions, and exponents.
SELECT 0, 42, 1.5, .5, 3., 1e10, 2.5E-3, 1E+2 FROM t;

-- Dynamic parameter markers in all three spellings.
SELECT ?, $1, :name FROM t WHERE a = ? AND b = $2 AND c = :id;

-- Typed literals; an `INTERVAL` may carry its qualifier.
SELECT DATE '2024-01-01', TIME '12:00:00', TIMESTAMP '2024-01-01 12:00:00' FROM t;
SELECT INTERVAL '1', INTERVAL '1-2' YEAR TO MONTH, INTERVAL '3' DAY(2) FROM t;

-- Concatenation and modulo.
SELECT a || b || 'x', a % b, a || b + c FROM t;

-- The functions the standard spells with keyword-separated arguments.
SELECT EXTRACT(YEAR FROM d), EXTRACT(epoch FROM d) FROM t;
SELECT SUBSTRING(s FROM 2), SUBSTRING(s FROM 2 FOR 3), SUBSTRING(s, 2, 3) FROM t;
SELECT TRIM(s), TRIM(' ' FROM s), TRIM(BOTH FROM s), TRIM(LEADING '0' FROM s) FROM t;
SELECT POSITION('a' IN s), OVERLAY(s PLACING 'x' FROM 2), OVERLAY(s PLACING 'x' FROM 2 FOR 1) FROM t;

-- Comparison predicates that need more than a binary operator.
SELECT a FROM t WHERE a IS DISTINCT FROM b AND c IS NOT DISTINCT FROM d;
SELECT a FROM t WHERE a LIKE '%x%' ESCAPE '!' AND b NOT LIKE 'y' ESCAPE '!';
SELECT a FROM t WHERE a SIMILAR TO '%x%' AND b NOT SIMILAR TO 'y';
SELECT a FROM t WHERE a = ANY (SELECT x FROM u) AND b > ALL (SELECT y FROM v);
SELECT a FROM t WHERE a <> SOME (SELECT z FROM w);
SELECT a COLLATE "de_DE" FROM t ORDER BY b COLLATE c;

-- `LATERAL` and `WITH ORDINALITY` in the `FROM` list.
SELECT * FROM t, LATERAL (SELECT * FROM u WHERE u.a = t.a) AS x;
SELECT * FROM unnest(a) WITH ORDINALITY AS u (v, n);
