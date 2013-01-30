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
 * Copyright August 2008-2012 MonetDB B.V.
 * All Rights Reserved.
 */

#ifndef _RDFSCHEMA_H_
#define _RDFSCHEMA_H_

rdf_export str
RDFSchemaExplore(int *ret, str *tbname, str *clname);

rdf_export str
RDFextractCS(int *ret, bat *sbatid, bat *pbatid, int *freqThreshold); 

rdf_export str
RDFextractPfromPSO(int *ret, bat *pbatid, bat *sbatid); 

rdf_export str 
RDFextractCSwithTypes(int *ret, bat *sbatid, bat *pbatid, bat *obatid, int *freqThreshold);

typedef struct {
	BAT*	keyBat; 
	BAT* 	valueBat; 
} hsBats; 

typedef struct CS
{
	oid 	subIdx;		//Id of subject
	oid*	lstProp;	//List of properties' Ids
	int	numProp;
	int	numAllocation;
	char 	isSubset; 
} CS;

#define INIT_NUM_CS 100

typedef struct CSset{
	CS* items;
	int numCSadded;
	int numAllocation;
} CSset; 

#endif /* _RDFSCHEMA_H_ */
