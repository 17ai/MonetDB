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
-- Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
-- Copyright August 2008-2016 MonetDB B.V.
-- All Rights Reserved.

-- This is the first interface for continuous queries

create schema cquery;

create procedure cquery.register(sch string, cqname string)
	external name cquery.register;

create procedure cquery.resume()
	external name cquery.resume;
create procedure cquery.resume(sch string, cqname string)
	external name cquery.resume;

create procedure cquery.pause()
	external name cquery.pause;
create procedure cquery.pause(sch string, cqname string)
	external name cquery.pause;

create procedure cquery.stop()
	external name cquery.stop;
create procedure cquery.stop(sch string, cqname string)
	external name cquery.stop;

create procedure cquery.deregister()
	external name cquery.deregister;
create procedure cquery.deregister(sch string, cqname string)
	external name cquery.deregister;

-- The following commands can be part of the cquery itself
create procedure cquery.wait(ms integer)
	external name cquery.wait;
create procedure cquery.wait(ms bigint)
	external name cquery.wait;

-- Limit the number of iterations of a CQ
create procedure cquery.cycles(cycles integer)
	external name cquery.cycles;
create procedure cquery.cycles(sch string, cqname string, cycles integer)
	external name cquery.cycles;

-- set the scheduler heartbeat 
create procedure cquery.heartbeat("schema" string, qryname string, msec integer)
	external name cquery.heartbeat;
create procedure cquery.heartbeat(msec integer)
	external name cquery.heartbeat;

-- Tumble the stream buffer
create procedure cquery.tumble("schema" string, "table" string, elem integer)
	external name cquery.tumble;

-- Window based consumption for stream queries
create procedure cquery.window("schema" string, "table" string, elem integer)
	external name cquery.window;

-- continuous query status analysis

create function cquery.log()
 returns table(tick timestamp,  "schema" string, "function" string, time bigint, errors string)
 external name cquery.log;

create function cquery.summary()
 returns table( "schema" string, "function" string, runs int, totaltime bigint)
begin
 return select "schema","function", count(*), sum(time) from cquery.log() group by "schema","function";
end;

create function cquery.status()
 returns table(tick timestamp,  "schema" string, "function" string, state string, errors string)
 external name cquery.status;

create function cquery.show("schema" string, qryname string)
returns string
external name cquery.show;

-- Debugging status
create procedure cquery.dump()
external name cquery.dump;
