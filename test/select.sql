SELECT DISTINCT a.*, 4 * 5, a.b.c.* FROM test GROUP BY 23 * 4 + +5 * 42;
SELECT DISTINCT a.*, 4 * 5, a.b.c.* AS (x, y) FROM test GROUP BY bar;
SELECT DISTINCT * FROM test GROUP BY bar;
SELECT * FROM test INNER JOIN x  GROUP BY bar HAVING x < 5;
SELECT * FROM test NATURAL JOIN x ON x < 42, x NATURAL LEFT JOIN a.b.c ON NOT x AND y GROUP BY bar HAVING x < 5;
SELECT * FROM test NATURAL JOIN x USING (a, b, c) GROUP BY bar HAVING x < 5;
SELECT * FROM test NATURAL FULL JOIN x  GROUP BY bar HAVING x < 5;
SELECT * FROM test CROSS JOIN x  GROUP BY bar HAVING x < 5;
SELECT * FROM test CROSS x  GROUP BY bar HAVING x < 5;
SELECT *
FROM Customers
WHERE last_name = doe AND country = usa;

SELECT *
FROM students
WHERE x > (SELECT asdf FROM students);
CREATE TABLE foobar (
    x INT,
    y SMALLINT);
