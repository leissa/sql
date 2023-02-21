SELECT DISTINCT a.*, 4 * 5, a.b.c.* FROM test.x.foo.* GROUP BY 23 * 4 + +5 * 42;
SELECT DISTINCT a.*, 4 * 5, a.b.c.* AS x FROM test.x.foo.* GROUP BY bar;
SELECT DISTINCT a.*, 4 * 5, a.b.c.* AS (x, y) FROM test.x.foo.* GROUP BY bar;
