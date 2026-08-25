-- Searched CASE - no operand between CASE and the first WHEN.
SELECT CASE WHEN a > 1 THEN 'big' END FROM t;
SELECT CASE WHEN a > 1 THEN 'big' ELSE 'small' END FROM t;
SELECT CASE WHEN a > 2 THEN 'big' WHEN a > 1 THEN 'medium' ELSE 'small' END FROM t;

-- Simple CASE - the operand is compared against each WHEN.
SELECT CASE a WHEN 1 THEN 'one' WHEN 2 THEN 'two' ELSE 'many' END FROM t;

-- Nested, and used as an ordinary operand.
SELECT CASE WHEN a THEN CASE WHEN b THEN 1 ELSE 2 END ELSE 3 END FROM t;
SELECT 1 + CASE WHEN a THEN 2 ELSE 3 END FROM t;
SELECT a FROM t WHERE CASE WHEN a THEN 1 ELSE 2 END = 1;

SELECT CAST(a AS INTEGER) FROM t;
SELECT CAST(a AS CHARACTER VARYING(10)) FROM t;
SELECT CAST(a + 1 AS text) FROM t;
SELECT CAST(CAST(a AS INTEGER) AS text) FROM t;
