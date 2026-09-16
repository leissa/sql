-- A single-line comment.
SELECT 1 FROM t; -- trailing after a statement
/* A multi-line
   comment. */
SELECT /* inline */ 2 FROM t;
/* nested-looking /* but SQL stops at the first close */
SELECT 3 FROM t;
--
/**/
/***/
SELECT 4 FROM t;

-- String literals. A doubled quote escapes one, so the dump has to re-double it.
SELECT 'plain', '', 'it''s', 'a-b_c', '%wild%', 'tab	here' FROM t;

-- A backslash escapes the next character, so the dump has to re-double that too.
SELECT 'back\\slash', '\\', 'q\'s', 'a\tb' FROM t;
SELECT a AS "back\\slash id", b AS "quote "" and \\ id" FROM t;

-- Keywords are case insensitive and identifiers fold to lower case.
select A from T where B = 1;
SeLeCt a FrOm t;

-- A double-quoted delimited identifier keeps its case, so the dump has to re-quote it.
SELECT "MixedCase", "with space" FROM "T";

-- Reserved words are accepted as identifiers where one is expected.
SELECT a AS character, b AS at, c AS "select" FROM t;

-- Integer literals.
SELECT 0, 1, 10, 1000000 FROM t;

-- a line comment ending at EoF, with no newline of its own
