-- `COPY` reads a table with `FROM` and writes one with `TO`; the `WITH` before the options is noise.
COPY students FROM 'students.tbl';
COPY students FROM 'students.tbl' WITH (FORMAT TBL, ENCODING 'Dictionary');
COPY s.students (name, grade) FROM 'students.csv' (FORMAT CSV, DELIMITER '|', NULL '', HEADER);
COPY students FROM STDIN;
COPY good_students FROM 'students.tbl' WHERE grade > (SELECT AVG(grade) FROM alumni);
COPY students TO 'students.tbl';
COPY students TO 'students.bin' WITH (FORMAT BINARY);
COPY students TO STDOUT (FORMAT CSV);
COPY (SELECT name, COUNT(*) FROM students GROUP BY name) TO 'names.csv' (FORMAT CSV);
