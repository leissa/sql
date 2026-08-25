-- Precedence: * and / bind tighter than + and -, which bind tighter than the comparisons.
SELECT a + b * c, a * b + c, (a + b) * c FROM t;
SELECT a = b + c FROM t;

-- Left associativity - `a - b - c` is `(a - b) - c`, not `a - (b - c)`.
SELECT a - b - c, a / b / c, a + b + c FROM t;

-- Unary operators.
SELECT -a, +b, NOT c FROM t;
SELECT -a * b FROM t;

-- Boolean connectives: AND binds tighter than OR.
SELECT a FROM t WHERE a AND b OR c;
SELECT a FROM t WHERE a OR b AND c;
SELECT a FROM t WHERE (a OR b) AND c;

-- Comparisons.
SELECT a FROM t WHERE a = 1 AND b <> 2 AND c != 3 AND d < 4 AND e <= 5 AND f > 6 AND g >= 7;

-- IS / IS NOT.
SELECT a FROM t WHERE a IS NULL;
SELECT a FROM t WHERE a IS NOT NULL;
SELECT a FROM t WHERE a IS TRUE AND b IS NOT FALSE AND c IS UNKNOWN;

-- LIKE / IN / BETWEEN, plain and negated.
SELECT a FROM t WHERE a LIKE '%x%';
SELECT a FROM t WHERE a NOT LIKE '%x%';
SELECT a FROM t WHERE a IN (1, 2, 3);
SELECT a FROM t WHERE a NOT IN (1, 2, 3);
SELECT a FROM t WHERE a BETWEEN 1 AND 10;
SELECT a FROM t WHERE a NOT BETWEEN 1 AND 10;
SELECT a FROM t WHERE a BETWEEN 1 AND 10 AND b = 2;

-- Qualified references, including one qualified by the reserved word AT.
SELECT t.a, s.t.a, at.movie_id FROM t;

-- Values.
SELECT 0, 42, TRUE, FALSE, UNKNOWN, NULL FROM t;
