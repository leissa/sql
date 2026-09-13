-- Queries from the hyrise/sql-parser test suite (test/queries/queries-good.sql) that this parser
-- accepts too. Its `#` section comments became `--` ones and every query got the trailing `;`
-- this parser insists on; the queries themselves are verbatim.
--
-- Left out are the ones written in hyrise's own dialect, which this parser does not claim to
-- speak - `CREATE TABLE ... FROM TBL FILE`, `COPY ... WITH (...)`, `PREPARE`/`EXECUTE`/
-- `DEALLOCATE`, `SHOW`/`DESCRIBE`, `WITH HINT (...)`, `SELECT TOP n`, `SELECT ... FOR UPDATE`,
-- `ALTER TABLE ... DROP COLUMN IF EXISTS`, `TRUNCATE` without `TABLE`, and unquantified intervals
-- such as `- 1 MONTH`. A section whose every query went that way lost its label with them.
--
-- The three queries over the delimited identifier `"table"` are out for a different reason:
-- the printer drops the quotes, so the dump no longer parses back. Bring them in once it
-- keeps them.

-- SELECT statement
SELECT * FROM orders;
SELECT a FROM foo WHERE a > 12 OR b > 3 AND NOT c LIMIT 10;
SELECT a FROM some_schema.foo WHERE a > 12 OR b > 3 AND NOT c LIMIT 10;
(SELECT a FROM foo WHERE a > 12 OR b > 3 AND c NOT LIKE 's%' LIMIT 10);
SELECT * FROM t1 UNION SELECT * FROM t2 ORDER BY col1;
SELECT * FROM (SELECT * FROM t1);
SELECT * FROM t1 UNION (SELECT * FROM t2 UNION SELECT * FROM t3) ORDER BY col1;
SELECT a, MAX(b), MAX(c, d), CUSTOM(q, UP(r)) AS f FROM t1;
SELECT * FROM t WHERE a BETWEEN 1 and c;
SELECT * FROM t WHERE a = ? AND b = ?;
SELECT City.name, Product.category, SUM(price) FROM fact INNER JOIN City ON fact.city_id = City.id INNER JOIN Product ON fact.product_id = Product.id GROUP BY City.name, Product.category;
SELECT SUBSTR(a, 3, 5) FROM t;
SELECT * FROM t WHERE a = DATE '1996-12-31';
-- JOIN
SELECT * FROM t1 JOIN t2 ON c1 = c2;
SELECT a, SUM(b) FROM t2 GROUP BY a HAVING SUM(b) > 100;
-- CREATE statement
CREATE TABLE students (name TEXT, student_number INTEGER, city TEXT, grade DOUBLE, credits BIGINT);
CREATE TABLE students (name TEXT, student_number INTEGER NOT NULL, city TEXT, grade DOUBLE PRIMARY KEY UNIQUE);
CREATE TABLE teachers (name VARCHAR(30), student_number LONG, city CHAR(10), grade FLOAT);
CREATE TABLE teachers (name VARCHAR(30), student_number LONG, PRIMARY KEY (name, student_number), city CHAR(10), grade FLOAT);
CREATE TABLE teachers (name CHARACTER VARYING(30));
CREATE TABLE students_2 AS SELECT * FROM students;
CREATE TABLE students_3 AS SELECT city, grade FROM students WHERE grade > 3.0;
CREATE TABLE students (date_of_birth DATE, matriculation_date DATETIME, graduation_date TIMESTAMP, graduated BOOLEAN);
CREATE TABLE foo (a int, b int REFERENCES bar REFERENCES baz);
CREATE TABLE foo (a int, b int REFERENCES bar (x) REFERENCES baz (y));
CREATE TABLE foo (a int, b int, FOREIGN KEY (b) REFERENCES bar, FOREIGN KEY (b) REFERENCES baz);
CREATE TABLE foo (a int, b int, FOREIGN KEY (b) REFERENCES bar (x), FOREIGN KEY (b) REFERENCES baz (y));
-- INSERT
INSERT INTO test_table VALUES (1, 2, 'test');
INSERT INTO test_table (id, value, name) VALUES (1, 2, 'test');
INSERT INTO test_table SELECT * FROM students;
INSERT INTO some_schema.test_table SELECT * FROM another_schema.students;
-- DELETE
DELETE FROM students WHERE grade > 3.0;
DELETE FROM students;
-- UPDATE
UPDATE students SET grade = 1.3 WHERE name = 'Max Mustermann';
UPDATE students SET grade = 1.3, name='Felix Fürstenberg' WHERE name = 'Max Mustermann';
UPDATE students SET grade = 1.0;
UPDATE some_schema.students SET grade = 1.0;
-- DROP
DROP TABLE students;
DROP TABLE IF EXISTS students;
DROP VIEW IF EXISTS students;
DROP INDEX myindex;
DROP INDEX IF EXISTS myindex;
-- HINTS
SELECT * FROM t WHERE a = DATE '2000-01-01' + INTERVAL '30 DAYS';
SELECT * FROM t WHERE a = DATE '2000-01-01' + INTERVAL '10' DAY;
SELECT (CAST('2002-5-01' as DATE) + INTERVAL '60 days');
SELECT CAST(student.student_number as BIGINT) FROM student;
SELECT student.name AS character FROM student;
-- WINDOW EXPRESSIONS
SELECT test1, sum(sum(test2)) OVER (PARTITION BY test3 ORDER BY test4 ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW) an_alias FROM test;
SELECT sum(test2)/sum(sum(test2)) OVER (PARTITION BY test1) FROM test GROUP BY test3;
SELECT test1, sum(sum(test2)) OVER (PARTITION BY test3, test4 ORDER BY test5, test6 ROWS BETWEEN 1 PRECEDING AND 2 FOLLOWING) FROM test;
SELECT test1, rank() OVER (ORDER BY test2 DESC, test3 ASC) rnk FROM test;
SELECT rank() OVER () FROM test;
SELECT rank() OVER (PARTITION BY test1) FROM test;
SELECT rank() OVER (PARTITION BY test1 ORDER BY test2) FROM test;
