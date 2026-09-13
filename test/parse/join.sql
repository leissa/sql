SELECT * FROM a JOIN b ON a.id = b.id;
SELECT * FROM a INNER JOIN b ON a.id = b.id;
SELECT * FROM a LEFT JOIN b ON a.id = b.id;
SELECT * FROM a LEFT OUTER JOIN b ON a.id = b.id;
SELECT * FROM a RIGHT JOIN b ON a.id = b.id;
SELECT * FROM a FULL OUTER JOIN b ON a.id = b.id;
SELECT * FROM a CROSS JOIN b;
SELECT * FROM a NATURAL JOIN b;
SELECT * FROM a NATURAL LEFT JOIN b;
SELECT * FROM a JOIN b USING (id);
SELECT * FROM a JOIN b USING (id, kind);

-- A chain of joins: the ON condition must not swallow the join that follows it.
SELECT * FROM a JOIN b ON a.id = b.id LEFT JOIN c ON b.id = c.id;
SELECT * FROM a JOIN b ON a.id = b.id AND a.k = b.k JOIN c USING (id);

-- Joins alongside a comma-separated FROM list.
SELECT * FROM a JOIN b ON a.id = b.id, c;

-- A correlation name binds to one table reference, not to the whole join chain.
SELECT * FROM a AS x JOIN b AS y ON x.id = y.id;
SELECT * FROM a x JOIN (SELECT * FROM b) y ON x.id = y.id;
SELECT * FROM a AS x (p, q) JOIN b ON a.id = b.id;

-- Parentheses around a table reference only group: a right-nested join keeps them, a left-nested
-- one does not need them, and one around a lone table reference falls away.
SELECT * FROM a JOIN (b JOIN c ON b.id = c.id) ON a.id = b.id;
SELECT * FROM ((a JOIN b ON a.id = b.id) JOIN c ON b.id = c.id);
SELECT * FROM (t);
