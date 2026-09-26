# 📜 Grammar

*Part of [sql](README.md), a fast SQL parser handwritten on top of [FE](https://leissa.github.io/fe/).*

The grammar below is the one this parser actually implements - not the standard's, which is both
larger and stricter.
It is complete: every construct the parser accepts has a production here.
It is deliberately *ambiguous*: `expr ::= expr '+' expr | expr '*' expr` says which operators exist,
and the [precedence table](#precedence) below says how to read them.
Spelling the levels out as a chain of nonterminals would say the same thing far less legibly, and
the parser does not work that way either - it climbs precedences.

Notation: `[ x ]` is optional, `{ x }` repeats zero or more times, `|` separates alternatives, and
`( x )` groups.
`UPPERCASE` is a keyword, `'x'` is punctuation, and `(* ... *)` is a remark.

## Lexical

```ebnf
ident    ::= ( letter | '_' ) { letter | digit | '_' }  (* folded to lower case *)
           | '"' { char } '"'               (* delimited: keeps its case, "" escapes one " *)
name     ::= ident | keyword                (* a reserved word is a fine name - see above *)
qname    ::= name { '.' name }              (* t, s.t, cat.sch.tab *)
col-list ::= '(' name { ',' name } ')'

integer  ::= digit { digit }
real     ::= [ digit { digit } ] '.' digit { digit } [ exponent ] | digit { digit } exponent
string   ::= "'" { char } "'"               (* '' escapes one ' *)
param    ::= '?' | '$' integer | ':' ident
```

Keywords are case insensitive, being folded the same way.
Comments are `-- to end of line` and `/* ... */`.

## Statements

```ebnf
prog ::= { [ stmt ] ';' }                          (* a stray ';' is an empty statement *)

stmt ::= create | alter | drop | truncate | insert | update | delete | transact
       | prepare | execute | deallocate | show | copy | query
```

A statement is *not* a value expression: `1 + 2;` parses as an expression but is no statement.

```ebnf
(* OR REPLACE and TEMPORARY are read before it is clear what is being created, so every
   branch accepts them - only the ones they belong to will survive a later check. *)
create ::= CREATE [ OR REPLACE ] [ GLOBAL | LOCAL ] [ TEMPORARY ]
           ( TABLE [ IF NOT EXISTS ] qname ( '(' table-elem { ',' table-elem } ')' | AS query )
           | VIEW qname [ col-list ] AS query [ WITH [ CASCADED | LOCAL ] CHECK OPTION ]
           | [ UNIQUE ] INDEX [ IF NOT EXISTS ] name ON qname '(' order { ',' order } ')'
           | SCHEMA [ IF NOT EXISTS ] qname )

table-elem ::= col-def | constraint
col-def    ::= name type { constraint }

constraint ::= [ CONSTRAINT name ]
               ( PRIMARY [ KEY ] [ col-list ]      (* the column list only at table level *)
               | UNIQUE [ col-list ]
               | FOREIGN [ KEY ] col-list REFERENCES qname [ col-list ] { ref-action }
               | REFERENCES qname [ col-list ] { ref-action }
               | CHECK '(' expr ')'
               | DEFAULT expr )
ref-action ::= ON ( DELETE | UPDATE )
               ( NO ACTION | RESTRICT | CASCADE | SET NULL | SET DEFAULT )

alter ::= ALTER TABLE [ IF EXISTS ] qname
          ( ADD constraint
          | ADD [ COLUMN ] col-def
          | DROP CONSTRAINT [ IF EXISTS ] name [ behavior ]
          | DROP [ COLUMN ] [ IF EXISTS ] name [ behavior ]
          | ALTER [ COLUMN ] name ( SET DEFAULT expr | SET NOT NULL | SET DATA TYPE type
                                  | DROP DEFAULT | DROP NOT NULL )
          | RENAME TO name
          | RENAME [ COLUMN ] name TO name )

drop     ::= DROP ( TABLE | VIEW | INDEX | SCHEMA ) [ IF EXISTS ] qname [ behavior ]
truncate ::= TRUNCATE [ TABLE ] qname
behavior ::= CASCADE | RESTRICT

insert ::= INSERT INTO qname [ col-list ] ( query | DEFAULT VALUES )
update ::= UPDATE qname [ AS name ] SET assign { ',' assign } [ WHERE expr ]
assign ::= qname '=' expr
delete ::= DELETE FROM qname [ AS name ] [ WHERE expr ]

transact ::= ( BEGIN | START ) [ noise ]
           | COMMIT [ noise ]
           | ROLLBACK [ noise ] [ TO [ SAVEPOINT ] name ]
           | SAVEPOINT name
           | RELEASE [ SAVEPOINT ] name
noise    ::= TRANSACTION | WORK
```

A prepared statement is named by a bare `name`, never qualified.
`PREPARE ... FROM` leaves the statement as text and is the one form that does not nest one statement
inside another.

```ebnf
prepare    ::= PREPARE name [ '(' type { ',' type } ')' ] ( AS stmt | FROM string )
execute    ::= EXECUTE name [ '(' [ expr { ',' expr } ] ')' ]
deallocate ::= DEALLOCATE [ PREPARE ] ( name | ALL )

show ::= SHOW TABLES | SHOW COLUMNS qname | DESCRIBE qname
```

`COPY` reads a table with `FROM` and writes one with `TO`; only the latter takes a query in place of
a table, and only the former takes a `WHERE`, but as everywhere the grammar accepts both and leaves
the pairing to a later check. The `WITH` before the option list is a noise word.

```ebnf
copy   ::= COPY ( qname [ col-list ] | '(' query ')' )
           ( FROM ( string | STDIN ) | TO ( string | STDOUT ) )
           [ [ WITH ] '(' option { ',' option } ')' ] [ WHERE expr ]
option ::= name [ name | expr ]        (* FORMAT csv, DELIMITER '|', HEADER *)
```

## Query expressions

```ebnf
query ::= [ WITH [ RECURSIVE ] cte { ',' cte } ]
          body
          [ ORDER BY order { ',' order } ]
          { offset | fetch | limit }                (* in any order, each at most once *)
          { lock }

cte    ::= name [ col-list ] AS '(' query ')'
offset ::= OFFSET expr [ ROW | ROWS ]
fetch  ::= FETCH [ FIRST | NEXT ] expr [ ROW | ROWS ] ONLY
limit  ::= LIMIT ( expr | ALL )            (* ALL spells out no limit at all *)

body ::= body ( UNION | EXCEPT | INTERSECT ) [ ALL | DISTINCT ] body
       | select | values | TABLE qname | '(' query { ',' query } ')'

select ::= SELECT [ ALL | DISTINCT ] ( '*' | select-elem { ',' select-elem } )
           [ FROM table-ref { ',' table-ref } ]
           [ WHERE expr ]
           [ GROUP BY group-elem { ',' group-elem } ]
           [ HAVING expr ]
           [ WINDOW name AS window { ',' name AS window } ]

select-elem ::= expr [ AS ( name | col-list ) | ident ]   (* without AS only a plain ident aliases *)
group-elem  ::= ( ROLLUP | CUBE | GROUPING SETS ) '(' group-elem { ',' group-elem } ')'
              | '(' ')'                                   (* the empty grouping set *)
              | expr

values ::= VALUES row { ',' row }
row    ::= '(' expr { ',' expr } ')' | expr               (* a one-column row needs no parentheses *)

order ::= expr [ ASC | DESC ] [ NULLS ( FIRST | LAST ) ]

lock ::= FOR ( UPDATE | NO KEY UPDATE | SHARE | KEY SHARE )
         [ OF qname { ',' qname } ] [ NOWAIT | SKIP LOCKED ]
```

## Table references

A correlation name binds to one *factor*, which is what makes `a AS x JOIN b AS y ON ...` come out
right; a parenthesized table reference takes none of its own.

```ebnf
table-ref ::= table-ref join-op table-ref [ ON expr | USING col-list ]
            | table-factor
join-op   ::= [ NATURAL ] [ INNER | ( LEFT | RIGHT | FULL ) [ OUTER ] ] JOIN
            | CROSS JOIN

table-factor  ::= [ LATERAL ] table-primary [ WITH ORDINALITY ] [ correlation ]
correlation   ::= ( AS name | ident ) [ col-list ]
table-primary ::= '(' table-ref ')'                (* a '(' that opens no query merely groups *)
                | expr                             (* in practice a qname, a call, or '(' query ')' *)
```

## Value expressions

```ebnf
expr ::= expr ( '+' | '-' | '*' | '/' | '%' | '^' ) expr
       | expr '||' expr
       | expr ( '=' | '<>' | '!=' | '<' | '<=' | '>' | '>=' ) expr
       | expr ( '=' | '<>' | '!=' | '<' | '<=' | '>' | '>=' ) ( ALL | ANY | SOME ) '(' query ')'
       | expr AND expr
       | expr OR expr
       | expr IS [ NOT ] expr
       | expr IS [ NOT ] DISTINCT FROM expr
       | expr [ NOT ] IN expr
       | expr [ NOT ] BETWEEN expr AND expr
       | expr [ NOT ] ( LIKE | ILIKE | SIMILAR TO ) expr [ ESCAPE expr ]
       | expr COLLATE qname
       | ( '+' | '-' | NOT | EXISTS ) expr
       | primary

primary ::= literal
          | ref { '[' expr ']' }                   (* array subscripts *)
          | ARRAY '[' [ expr { ',' expr } ] ']'    (* an array value; ARRAY(x) is the call *)
          | CASE [ expr ] { WHEN expr THEN expr } [ ELSE expr ] END
          | CAST '(' expr AS type ')'
          | special-func
          | '(' query { ',' query } ')'            (* a subquery, or a row: '(' a ',' b ')' *)

ref  ::= qname [ '.' '*' ] | call
call ::= qname '(' [ DISTINCT | ALL ] [ expr { ',' expr } ] ')'
         [ WITHIN GROUP '(' ORDER BY order { ',' order } ')' ]
         [ FILTER '(' WHERE expr ')' ]
         [ OVER window ]

special-func ::= EXTRACT '(' name FROM expr ')'
               | SUBSTRING '(' expr FROM expr [ FOR expr ] ')'
               | TRIM '(' [ LEADING | TRAILING | BOTH ] [ expr FROM | FROM ] expr ')'
               | POSITION '(' expr IN expr ')'
               | OVERLAY '(' expr PLACING expr FROM expr [ FOR expr ] ')'

literal ::= integer | real | string | param
          | TRUE | FALSE | UNKNOWN | NULL | DEFAULT
          | '*'                                    (* what COUNT(*) counts *)
          | ( DATE | TIME | TIMESTAMP | INTERVAL ) string [ interval-qual ]

window ::= name                                    (* one named in the WINDOW clause *)
         | '(' [ ident ] [ PARTITION BY expr { ',' expr } ]
                         [ ORDER BY order { ',' order } ] [ frame ] ')'
frame  ::= ( ROWS | RANGE | GROUPS ) ( bound | BETWEEN bound AND bound )
           [ EXCLUDE ( CURRENT ROW | GROUP | TIES | NO OTHERS ) ]
bound  ::= UNBOUNDED ( PRECEDING | FOLLOWING ) | CURRENT ROW | expr ( PRECEDING | FOLLOWING )
```

## Types

```ebnf
type ::= simple-type | name-type
name-type   ::= ident [ '(' expr { ',' expr } ')' ] [ [ NOT ] NULL ]
simple-type ::= ( INTEGER | INT | SMALLINT | BIGINT | BOOLEAN | DATE | REAL | FLOAT
                | DOUBLE [ PRECISION ] | TIME | TIMESTAMP | INTERVAL | NUMERIC | DECIMAL | DEC
                | CHAR | CHARACTER | VARCHAR | BINARY | VARBINARY | BLOB | CLOB )
                [ LARGE OBJECT ] [ VARYING ] [ '(' expr { ',' expr } ')' ]
                [ interval-qual ] [ ( WITH | WITHOUT ) TIME ZONE ] [ [ NOT ] NULL ]

interval-qual ::= field [ '(' expr ')' ] [ TO field [ '(' expr ')' ] ]
field         ::= YEAR | MONTH | DAY | HOUR | MINUTE | SECOND
```

## Precedence

Loosest first; every binary operator is left-associative.
A `NOT` may precede any binary operator and ranks with it, so `a NOT LIKE b` and `a NOT IN c` need
no rules of their own.

| Precedence | Operators |
| --- | --- |
| `OR` | `a OR b` |
| `AND` | `a AND b` |
| `BETWEEN` | `a BETWEEN lo AND hi` |
| `NOT` | prefix `NOT a` |
| comparison | `= <> != < <= > >=`, `IS`, `IS DISTINCT FROM`, `IN`, `LIKE`, `ILIKE`, `SIMILAR TO` |
| concatenation | `a \|\| b` |
| additive | `a + b`, `a - b` |
| multiplicative | `a * b`, `a / b`, `a % b` |
| power | `a ^ b` |
| unary | prefix `+a`, `-a`, `EXISTS a`, postfix `a COLLATE c` |
| subscript | `a[i]` |

The bounds of a `BETWEEN` parse above `NOT`, so its own `AND` ends the lower bound instead of being
swallowed as a conjunction.
Subscripts bind tighter than the prefix operators: `-a[1]` negates the element, not the array.
The prefix operators, in turn, bind tighter than `^`, so `-a ^ 2` squares the negation.

Two more chains sit outside the expression grammar.
`INTERSECT` binds tighter than `UNION` and `EXCEPT`, and both chains are left-associative.
`JOIN` chains are left-associative too, so `a JOIN b JOIN c` reads as `(a JOIN b) JOIN c` and only a
right-nested join needs parentheses.
