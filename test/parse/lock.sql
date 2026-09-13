-- Row locking. Of the words involved only FROM, FOR, NO, OF, and UPDATE are reserved - SHARE, KEY,
-- NOWAIT, SKIP, and LOCKED are plain identifiers everywhere else.
SELECT * FROM t WHERE id = 1 FOR UPDATE;
SELECT * FROM t FOR NO KEY UPDATE;
SELECT * FROM t FOR SHARE;
SELECT * FROM t FOR KEY SHARE;

-- Which tables to lock, and what to do about a row someone else holds.
SELECT * FROM t, u FOR UPDATE OF t;
SELECT * FROM t, u FOR SHARE OF t, some_schema.u;
SELECT * FROM t FOR UPDATE NOWAIT;
SELECT * FROM t FOR UPDATE SKIP LOCKED;
SELECT * FROM t, u FOR UPDATE OF t FOR SHARE OF u;

-- It comes last, after ORDER BY and LIMIT.
SELECT a FROM t ORDER BY a LIMIT 5 FOR UPDATE;
WITH c AS (SELECT 1) SELECT * FROM c FOR UPDATE;

-- None of those words is reserved.
SELECT key, share, skip, locked, nowait FROM t;
