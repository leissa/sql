-- `PREPARE` either nests the statement or leaves it as text for whoever prepares it to parse.
PREPARE p AS SELECT * FROM t WHERE a = ?;
PREPARE p (INTEGER, VARCHAR(10)) AS SELECT * FROM t WHERE a = $1 AND b = $2;
PREPARE ins AS INSERT INTO t VALUES (1, 2);
PREPARE p FROM 'SELECT * FROM t WHERE a = ?';
PREPARE p FROM 'INSERT INTO t VALUES (?, 0); INSERT INTO t VALUES (0, ?);';

-- A bare name and an empty argument list are not the same thing.
EXECUTE p;
EXECUTE p ();
EXECUTE p (1, 'x', DATE '2000-01-01', -2.5, NULL);

DEALLOCATE p;
DEALLOCATE PREPARE p;
DEALLOCATE ALL;
