SELECT ARRAY (1,2,3,4); 
SELECT ARRAY((1,2),(3,4)); 
SELECT x, y, val FROM matrix WHERE val >2; 
SELECT x, y, val FROM stripes WHERE val >2; 
SELECT [x], [y], val FROM matrix WHERE val >2; 
SELECT [T.k], [y], val FROM matrix JOIN T ON matrix.x = T.i;
