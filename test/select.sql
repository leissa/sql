SELECT MAX(mc.note) AS production_note, 
       MIN(t.title) AS movie_title, 
       MIN(t.production_year) AS movie_year 
FROM company_type
WHERE ct.kind = it.info;