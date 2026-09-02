-- A statement list is `;`-separated; a stray `;` is an empty statement and simply skipped.
SELECT a FROM t;;
;
SELECT b FROM u;

-- A `SELECT` needs no `FROM` - it computes its value out of thin air.
SELECT 1;
SELECT 1 + 2 AS three;

-- A `VALUES` table and an explicit `TABLE` are query expressions in their own right.
VALUES (1, 'a'), (2, 'b');
VALUES 1, 2, 3;
TABLE t;
TABLE s.t;

-- An alias may drop the `AS`, in the select list just as much as in the `FROM` list.
SELECT a b, c AS d FROM t u, v AS w;

-- Names may be qualified wherever a table is named.
SELECT * FROM cat.sch.tab;
INSERT INTO s.t (a) VALUES (1);
UPDATE s.t SET s.t.a = 1;
DELETE FROM s.t;
