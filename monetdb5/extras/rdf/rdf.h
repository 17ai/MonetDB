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
 * @f rdf
 * @a L.Sidirourgos, Minh-Duc Pham
 *
 * @* The RDF module For MonetDB5 (aka. MonetDB/RDF)
 *
 */
#ifndef _RDF_H_
#define _RDF_H_

#include <gdk.h>

#ifdef WIN32
#ifndef LIBRDF
#define rdf_export extern __declspec(dllimport)
#else
#define rdf_export extern __declspec(dllexport)
#endif
#else
#define rdf_export extern
#endif

/* internal debug messages */
#define _RDF_DEBUG

rdf_export str
RDFParser(BAT **graph, str *location, str *graphname, str *schemam);

rdf_export str 
RDFleftfetchjoin_sortedestimate(int *result, int *lid, int *rid, lng *estimate);
rdf_export str 
RDFleftfetchjoin_sorted(int *result, int* lid, int *rid);

rdf_export str 
TKNZRrdf2str (bat *res, bat *bid, bat *map);

rdf_export str
RDFpartialjoin (bat *res, bat *lmap, bat *rmap, bat *input); 



#define RDF_MIN_LITERAL (((oid) 1) << ((sizeof(oid)==8)?59:27))

typedef enum {
	URI,		
	DATETIME, 
	INTEGER,
	FLOAT,	
	STRING,
	BLANKNODE,
	MULTIVALUES		// For the multi-value property 
} ObjectType; 

#define IS_DUPLICATE_FREE 0		/* 0: Duplications have not been removed, otherwise 1 */
#define IS_COMPACT_TRIPLESTORE 1	/* 1: Only keep SPO for triple store */
#define TRIPLE_STORE 1
#define MLA_STORE    2
#define NOT_IGNORE_ERROR_TRIPLE 0
#define USE_MULTIPLICITY 1		/* Properties having >= 2 values are being considered as having 
					the same type, i.e., MULTIVALUES */

#define STORE TRIPLE_STORE /* this should become a compile time option */

#define batsz 10000000
#define smallbatsz 100000

#if STORE == TRIPLE_STORE
 typedef enum {
	S_sort, P_sort, O_sort, /* sorted */
	P_PO, O_PO, /* spo */
	P_OP, O_OP, /* sop */
	S_SO, O_SO, /* pso */
	S_OS, O_OS, /* pos */
	S_SP, P_SP, /* osp */
	S_PS, P_PS, /* ops */
	MAP_LEX
 } graphBATType;
#elif STORE == MLA_STORE
 typedef enum {
	S_sort, P_sort, O_sort,
	MAP_LEX
 } graphBATType;
#endif /* STORE */

#define N_GRAPH_BAT (MAP_LEX+1)

#define INFO_WHERE_NAME_FROM 1
#define TOP_GENERAL_NAME 2	//Level of hierrachy in which a name is considered to be a general name
				//For example, PERSON, THING is at level 1	
#define	USE_ALTERNATIVE_NAME 0	//Use different but may be better name for a general name

// Final data structure that stores the labels for tables and attributes
typedef struct CSlabel {
	oid		name;		// table name
	oid		*candidates;	// list of table name candidates, candidates[0] == name
	int		candidatesCount;// number of entries in the candidates list
	int		candidatesNew;		// number of candidates that are created during merging (e.g. ancestor name)
	int		candidatesOntology;	// number of ontology candidates (first category)
	int		candidatesType;		// number of type candidates (second category)
	int		candidatesFK;		// number of fk candidates (third category)
	oid		*hierarchy;     // hierarchy "bottom to top"
	int		hierarchyCount; // number of entries in the hierarchy list
	int		numProp;	// number of properties, copied from freqCSset->items[x].numProp
	oid		*lstProp;	// attribute names (same order as in freqCSset->items[x].lstProp)
	#if 	INFO_WHERE_NAME_FROM	
	char 		isOntology; 	// First name is decided by ontology
	char		isType; 	// First name is decided based on Type
	char		isFK; 	
	#endif
} CSlabel;

#endif /* _RDF_H_ */
