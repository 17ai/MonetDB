-- The contents of this file are subject to the MonetDB Public License
-- Version 1.1 (the "License"); you may not use this file except in
-- compliance with the License. You may obtain a copy of the License at
-- http://www.monetdb.org/Legal/MonetDBLicense
--
-- Software distributed under the License is distributed on an "AS IS"
-- basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
-- License for the specific language governing rights and limitations
-- under the License.
--
-- The Original Code is the MonetDB Database System.
--
-- The Initial Developer of the Original Code is CWI.
-- Copyright August 2008-2013 MonetDB B.V.
-- All Rights Reserved.

-- Vacuum a relational table should be done with care.
-- For, the oid's are used in join-indices.

-- Vacuum of tables may improve IO performance and disk footprint.
-- The foreign key constraints should be dropped before
-- and re-established after the cluster operation.

-- TODO: DATE, TIME, TIMESTAMP, CHAR and VARCHAR not supported yet
create function array_series("start" tinyint,  step tinyint,  stop tinyint,  N integer, M integer) returns table (id bigint, dimval tinyint)  external name "array".series_;
create function array_series("start" smallint, step smallint, stop smallint, N integer, M integer) returns table (id bigint, dimval smallint) external name "array".series_;
create function array_series("start" integer,  step integer,  stop integer,  N integer, M integer) returns table (id bigint, dimval integer)  external name "array".series_;
create function array_series("start" bigint,   step bigint,   stop bigint,   N integer, M integer) returns table (id bigint, dimval bigint)   external name "array".series_;
create function array_series("start" real,     step real,     stop real,     N integer, M integer) returns table (id bigint, dimval real)     external name "array".series_;
create function array_series("start" double,   step double,   stop double,   N integer, M integer) returns table (id bigint, dimval double)   external name "array".series_;

create function array_series1("start" tinyint,  step tinyint,  stop tinyint,  N integer, M integer) returns table (dimval tinyint)  external name "array".series;
create function array_series1("start" smallint, step smallint, stop smallint, N integer, M integer) returns table (dimval smallint) external name "array".series;
create function array_series1("start" integer,  step integer,  stop integer,  N integer, M integer) returns table (dimval integer)  external name "array".series;
create function array_series1("start" bigint,   step bigint,   stop bigint,   N integer, M integer) returns table (dimval bigint)   external name "array".series;
create function array_series1("start" real,     step real,     stop real,     N integer, M integer) returns table (dimval real)     external name "array".series;
create function array_series1("start" double,   step double,   stop double,   N integer, M integer) returns table (dimval double)   external name "array".series;

create function array_offsets(val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_offsets(val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint , val tinyint ) returns table (val tinyint ) external name "array".offsets;
create function array_offsets(val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint, val smallint) returns table (val smallint) external name "array".offsets;
create function array_offsets(val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer , val integer ) returns table (val integer ) external name "array".offsets;
create function array_offsets(val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  , val bigint  ) returns table (val bigint  ) external name "array".offsets;
create function array_offsets(val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    , val real    ) returns table (val real    ) external name "array".offsets;
create function array_offsets(val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  , val double  ) returns table (val double  ) external name "array".offsets;

create function array_filler(count bigint, val tinyint)       returns table (id bigint, cellval tinyint)    external name "array".filler_;
create function array_filler(count bigint, val smallint)      returns table (id bigint, cellval smallint)   external name "array".filler_;
create function array_filler(count bigint, val integer)       returns table (id bigint, cellval integer)    external name "array".filler_;
create function array_filler(count bigint, val bigint)        returns table (id bigint, cellval bigint)     external name "array".filler_;
create function array_filler(count bigint, val real)          returns table (id bigint, cellval real)       external name "array".filler_;
create function array_filler(count bigint, val double)        returns table (id bigint, cellval double)     external name "array".filler_;
create function array_filler(count bigint, val date)          returns table (id bigint, vals date)          external name "array".filler_;
create function array_filler(count bigint, val time)          returns table (id bigint, vals time)          external name "array".filler_;
create function array_filler(count bigint, val timestamp)     returns table (id bigint, vals timestamp)     external name "array".filler_;
create function array_filler(count bigint, val char(2048))    returns table (id bigint, vals char(2048))    external name "array".filler_;
create function array_filler(count bigint, val varchar(2048)) returns table (id bigint, vals varchar(2048)) external name "array".filler_;
create function array_filler(count bigint, val blob)          returns table (id bigint, vals blob)          external name "array".filler_;
create function array_filler(count bigint, val clob)          returns table (id bigint, vals clob)          external name "array".filler_;

create function array_avg(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 tinyint, offsets1 tinyint, size1 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 tinyint, offsets1 tinyint, size1 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 tinyint, offsets1 tinyint, size1 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 tinyint, offsets1 tinyint, size1 int) returns real      external name "array".min;
create function array_min(val double,   dim1 tinyint, offsets1 tinyint, size1 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 tinyint, offsets1 tinyint, size1 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 tinyint, offsets1 tinyint, size1 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 tinyint, offsets1 tinyint, size1 int) returns real      external name "array".max;
create function array_max(val double,   dim1 tinyint, offsets1 tinyint, size1 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 tinyint, offsets1 tinyint, size1 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns real      external name "array".min;
create function array_min(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns real      external name "array".max;
create function array_max(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns real      external name "array".min;
create function array_min(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns real      external name "array".max;
create function array_max(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".avg;
                                                                                                                                                                                                                  
create function array_sum(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double external name "array".sum;
                                                                                                                                                                                                                  
create function array_min(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns real      external name "array".min;
create function array_min(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double    external name "array".min;
                                                                                                                                                                                                                  
create function array_max(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns real      external name "array".max;
create function array_max(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 tinyint, offsets1 tinyint, size1 int, dim2 tinyint, offsets2 tinyint, size2 int, dim3 tinyint, offsets3 tinyint, size3 int, dim4 tinyint, offsets4 tinyint, size4 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 smallint, offsets1 smallint, size1 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 smallint, offsets1 smallint, size1 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 smallint, offsets1 smallint, size1 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 smallint, offsets1 smallint, size1 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 smallint, offsets1 smallint, size1 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 smallint, offsets1 smallint, size1 int) returns real      external name "array".min;
create function array_min(val double,   dim1 smallint, offsets1 smallint, size1 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 smallint, offsets1 smallint, size1 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 smallint, offsets1 smallint, size1 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 smallint, offsets1 smallint, size1 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 smallint, offsets1 smallint, size1 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 smallint, offsets1 smallint, size1 int) returns real      external name "array".max;
create function array_max(val double,   dim1 smallint, offsets1 smallint, size1 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 smallint, offsets1 smallint, size1 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns real      external name "array".min;
create function array_min(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns real      external name "array".max;
create function array_max(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns real      external name "array".min;
create function array_min(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns real      external name "array".max;
create function array_max(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".avg;
                                                                                                                                                                                                                  
create function array_sum(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double external name "array".sum;
                                                                                                                                                                                                                  
create function array_min(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns real      external name "array".min;
create function array_min(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double    external name "array".min;
                                                                                                                                                                                                                  
create function array_max(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns real      external name "array".max;
create function array_max(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 smallint, offsets1 smallint, size1 int, dim2 smallint, offsets2 smallint, size2 int, dim3 smallint, offsets3 smallint, size3 int, dim4 smallint, offsets4 smallint, size4 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 int, offsets1 int, size1 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 int, offsets1 int, size1 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 int, offsets1 int, size1 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 int, offsets1 int, size1 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 int, offsets1 int, size1 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 int, offsets1 int, size1 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 int, offsets1 int, size1 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 int, offsets1 int, size1 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 int, offsets1 int, size1 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 int, offsets1 int, size1 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 int, offsets1 int, size1 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 int, offsets1 int, size1 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 int, offsets1 int, size1 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 int, offsets1 int, size1 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 int, offsets1 int, size1 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 int, offsets1 int, size1 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 int, offsets1 int, size1 int) returns real      external name "array".min;
create function array_min(val double,   dim1 int, offsets1 int, size1 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 int, offsets1 int, size1 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 int, offsets1 int, size1 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 int, offsets1 int, size1 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 int, offsets1 int, size1 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 int, offsets1 int, size1 int) returns real      external name "array".max;
create function array_max(val double,   dim1 int, offsets1 int, size1 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 int, offsets1 int, size1 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 int, offsets1 int, size1 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 int, offsets1 int, size1 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 int, offsets1 int, size1 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 int, offsets1 int, size1 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 int, offsets1 int, size1 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns real      external name "array".min;
create function array_min(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns real      external name "array".max;
create function array_max(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".avg;

create function array_sum(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double external name "array".sum;

create function array_min(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns real      external name "array".min;
create function array_min(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double    external name "array".min;

create function array_max(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns real      external name "array".max;
create function array_max(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int) returns bigint external name "array".cnt;

create function array_avg(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".avg;
create function array_avg(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".avg;
create function array_avg(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".avg;
create function array_avg(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".avg;
create function array_avg(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".avg;
create function array_avg(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".avg;
                                                                                                                                                                          
create function array_sum(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".sum;
create function array_sum(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".sum;
create function array_sum(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".sum;
create function array_sum(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".sum;
create function array_sum(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".sum;
create function array_sum(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double external name "array".sum;
                                                                                                                                                                          
create function array_min(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns tinyint   external name "array".min;
create function array_min(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns smallint  external name "array".min;
create function array_min(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns int       external name "array".min;
create function array_min(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint    external name "array".min;
create function array_min(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns real      external name "array".min;
create function array_min(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double    external name "array".min;
                                                                                                                                                                          
create function array_max(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns tinyint   external name "array".max;
create function array_max(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns smallint  external name "array".max;
create function array_max(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns int       external name "array".max;
create function array_max(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint    external name "array".max;
create function array_max(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns real      external name "array".max;
create function array_max(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns double    external name "array".max;

create function array_count(val tinyint,  dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".cnt;
create function array_count(val smallint, dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".cnt;
create function array_count(val int,      dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".cnt;
create function array_count(val bigint,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".cnt;
create function array_count(val real,     dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".cnt;
create function array_count(val double,   dim1 int, offsets1 int, size1 int, dim2 int, offsets2 int, size2 int, dim3 int, offsets3 int, size3 int, dim4 int, offsets4 int, size4 int) returns bigint external name "array".cnt;

create filter function array_slice(dim1_strt bigint, dim1_step bigint, dim1_stop bigint, slice1_strt bigint, slice1_step bigint, slice1_stop bigint) external name "array".slice;
create filter function array_slice(dim1_strt bigint, dim1_step bigint, dim1_stop bigint, slice1_strt bigint, slice1_step bigint, slice1_stop bigint, dim2_strt bigint, dim2_step bigint, dim2_stop bigint, slice2_strt bigint, slice2_step bigint, slice2_stop bigint) external name "array".slice;
create filter function array_slice(dim1_strt bigint, dim1_step bigint, dim1_stop bigint, slice1_strt bigint, slice1_step bigint, slice1_stop bigint, dim2_strt bigint, dim2_step bigint, dim2_stop bigint, slice2_strt bigint, slice2_step bigint, slice2_stop bigint, dim3_strt bigint, dim3_step bigint, dim3_stop bigint, slice3_strt bigint, slice3_step bigint, slice3_stop bigint) external name "array".slice;
create filter function array_slice(dim1_strt bigint, dim1_step bigint, dim1_stop bigint, slice1_strt bigint, slice1_step bigint, slice1_stop bigint, dim2_strt bigint, dim2_step bigint, dim2_stop bigint, slice2_strt bigint, slice2_step bigint, slice2_stop bigint, dim3_strt bigint, dim3_step bigint, dim3_stop bigint, slice3_strt bigint, slice3_step bigint, slice3_stop bigint, dim4_strt bigint, dim4_step bigint, dim4_stop bigint, slice4_strt bigint, slice4_step bigint, slice4_stop bigint) external name "array".slice;

