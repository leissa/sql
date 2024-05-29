SELECT MIN(mc.note) AS production_note, 
       MIN(t.title) AS movie_title, 
       MIN(t.production_year) AS movie_year 
FROM company_type AS ct
WHERE kind = 'productioncompanies';