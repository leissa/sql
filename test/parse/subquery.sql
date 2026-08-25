SELECT * FROM t WHERE a IN (SELECT x FROM u);
SELECT * FROM t WHERE a NOT IN (SELECT x FROM u);
SELECT * FROM t WHERE EXISTS (SELECT 1 FROM u WHERE u.id = t.id);
SELECT * FROM t WHERE NOT EXISTS (SELECT 1 FROM u);
SELECT * FROM t WHERE a = (SELECT MAX(x) FROM u);
SELECT (SELECT MAX(x) FROM u) AS m FROM t;

-- A derived table in the FROM list keeps its parentheses; the correlation name may rename columns.
SELECT * FROM (SELECT a FROM t) AS s;
SELECT * FROM (SELECT a FROM t) AS s (col);
SELECT * FROM (SELECT a FROM t) AS s, u;
SELECT * FROM (SELECT a FROM t UNION SELECT b FROM u) AS s;

-- Correlated and nested.
SELECT * FROM t WHERE EXISTS (SELECT 1 FROM u WHERE EXISTS (SELECT 1 FROM v WHERE v.id = u.id));
