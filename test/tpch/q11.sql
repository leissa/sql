-- The alias `VALUE` became `TOTAL_VALUE`: it is a reserved word, and this parser takes
-- those only where a name is expected, not as a reference in an `ORDER BY`.
-- http://www.sqlserver-dba.com/2011/09/this-is-a-followup-on-my-earlier-post-of-sql-server-test-data-generation-testing-tools-i-had-some-requests-for-my-set-up-pr.html
-- TPC_H Query 11 - Important Stock Identification
SELECT PS_PARTKEY, SUM(PS_SUPPLYCOST*PS_AVAILQTY) AS TOTAL_VALUE
FROM PARTSUPP, SUPPLIER, NATION
WHERE PS_SUPPKEY = S_SUPPKEY AND S_NATIONKEY = N_NATIONKEY AND N_NAME = 'GERMANY'
GROUP BY PS_PARTKEY
HAVING SUM(PS_SUPPLYCOST*PS_AVAILQTY) > (SELECT SUM(PS_SUPPLYCOST*PS_AVAILQTY) * 0.0001000000
 FROM PARTSUPP, SUPPLIER, NATION
 WHERE PS_SUPPKEY = S_SUPPKEY AND S_NATIONKEY = N_NATIONKEY AND N_NAME = 'GERMANY')
ORDER BY TOTAL_VALUE DESC;
