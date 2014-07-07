/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2013 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * @* The RDF module For MonetDB5 (aka. MonetDB/RDF)
 *
 */
#ifndef _RDFTYPES_H_
#define _RDFTYPES_H_

#include <mtime.h>

#ifdef WIN32
#ifndef LIBRDF
#define rdf_export extern __declspec(dllimport)
#else
#define rdf_export extern __declspec(dllexport)
#endif
#else
#define rdf_export extern
#endif


typedef enum {
	URI,		
	DATETIME, 
	INTEGER,
	DOUBLE,	
	STRING,
	BLANKNODE,
	MULTIVALUES		// For the multi-value property 
} ObjectType; 

rdf_export char* 
substring(char *string, int position, int length);

rdf_export int
is_little_endian(void);

rdf_export char 
isInt(char *input, int len);

rdf_export char 
isDouble(char *input, int len);

rdf_export int
getIntFromRDFString(str input); 

rdf_export double
getDoubleFromRDFString(str input); 

rdf_export str
getDateTimeFromRDFString(str input); 

rdf_export char 
rdfcast(ObjectType srcT, ObjectType dstT, ValPtr srcPtr, ValPtr dstPrt); 

rdf_export void
encodeValueInOid(ValPtr vrPtrRealValue, ObjectType objType, BUN* bun);

rdf_export void
decodeValueFromOid(BUN bun, ObjectType objType, ValPtr vrPtrRealValue);

rdf_export int 
convertDateTimeStringToTimeT(char *sDateTime, int len, time_t *t);

rdf_export void
convertTMtimeToMTime(time_t t, timestamp *ts);

#endif /* _RDFTYPES_H_ */
