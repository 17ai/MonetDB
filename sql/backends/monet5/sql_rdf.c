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
 * Copyright August 2008-2015 MonetDB B.V.
 * All Rights Reserved.
 */

#include "monetdb_config.h"
#include "sql.h"
#include "sql_result.h"
#include "sql_gencode.h"
#include <sql_storage.h>
#include <sql_scenario.h>
#include <store_sequence.h>
#include <sql_optimizer.h>
#include <sql_datetime.h>
#include <rel_optimizer.h>
#include <rel_distribute.h>
#include <rel_select.h>
#include <rel_exp.h>
#include <rel_dump.h>
#include <rel_bin.h>
#include <bbp.h>
#include <cluster.h>
#include <opt_pipes.h>
#include "clients.h"
#include "sql_rdf.h"
#include "mal_instruction.h"
#include "rdfontologyload.h"

/*
 * Shredding RDF documents through SQL
 * Wrapper around the RDF shredder of the rdf module of M5.
 *
 * An rdf file can be now shredded with SQL command:
 * CALL rdf_shred('/path/to/location','graph name');
 *
 * The table rdf.graph will be updated with an entry of the form:
 * [graph name, graph id] -> [gname,gid].
 *
 * In addition all permutation of SPO for the specific rdf document will be
 * created. The name of the triple tables are rdf.pso$gid$, rdf.spo$gid$ etc.
 * For example if gid = 3 then rdf.spo3 is the triple table ordered on subject,
 * property, object. Finally, there is one more table called rdf.map$gid$ that
 * maps oids to strings (i.e., the lexical representation).
 */

str
SQLrdfShred(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
#ifdef HAVE_RAPTOR
	BAT *b[128];
	BAT *p, *s, *o;
	sql_schema *sch;
	sql_table *g_tbl;
	sql_column *gname, *gid;
#if STORE == TRIPLE_STORE
	#if IS_COMPACT_TRIPLESTORE == 0				       
	sql_table *spo_tbl, *sop_tbl, *pso_tbl, *pos_tbl, *osp_tbl, *ops_tbl;
	#else
	sql_table *spo_tbl;
	#endif
#elif STORE == MLA_STORE
	sql_table *spo_tbl;
#endif /* STORE */
	sql_table *map_tbl;
	sql_subtype tpe;
	str *location = getArgReference_str(stk, pci, 1);
	str *name = getArgReference_str(stk, pci, 2);
	str *schema = getArgReference_str(stk, pci, 3);
	char buff[24];
	mvc *m = NULL;
	int id = 0;
	oid rid = oid_nil;
	str msg;
#if IS_DUPLICATE_FREE == 0
	BATiter si, pi, oi; 
	BUN	pb, qb; 
	oid 	*sbt, *pbt, *obt; 
	oid	curS = 0, curP = 0, curO = 0;
#endif /* IS_DUPLICATE_FREE */

	BAT *ontbat = NULL; 
	clock_t tmpbeginT, tmpendT, beginT, endT; 
	
	beginT = clock();

	rethrow("sql.rdfShred", msg, getSQLContext(cntxt, mb, &m, NULL));

	if ((sch = mvc_bind_schema(m, *schema)) == NULL)
		throw(SQL, "sql.rdfShred", "3F000!schema missing");

	g_tbl = mvc_bind_table(m, sch, "graph");
	gname = mvc_bind_column(m, g_tbl, "gname");
	gid = mvc_bind_column(m, g_tbl, "gid");

	rid = table_funcs.column_find_row(m->session->tr, gname, *name, NULL);
	if (rid != oid_nil)
		throw(SQL, "sql.rdfShred", "graph name already exists in rdf.graph");

	id = (int) store_funcs.count_col(m->session->tr, gname, 1);
	store_funcs.append_col(m->session->tr, gname, *name, TYPE_str);
	store_funcs.append_col(m->session->tr, gid, &id, TYPE_int);

	ontbat = mvc_bind(m, "sys", "ontlist", "mont",0);

	tmpbeginT = clock(); 
	if (ontbat == NULL){
		printf("There is no column ontlist/mont \n"); 
		rethrow("sql.rdfShred", msg, RDFParser(b, location, name, schema, NULL));
	}
	else{
		rethrow("sql.rdfShred", msg, RDFParser(b, location, name, schema, &ontbat->batCacheid));

		BBPunfix(ontbat->batCacheid);
	}
	tmpendT = clock(); 
	printf ("Parsing process took %f seconds.\n", ((float)(tmpendT - tmpbeginT))/CLOCKS_PER_SEC);

	if (sizeof(oid) == 8) {
		sql_find_subtype(&tpe, "oid", 31, 0);
		/* todo for niels: if use int/bigint the @0 is serialized */
		/* sql_find_subtype(&tpe, "bigint", 64, 0); */
	} else {
		sql_find_subtype(&tpe, "oid", 31, 0);
		/* sql_find_subtype(&tpe, "int", 32, 0); */
	}
#if STORE == TRIPLE_STORE
	sprintf(buff, "spo%d", id);
	spo_tbl = mvc_create_table(m, sch, buff, tt_table, 0,
				   SQL_PERSIST, 0, 3);
	mvc_create_column(m, spo_tbl, "subject", &tpe);
	mvc_create_column(m, spo_tbl, "property", &tpe);
	mvc_create_column(m, spo_tbl, "object", &tpe);

	#if IS_COMPACT_TRIPLESTORE == 0 
	tmpbeginT = clock(); 
	sprintf(buff, "sop%d", id);
	sop_tbl = mvc_create_table(m, sch, buff, tt_table, 0,
				   SQL_PERSIST, 0, 3);
	mvc_create_column(m, sop_tbl, "subject", &tpe);
	mvc_create_column(m, sop_tbl, "object", &tpe);
	mvc_create_column(m, sop_tbl, "property", &tpe);

	sprintf(buff, "pso%d", id);
	pso_tbl = mvc_create_table(m, sch, buff, tt_table, 0,
				   SQL_PERSIST, 0, 3);
	mvc_create_column(m, pso_tbl, "property", &tpe);
	mvc_create_column(m, pso_tbl, "subject", &tpe);
	mvc_create_column(m, pso_tbl, "object", &tpe);

	sprintf(buff, "pos%d", id);
	pos_tbl = mvc_create_table(m, sch, buff, tt_table, 0,
				   SQL_PERSIST, 0, 3);
	mvc_create_column(m, pos_tbl, "property", &tpe);
	mvc_create_column(m, pos_tbl, "object", &tpe);
	mvc_create_column(m, pos_tbl, "subject", &tpe);

	sprintf(buff, "osp%d", id);
	osp_tbl = mvc_create_table(m, sch, buff, tt_table, 0,
				   SQL_PERSIST, 0, 3);
	mvc_create_column(m, osp_tbl, "object", &tpe);
	mvc_create_column(m, osp_tbl, "subject", &tpe);
	mvc_create_column(m, osp_tbl, "property", &tpe);

	sprintf(buff, "ops%d", id);
	ops_tbl = mvc_create_table(m, sch, buff, tt_table, 0,
				   SQL_PERSIST, 0, 3);
	mvc_create_column(m, ops_tbl, "object", &tpe);
	mvc_create_column(m, ops_tbl, "property", &tpe);
	mvc_create_column(m, ops_tbl, "subject", &tpe);

	tmpendT = clock(); 
	printf ("Creating remaining triple tables took %f seconds.\n", ((float)(tmpendT - tmpbeginT))/CLOCKS_PER_SEC);

	#endif /* IS_COMPACT_TRIPLESTORE == 0 */

#elif STORE == MLA_STORE
	sprintf(buff, "spo%d", id);
	spo_tbl = mvc_create_table(m, sch, buff, tt_table,
				   0, SQL_PERSIST, 0, 3);
	mvc_create_column(m, spo_tbl, "subject", &tpe);
	mvc_create_column(m, spo_tbl, "property", &tpe);
	mvc_create_column(m, spo_tbl, "object", &tpe);
#endif /* STORE */

	sprintf(buff, "map%d", id);
	map_tbl = mvc_create_table(m, sch, buff, tt_table, 0, SQL_PERSIST, 0, 2);
	mvc_create_column(m, map_tbl, "sid", &tpe);
	sql_find_subtype(&tpe, "varchar", 1024, 0);
	mvc_create_column(m, map_tbl, "lexical", &tpe);

	s = b[MAP_LEX];
	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, map_tbl, "lexical"),
			BATmirror(BATmark(BATmirror(s),0)), TYPE_bat);
	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, map_tbl, "sid"),
			BATmirror(BATmark(s, 0)),
			TYPE_bat);
	BBPunfix(s->batCacheid);

#if STORE == TRIPLE_STORE
	#if IS_DUPLICATE_FREE == 0
		
		s = b[S_sort];
		p = b[P_PO];
		o = b[O_PO];
		si = bat_iterator(s); 
		pi = bat_iterator(p); 
		oi = bat_iterator(o); 

		BATloop(s, pb, qb){
			sbt = (oid *) BUNtloc(si, pb);
			pbt = (oid *) BUNtloc(pi, pb);
			obt = (oid *) BUNtloc(oi, pb);

			if (*sbt != curS || *pbt != curP || *obt != curO){

				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, spo_tbl, "subject"),
						       sbt, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, spo_tbl, "property"),
						       pbt, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, spo_tbl, "object"),
						       obt, TYPE_oid);
				/* Update current value */		       
				curS = *sbt; 
				curP = *pbt; 
				curO = *obt; 
			}
		}
		
		#if IS_COMPACT_TRIPLESTORE == 0
		tmpbeginT = clock(); 
		s = b[S_sort];
		p = b[P_OP];
		o = b[O_OP];

		si = bat_iterator(s); 
		pi = bat_iterator(p); 
		oi = bat_iterator(o); 
		
		curS = 0;
		curP = 0;
		curO = 0;

		BATloop(s, pb, qb){
			sbt = (oid *) BUNtloc(si, pb);
			pbt = (oid *) BUNtloc(pi, pb);
			obt = (oid *) BUNtloc(oi, pb);

			if (*sbt != curS || *pbt != curP || *obt != curO){
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, sop_tbl, "subject"),
						       s, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, sop_tbl, "property"),
						       p, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, sop_tbl, "object"),
						       o, TYPE_oid);
				
				/* Update current value */		       
				curS = *sbt; 
				curP = *pbt; 
				curO = *obt; 
			}
		}

		s = b[S_SO];
		p = b[P_sort];
		o = b[O_SO];

		si = bat_iterator(s); 
		pi = bat_iterator(p); 
		oi = bat_iterator(o); 
		
		curS = 0;
		curP = 0;
		curO = 0;

		BATloop(s, pb, qb){
			sbt = (oid *) BUNtloc(si, pb);
			pbt = (oid *) BUNtloc(pi, pb);
			obt = (oid *) BUNtloc(oi, pb);

			if (*sbt != curS || *pbt != curP || *obt != curO){
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, pso_tbl, "subject"),
						       s, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, pso_tbl, "property"),
						       p, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, pso_tbl, "object"),
						       o, TYPE_oid);		
				
				/* Update current value */		       
				curS = *sbt; 
				curP = *pbt; 
				curO = *obt; 
			}
		}
		s = b[S_OS];
		p = b[P_sort];
		o = b[O_OS];
		
		si = bat_iterator(s); 
		pi = bat_iterator(p); 
		oi = bat_iterator(o); 
		
		curS = 0;
		curP = 0;
		curO = 0;

		BATloop(s, pb, qb){
			sbt = (oid *) BUNtloc(si, pb);
			pbt = (oid *) BUNtloc(pi, pb);
			obt = (oid *) BUNtloc(oi, pb);

			if (*sbt != curS || *pbt != curP || *obt != curO){
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, pos_tbl, "subject"),
						       s, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, pos_tbl, "property"),
						       p, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, pos_tbl, "object"),
						       o, TYPE_oid);
				
				/* Update current value */		       
				curS = *sbt; 
				curP = *pbt; 
				curO = *obt; 
			}
		}

		s = b[S_SP];
		p = b[P_SP];
		o = b[O_sort];
		
		si = bat_iterator(s); 
		pi = bat_iterator(p); 
		oi = bat_iterator(o); 
		
		curS = 0;
		curP = 0;
		curO = 0;

		BATloop(s, pb, qb){
			sbt = (oid *) BUNtloc(si, pb);
			pbt = (oid *) BUNtloc(pi, pb);
			obt = (oid *) BUNtloc(oi, pb);

			if (*sbt != curS || *pbt != curP || *obt != curO){
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, osp_tbl, "subject"),
						       s, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, osp_tbl, "property"),
						       p, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, osp_tbl, "object"),
						       o, TYPE_oid);
				
				/* Update current value */		       
				curS = *sbt; 
				curP = *pbt; 
				curO = *obt; 
			}
		}

		s = b[S_PS];
		p = b[P_PS];
		o = b[O_sort];
		
		si = bat_iterator(s); 
		pi = bat_iterator(p); 
		oi = bat_iterator(o); 
		
		curS = 0;
		curP = 0;
		curO = 0;

		BATloop(s, pb, qb){
			sbt = (oid *) BUNtloc(si, pb);
			pbt = (oid *) BUNtloc(pi, pb);
			obt = (oid *) BUNtloc(oi, pb);

			if (*sbt != curS || *pbt != curP || *obt != curO){
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, ops_tbl, "subject"),
						       s, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, ops_tbl, "property"),
						       p, TYPE_oid);
				store_funcs.append_col(m->session->tr,
						       mvc_bind_column(m, ops_tbl, "object"),
						       o, TYPE_oid);

				/* Update current value */		       
				curS = *sbt; 
				curP = *pbt; 
				curO = *obt; 
			}
		}
		
		tmpendT = clock(); 

		printf ("Inserting from BATs to remaining triple tables took %f seconds.\n", ((float)(tmpendT - tmpbeginT))/CLOCKS_PER_SEC);

		#endif	/* IS_COMPACT_TRIPLESTORE == 0 */		 
			       
	#else  /* IS_DUPLICATE_FREE == 1*/
	
		s = b[S_sort];
		p = b[P_PO];
		o = b[O_PO];
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, spo_tbl, "subject"),
				       s, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, spo_tbl, "property"),
				       p, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, spo_tbl, "object"),
				       o, TYPE_bat);
				       
		#if IS_COMPACT_TRIPLESTORE == 0				       

		s = b[S_sort];
		p = b[P_OP];
		o = b[O_OP];
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, sop_tbl, "subject"),
				       s, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, sop_tbl, "property"),
				       p, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, sop_tbl, "object"),
				       o, TYPE_bat);
				       

		s = b[S_SO];
		p = b[P_sort];
		o = b[O_SO];
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, pso_tbl, "subject"),
				       s, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, pso_tbl, "property"),
				       p, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, pso_tbl, "object"),
				       o, TYPE_bat);
		s = b[S_OS];
		p = b[P_sort];
		o = b[O_OS];
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, pos_tbl, "subject"),
				       s, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, pos_tbl, "property"),
				       p, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, pos_tbl, "object"),
				       o, TYPE_bat);
		s = b[S_SP];
		p = b[P_SP];
		o = b[O_sort];
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, osp_tbl, "subject"),
				       s, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, osp_tbl, "property"),
				       p, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, osp_tbl, "object"),
				       o, TYPE_bat);
		s = b[S_PS];
		p = b[P_PS];
		o = b[O_sort];
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, ops_tbl, "subject"),
				       s, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, ops_tbl, "property"),
				       p, TYPE_bat);
		store_funcs.append_col(m->session->tr,
				       mvc_bind_column(m, ops_tbl, "object"),
				       o, TYPE_bat);
		#endif /* IS_COMPACT_TRIPLESTORE == 0 */	       
	#endif /* IS_DUPLICATE_FREE */

#elif STORE == MLA_STORE
	s = b[S_sort];
	p = b[P_sort];
	o = b[O_sort];
	store_funcs.append_col(m->session->tr,
			       mvc_bind_column(m, spo_tbl, "subject"),
			       s, TYPE_bat);
	store_funcs.append_col(m->session->tr,
			       mvc_bind_column(m, spo_tbl, "property"),
			       p, TYPE_bat);
	store_funcs.append_col(m->session->tr,
			       mvc_bind_column(m, spo_tbl, "object"),
			       o, TYPE_bat);
#endif /* STORE */

	/* unfix graph */
	for(id=0; b[id]; id++) {
		BBPunfix(b[id]->batCacheid);
	}
	
	endT = clock(); 
	printf ("Full rdf_shred() took %f seconds.\n", ((float)(endT - beginT))/CLOCKS_PER_SEC);

	return MAL_SUCCEED;
#else
	(void) cntxt; (void) mb; (void) stk; (void) pci;
	throw(SQL, "sql.rdfShred", "RDF support is missing from MonetDB5");
#endif /* HAVE_RAPTOR */
}

void getTblSQLname(char *tmptbname, int tblIdx, int isExTbl, oid tblname, BATiter mapi, BAT *mbat){
	str	baseTblName;
	char	tmpstr[20]; 

	if (isExTbl ==0) 
		sprintf(tmpstr, "%d",tblIdx);
	else //isExTbl == 1
		sprintf(tmpstr, "ex%d",tblIdx);

	getSqlName(&baseTblName, tblname, mapi, mbat); 
	sprintf(tmptbname, "%s", baseTblName);
	strcat(tmptbname,tmpstr);

	GDKfree(baseTblName);
}

//If colType == -1, ==> default col
//If not, it is a ex-type column

void getColSQLname(char *tmpcolname, int colIdx, int colType, oid propid, BATiter mapi, BAT *mbat){
	str baseColName;
	char    tmpstr[20];

	if (colType == -1) sprintf(tmpstr, "%d",colIdx);
	else 
		sprintf(tmpstr, "%dtype%d",colIdx, colType); 
	getSqlName(&baseColName, propid, mapi, mbat);
	sprintf(tmpcolname, "%s", baseColName);
	strcat(tmpcolname,tmpstr); 


	GDKfree(baseColName);
}

void getMvTblSQLname(char *tmpmvtbname, int tblIdx, int colIdx, oid tblname, oid propid, BATiter mapi, BAT *mbat){
	str baseTblName;
	str baseColName; 

	getSqlName(&baseTblName, tblname, mapi, mbat);
	getSqlName(&baseColName, propid, mapi, mbat);

	sprintf(tmpmvtbname, "mv%s%d_%s%d", baseTblName, tblIdx, baseColName, colIdx);

	GDKfree(baseTblName);
	GDKfree(baseColName);
}

static
void addPKandFKs(CStableStat* cstablestat, CSPropTypes *csPropTypes, str schema, BATiter mapi, BAT *mbat){
	FILE            *fout, *foutPK, *foutMV;
	char            filename[100];
	int		i, j;
	char		fromTbl[100]; 
	char		fromTblCol[100]; 
	char		toTbl[100];
	char		mvTbl[100]; 
	char		mvCol[100];
	int		refTblId; 
	int		tblColIdx; 

	strcpy(filename, "fkCreate.sql");
	fout = fopen(filename, "wt");
	foutPK = fopen("pkCreate.sql","wt");
	foutMV = fopen("mvRefCreate.sql","wt"); 

	for (i = 0; i < cstablestat->numTables; i++){

		//Add PKs to all subject columns
		getTblSQLname(fromTbl, i, 0, cstablestat->lstcstable[i].tblname, mapi, mbat);
		fprintf(foutPK, "ALTER TABLE %s.\"%s\" ADD PRIMARY KEY (subject);\n",schema,fromTbl);
	
		//Add unique key constraint and FKs between MVTable and its corresponding column
		//in the default table
		for (j = 0; j < cstablestat->numPropPerTable[i]; j++){
			if (cstablestat->lstcstable[i].lstMVTables[j].numCol != 0){	//MVColumn
				getColSQLname(fromTblCol, j, -1, cstablestat->lstcstable[i].lstProp[j], mapi, mbat);
				getMvTblSQLname(mvTbl, i, j, cstablestat->lstcstable[i].tblname, cstablestat->lstcstable[i].lstProp[j], mapi, mbat);
				fprintf(foutMV, "ALTER TABLE %s.\"%s\" ADD UNIQUE (\"%s\");\n",schema,fromTbl,fromTblCol);		
				fprintf(foutMV, "ALTER TABLE %s.\"%s\" ADD FOREIGN KEY (\"mvKey\") REFERENCES %s.\"%s\" (\"%s\");\n",schema, mvTbl, schema, fromTbl,fromTblCol);
			
			}
		}
	}

	for (i = 0; i < cstablestat->numTables; i++){
		for(j = 0; j < csPropTypes[i].numProp; j++){
			if (csPropTypes[i].lstPropTypes[j].defColIdx == -1)	continue;
			if (csPropTypes[i].lstPropTypes[j].isFKProp == 1){
				tblColIdx = csPropTypes[i].lstPropTypes[j].defColIdx; 
				getTblSQLname(fromTbl, i, 0, cstablestat->lstcstable[i].tblname, mapi, mbat);
				refTblId = csPropTypes[i].lstPropTypes[j].refTblId;
				getTblSQLname(toTbl, refTblId, 0, cstablestat->lstcstable[refTblId].tblname, mapi, mbat);

				if (cstablestat->lstcstable[i].lstMVTables[tblColIdx].numCol == 0){
					getColSQLname(fromTblCol, tblColIdx, -1, cstablestat->lstcstable[i].lstProp[tblColIdx], mapi, mbat);

					fprintf(fout, "ALTER TABLE %s.\"%s\" ADD FOREIGN KEY (\"%s\") REFERENCES %s.\"%s\" (subject);\n\n", schema, fromTbl, fromTblCol, schema, toTbl);

				}
				else{	//This is a MV col
					getMvTblSQLname(mvTbl, i, tblColIdx, cstablestat->lstcstable[i].tblname, cstablestat->lstcstable[i].lstProp[tblColIdx], mapi, mbat);
					getColSQLname(fromTblCol, tblColIdx, -1, cstablestat->lstcstable[i].lstProp[tblColIdx], mapi, mbat);
					getColSQLname(mvCol, tblColIdx, 0, cstablestat->lstcstable[i].lstProp[tblColIdx], mapi, mbat); //Use the first column of MVtable
					
					if (0){		//Do not create the FK from MVtable to the original table
							//Since that column in original table may contains lots of NULL value
					fprintf(fout, "ALTER TABLE %s.\"%s\" ADD PRIMARY KEY (\"%s\");\n",schema, fromTbl,fromTblCol);
					fprintf(fout, "ALTER TABLE %s.\"%s\" ADD FOREIGN KEY (mvKey) REFERENCES %s.\"%s\" (\"%s\");\n",schema, mvTbl, schema, fromTbl,fromTblCol);
					}
					fprintf(fout, "ALTER TABLE %s.\"%s\" ADD FOREIGN KEY (\"%s\") REFERENCES %s.\"%s\" (subject);\n\n",schema, mvTbl, mvCol, schema, toTbl);
					
				}
			}
		}
	}
	fclose(fout); 	
	fclose(foutPK); 
	fclose(foutMV); 

}

static
int isRightPropBAT(BAT *b){
	
	if (b->trevsorted == 1){ 
		printf("Prop of the BAT is violated\n"); 
		return 0; 
	}
	if (b->tsorted == 1){
		printf("Prop of the BAT is violated\n"); 
		return 0; 
	}
	
	return 1; 
}

/*
 * Order the property by their support
 * for each table
 * */
static
int** createColumnOrder(CStableStat* cstablestat, CSPropTypes *csPropTypes){
	int i, j, k; 
	int num = cstablestat->numTables;
	int **colOrder;
	int tblColIdx; 
	
	colOrder = (int **)GDKmalloc(sizeof(int*) * num);
	for (i = 0; i < num; i++){
		int* tmpPropFreqs; 
		tmpPropFreqs = (int *) GDKmalloc(sizeof(int) * cstablestat->numPropPerTable[i]);
		colOrder[i] = (int *) GDKmalloc(sizeof(int) * cstablestat->numPropPerTable[i]); 
		
		for (j = 0; j < cstablestat->numPropPerTable[i]; j ++){
			colOrder[i][j] = j; 
		}

		//Store the frequencies of table columns to tmp arrays
		for (j = 0; j < csPropTypes[i].numProp; j++){
			tblColIdx = csPropTypes[i].lstPropTypes[j].defColIdx;
			if (tblColIdx == -1) continue; 
			assert(tblColIdx < cstablestat->numPropPerTable[i]);
			tmpPropFreqs[tblColIdx] = csPropTypes[i].lstPropTypes[j].propFreq; 		
		}


		//Do insertion sort the the property ascending according to their support	
		for (j = 1; j < cstablestat->numPropPerTable[i]; j ++){
			int tmpPos = colOrder[i][j]; 
			int tmpFreq = tmpPropFreqs[tmpPos];
			k = j; 
			while (k > 0 && tmpFreq > tmpPropFreqs[colOrder[i][k-1]]){
				colOrder[i][k] = colOrder[i][k-1]; 					
				k--;
			}
			colOrder[i][k] = tmpPos;
		}
	}
	
	return colOrder; 	
}

static
void freeColOrder(int **colOrder, CStableStat* cstablestat){
	int i; 
	for (i = 0; i < cstablestat->numTables; i++){
		GDKfree(colOrder[i]);	
	}
	GDKfree(colOrder);
} 

/* Re-organize triple table by using clustering storage
 * CALL rdf_reorganize('schema','tablename', 1);
 * e.g., rdf_reorganize('rdf','spo0');
 *
 */
str
SQLrdfreorganize(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
#ifdef HAVE_RAPTOR
	mvc *m = NULL;
	str *schema = (str *) getArgReference(stk,pci,1);
	str *tbname = (str *) getArgReference(stk,pci,2);
	int *threshold = (int *) getArgReference(stk,pci,3);
	int *mode = (int *) getArgReference(stk,pci,4);
	sql_schema *sch; 
	int ret = 0; 
	CStableStat *cstablestat; 
	CSPropTypes     *csPropTypes;
	char	tmptbname[100]; 
	char	tmpmvtbname[100];
	char	tmptbnameex[100];
	//char	tmpviewname[100]; 
	char	tmpcolname[100]; 
	char	tmpmvcolname[100];
	//char	viewcommand[500];
	sql_subtype tpe; 	
	sql_subtype tpes[50];

	sql_table	**cstables; 
	sql_table	***csmvtables; 	//table for storing multi-values 
	#if CSTYPE_TABLE == 1
	sql_table	**cstablesEx; 
	sql_table	**viewcstables; 
	#endif
	sql_table 	*psotbl;
	#if TRIPLEBASED_TABLE	
	sql_table	*respotbl; 	/*reorganized spo table*/
	#endif
	int 	i, j, k, colId; 
	int	tmpNumMVCols = 0;
	int	nonullmvtables = 0;
	int	totalNoTablesCreated = 0;
	int	totalNoViewCreated = 0;
	int	totalNoExTables = 0; 
	int	totalNumDefCols = 0; 
	int	totalNumNonDefCols = 0; 

	str msg;
	BAT	*sbat, *pbat, *obat, *mbat; 
	BAT	*tmpbat; 
	BATiter	mapi; 
	clock_t tmpbeginT, tmpendT, beginT, endT;

	BAT *ontbat = NULL;
	int **colOrder = NULL; 

	beginT = clock();
	
	rethrow("sql.rdfShred", msg, getSQLContext(cntxt, mb, &m, NULL));

	if ((sch = mvc_bind_schema(m, *schema)) == NULL)
		throw(SQL, "sql.rdfShred", "3F000!schema missing");

	sbat = mvc_bind(m, *schema, *tbname, "subject",0);
	pbat = mvc_bind(m, *schema, *tbname, "property",0);
	obat = mvc_bind(m, *schema, *tbname, "object",0);
	mbat = mvc_bind(m, *schema, "map0", "lexical",0);

	cstablestat = (CStableStat *) malloc (sizeof (CStableStat));
	
	ontbat = mvc_bind(m, "sys", "ontlist", "mont",0);

	tmpbeginT = clock();

	if (ontbat == NULL){
		printf("[PROBLEM] There is no column ontlist/mont \n"); 
		throw(SQL, "sql.rdfreorganize", "Colunm ontlist/mont is missing");
	}
	else{
		rethrow("sql.rdfreorganize", msg, RDFreorganize(&ret, cstablestat, &csPropTypes, &sbat->batCacheid, &pbat->batCacheid, 
				&obat->batCacheid, &mbat->batCacheid, &ontbat->batCacheid, threshold, mode));

		BBPunfix(ontbat->batCacheid);
	}

	tmpendT = clock(); 
	printf ("Sql.mx: Reorganizing process process took %f seconds.\n", ((float)(tmpendT - tmpbeginT))/CLOCKS_PER_SEC);

	//if (*mode == EXPLOREONLY){
	if (*mode < BUILDTABLE){
		BBPunfix(sbat->batCacheid); 
		BBPunfix(pbat->batCacheid);
		BBPunfix(obat->batCacheid); 
		BBPunfix(mbat->batCacheid);
		freeCSPropTypes(csPropTypes,cstablestat->numTables);
		freeCStableStat(cstablestat); 
		//free(cstablestat);
		return MAL_SUCCEED; 
	}
	
	mapi = bat_iterator(mbat); 	
	
	tmpbeginT = clock();
	cstables = (sql_table **)malloc(sizeof(sql_table*) * cstablestat->numTables);
	csmvtables = (sql_table ***)malloc(sizeof(sql_table**) * cstablestat->numTables);

	#if CSTYPE_TABLE == 1
	cstablesEx = (sql_table **)malloc(sizeof(sql_table*) * cstablestat->numTables);
	viewcstables = (sql_table **)malloc(sizeof(sql_table*) * cstablestat->numTables);
	#endif

	// Put to SQL tables
	sql_find_subtype(&tpe, "oid", 31, 0);

	// Add irregular triples to pso tbale
	psotbl = mvc_create_table(m, sch, "pso", tt_table, 0,
			                                   SQL_PERSIST, 0, 3);
	totalNoTablesCreated++;
	mvc_create_column(m, psotbl, "p",  &tpe);
	mvc_create_column(m, psotbl, "s",  &tpe);
	mvc_create_column(m, psotbl, "o",  &tpe);

	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, psotbl,"p" ), 
			cstablestat->pbat, TYPE_bat);
	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, psotbl,"s" ), 
			cstablestat->sbat, TYPE_bat);
	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, psotbl,"o" ), 
			cstablestat->obat, TYPE_bat);
	// Add regular triple to cstables and cstablesEx
	
	#if TRIPLEBASED_TABLE
	printf("Create triple-based relational table ..."); 
	respotbl = mvc_create_table(m, sch, "triples", tt_table, 0,
			                                   SQL_PERSIST, 0, 3);
	totalNoTablesCreated++;
	mvc_create_column(m, respotbl, "p",  &tpe);
	mvc_create_column(m, respotbl, "s",  &tpe);
	mvc_create_column(m, respotbl, "o",  &tpe);


	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, respotbl,"p" ), 
			cstablestat->repbat, TYPE_bat);
	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, respotbl,"s" ), 
			cstablestat->resbat, TYPE_bat);
	store_funcs.append_col(m->session->tr,
			mvc_bind_column(m, respotbl,"o" ), 
			cstablestat->reobat, TYPE_bat);
	printf("Done\n");

	#endif

	printf("Starting creating SQL Table -- \n");
	
	

	sql_find_subtype(&tpes[TYPE_oid], "oid", 31 , 0);
	//printf("Tpes %d Type name is: %s \n", TYPE_oid, tpes[TYPE_oid].type->sqlname);

	sql_find_subtype(&tpes[TYPE_str], "varchar", 500 , 0);
	//printf("Tpes %d Type name is: %s \n", TYPE_str, tpes[TYPE_str].type->sqlname);

	sql_find_subtype(&tpes[TYPE_dbl], "double", 53 , 0);
	//printf("Tpes %d Type name is: %s \n", TYPE_dbl, tpes[TYPE_dbl].type->sqlname);

	sql_find_subtype(&tpes[TYPE_int], "int", 9 , 0);
	//printf("Tpes %d Type name is: %s \n", TYPE_int, tpes[TYPE_int].type->sqlname);

	sql_find_subtype(&tpes[TYPE_flt], "real", 23, 0);
	//printf("Tpes %d Type name is: %s \n", TYPE_flt, tpes[TYPE_flt].type->sqlname);
	
	sql_find_subtype(&tpes[TYPE_timestamp],"timestamp",128,0);

	/*
	sql_find_subtype(&tpe, "float", 0 , 0);
	printf("Test Type name is: %s \n", tpe.type->sqlname);
	sql_find_subtype(&tpe, "int", 9 , 0);
	printf("Test Type name is: %s \n", tpe.type->sqlname);
	sql_find_subtype(&tpe, "oid", 31 , 0);
	printf("Test Type name is: %s \n", tpe.type->sqlname);
	*/
	{
	char*   rdfschema = "rdf";
	if (TKNZRopen (NULL, &rdfschema) != MAL_SUCCEED) {
		throw(RDF, "SQLrdfreorganize","could not open the tokenizer\n");
	}
	}

	//Re-order the columns 
	colOrder = createColumnOrder(cstablestat, csPropTypes);

	for (i = 0; i < cstablestat->numTables; i++){
		//printf("creating table %d \n", i);

		getTblSQLname(tmptbname, i, 0, cstablestat->lstcstable[i].tblname, mapi, mbat);
		printf("Table %d:||  %s ||\n",i, tmptbname);

		cstables[i] = mvc_create_table(m, sch, tmptbname, tt_table, 0,
				   SQL_PERSIST, 0, 3);
		totalNoTablesCreated++;
		//Multivalues tables for each column
		csmvtables[i] = (sql_table **)malloc(sizeof(sql_table*) * cstablestat->numPropPerTable[i]);
		
		#if APPENDSUBJECTCOLUMN
		mvc_create_column(m, cstables[i], "subject",  &tpes[TYPE_oid]);
		#endif
		for (colId = 0; colId < cstablestat->numPropPerTable[i]; colId++){
			j = colOrder[i][colId]; 
		//for (j = 0; j < cstablestat->numPropPerTable[i]; j++){

			//TODO: Use propertyId from Propstat
			getColSQLname(tmpcolname, j, -1, cstablestat->lstcstable[i].lstProp[j], mapi, mbat);


			tmpbat = cstablestat->lstcstable[i].colBats[j];
			isRightPropBAT(tmpbat);

			mvc_create_column(m, cstables[i], tmpcolname,  &tpes[tmpbat->ttype]);
			
			//For multi-values table
			tmpNumMVCols = cstablestat->lstcstable[i].lstMVTables[j].numCol;
			if (tmpNumMVCols != 0){
				getMvTblSQLname(tmpmvtbname, i, j, cstablestat->lstcstable[i].tblname, cstablestat->lstcstable[i].lstProp[j], mapi, mbat);
				csmvtables[i][j] = mvc_create_table(m, sch, tmpmvtbname, tt_table, 0, SQL_PERSIST, 0, 3); 
				totalNoTablesCreated++;

				//One column for key
				sprintf(tmpcolname, "mvKey");
				tmpbat = cstablestat->lstcstable[i].lstMVTables[j].keyBat;
				isRightPropBAT(tmpbat);
				mvc_create_column(m, csmvtables[i][j], tmpcolname,  &tpes[tmpbat->ttype]);

				//One column for subj oid
				sprintf(tmpcolname, "mvsubj");
				tmpbat = cstablestat->lstcstable[i].lstMVTables[j].subjBat;
				isRightPropBAT(tmpbat);
				mvc_create_column(m, csmvtables[i][j], tmpcolname,  &tpes[tmpbat->ttype]);

				//Value columns 
				for (k = 0; k < tmpNumMVCols; k++){
					getColSQLname(tmpmvcolname, j, k, cstablestat->lstcstable[i].lstProp[j], mapi, mbat);

					tmpbat = cstablestat->lstcstable[i].lstMVTables[j].mvBats[k];
					isRightPropBAT(tmpbat);
					mvc_create_column(m, csmvtables[i][j], tmpmvcolname,  &tpes[tmpbat->ttype]);
				}

			}
			else
				nonullmvtables++;
		}
		
		totalNumDefCols += cstablestat->lstcstable[i].numCol;

		#if CSTYPE_TABLE == 1
		// Add non-default type table
		if (cstablestat->lstcstableEx[i].numCol != 0){	

			getTblSQLname(tmptbnameex, i, 1, cstablestat->lstcstable[i].tblname, mapi, mbat);
			printf("TableEx %d: || %s || \n",i, tmptbnameex);

			cstablesEx[i] = mvc_create_table(m, sch, tmptbnameex, tt_table, 0,
					   SQL_PERSIST, 0, 3);
			totalNoTablesCreated++;
			totalNoExTables++;
			for (j = 0; j < cstablestat->lstcstableEx[i].numCol; j++){
				//TODO: Use propertyId from Propstat
				int tmpcolidx = cstablestat->lstcstableEx[i].mainTblColIdx[j];
				getColSQLname(tmpcolname, tmpcolidx, (int)(cstablestat->lstcstableEx[i].colTypes[j]), 
						cstablestat->lstcstable[i].lstProp[tmpcolidx], mapi, mbat);

				tmpbat = cstablestat->lstcstableEx[i].colBats[j];
				isRightPropBAT(tmpbat);
				mvc_create_column(m, cstablesEx[i], tmpcolname,  &tpes[tmpbat->ttype]);				
			}
			totalNumNonDefCols += cstablestat->lstcstableEx[i].numCol;
		}

		#endif

		#if APPENDSUBJECTCOLUMN
		{
			BAT* subjBat = createEncodedSubjBat(i,BATcount(cstablestat->lstcstable[i].colBats[0]));
                	store_funcs.append_col(m->session->tr,
					mvc_bind_column(m, cstables[i],"subject"), 
					subjBat, TYPE_bat);
			BBPreclaim(subjBat);
		}
		#endif
		for (colId = 0; colId < cstablestat->numPropPerTable[i]; colId++){
			j = colOrder[i][colId]; 
		//for (j = 0; j < cstablestat->numPropPerTable[i]; j++){

			//TODO: Use propertyId from Propstat
			getColSQLname(tmpcolname, j, -1, cstablestat->lstcstable[i].lstProp[j], mapi, mbat);

			tmpbat = cstablestat->lstcstable[i].colBats[j];
			isRightPropBAT(tmpbat);
			//printf("Column %d: \n",j); 
			//BATprint(tmpbat);
                	store_funcs.append_col(m->session->tr,
					mvc_bind_column(m, cstables[i],tmpcolname ), 
					tmpbat, TYPE_bat);

			//For multi-values table
			tmpNumMVCols = cstablestat->lstcstable[i].lstMVTables[j].numCol;
			if (tmpNumMVCols != 0){

				//One column for key
				sprintf(tmpcolname, "mvKey");
				tmpbat = cstablestat->lstcstable[i].lstMVTables[j].keyBat;
				isRightPropBAT(tmpbat);
				store_funcs.append_col(m->session->tr,
					mvc_bind_column(m, csmvtables[i][j],tmpcolname), 
					tmpbat, TYPE_bat);

				//One column for subj Bat
				sprintf(tmpcolname, "mvsubj");
				tmpbat = cstablestat->lstcstable[i].lstMVTables[j].subjBat;
				isRightPropBAT(tmpbat);
				store_funcs.append_col(m->session->tr,
					mvc_bind_column(m, csmvtables[i][j],tmpcolname), 
					tmpbat, TYPE_bat);

				//Value columns
				for (k = 0; k < tmpNumMVCols; k++){

					getColSQLname(tmpmvcolname, j, k, cstablestat->lstcstable[i].lstProp[j], mapi, mbat);

					tmpbat = cstablestat->lstcstable[i].lstMVTables[j].mvBats[k];
					isRightPropBAT(tmpbat);
					//printf("MVColumn %d: \n",k); 
					//BATprint(tmpbat);
                			store_funcs.append_col(m->session->tr,
						mvc_bind_column(m, csmvtables[i][j],tmpmvcolname), 
						tmpbat, TYPE_bat);
				}
			}
			else
				nonullmvtables++;
		}


		#if CSTYPE_TABLE == 1
		// Add non-default type table
		if (cstablestat->lstcstableEx[i].numCol != 0){	
			for (j = 0; j < cstablestat->lstcstableEx[i].numCol; j++){
				//TODO: Use propertyId from Propstat
				int tmpcolidx = cstablestat->lstcstableEx[i].mainTblColIdx[j];
				getColSQLname(tmpcolname, tmpcolidx, (int)(cstablestat->lstcstableEx[i].colTypes[j]), 
						cstablestat->lstcstable[i].lstProp[tmpcolidx], mapi, mbat);

				tmpbat = cstablestat->lstcstableEx[i].colBats[j];
				isRightPropBAT(tmpbat);
				//printf("ColumnEx %d: \n",j); 
				//BATprint(tmpbat);
				store_funcs.append_col(m->session->tr,
						mvc_bind_column(m, cstablesEx[i],tmpcolname ), 
						tmpbat, TYPE_bat);
			}
		}

		#endif
		//Create a view to combine these tables
		/*
		sprintf(tmpviewname, "viewcstable%d",i);
		sprintf(viewcommand, "SELECT * from rdf.%s UNION SELECT * from rdf.%s;", tmptbname, tmptbnameex); 
		//printf("Create view %s \n", viewcommand);
		viewcstables[i] = mvc_create_view(m, sch, tmpviewname, SQL_PERSIST,viewcommand, 1); 
		totalNoViewCreated++;
		for (j = 0; j < cstablestat->numPropPerTable[i]; j++){
			//TODO: Use propertyId from Propstat
			sprintf(tmpcolname, "col%d",j);
			mvc_create_column(m, viewcstables[i], tmpcolname,  &tpe);
		}
		*/

		//printf("Done creating table %d with %d cols \n", i,cstablestat->numPropPerTable[i]);

	}
	printf("... Done ( %d tables (in which %d tables are non-default) + %d views created. Already exclude %d Null mvtables ) \n", 
			totalNoTablesCreated, totalNoExTables, totalNoViewCreated, nonullmvtables);
	printf("Number of default-type columns: %d \n ", totalNumDefCols);
	printf("Number of non-default-type columns: %d  (%f ex-types per prop) \n ", totalNumNonDefCols, (float)totalNumNonDefCols/totalNumDefCols);

	printf("Generating script for FK creation ...");
	addPKandFKs(cstablestat, csPropTypes, *schema, mapi, mbat);
	printf("done\n");

	TKNZRclose(&ret);

	BBPunfix(sbat->batCacheid); 
	BBPunfix(pbat->batCacheid);
	BBPunfix(obat->batCacheid); 
	BBPunfix(mbat->batCacheid);
	for (i = 0; i < cstablestat->numTables; i++){
		free(csmvtables[i]);
	}
	free(csmvtables);
	freeColOrder(colOrder, cstablestat);
	freeCSPropTypes(csPropTypes,cstablestat->numTables);
	freeCStableStat(cstablestat); 
	free(cstables);
	free(cstablesEx); 
	free(viewcstables); 

	tmpendT = clock(); 
	printf ("Sql.mx: Put Bats to Relational Table  process took %f seconds.\n", ((float)(tmpendT - tmpbeginT))/CLOCKS_PER_SEC);

	endT = clock(); 
	printf ("Sql.mx: All processes took  %f seconds.\n", ((float)(endT - beginT))/CLOCKS_PER_SEC);

	return MAL_SUCCEED; 
#else
	(void) cntxt; (void) mb; (void) stk; (void) pci;
	throw(SQL, "sql.SQLrdfreorganize", "RDF support is missing from MonetDB5");
#endif /* HAVE_RAPTOR */	
}

#if 1
str
SQLrdfidtostr(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci){
	str msg; 
	mvc *m = NULL; 
	BAT *lmapBat = NULL, *rmapBat = NULL, *mBat = NULL; 
	bat lmapBatId, rmapBatId;
	str bnamelBat = "map_to_tknz_left";
	str bnamerBat = "map_to_tknz_right";
	char *schema = "rdf";
	sql_schema *sch;
	BUN pos; 
	oid *origId; 
	ObjectType objType; 
	oid *id = (oid *)getArgReference(stk,pci,1);
	str *ret = (str *) getArgReference(stk, pci, 0); 

	rethrow("sql.rdfidtostr", msg, getSQLContext(cntxt, mb, &m, NULL));
	
	if (*id == oid_nil){
		*ret = GDKstrdup(str_nil);
		return MAL_SUCCEED; 
	}

	objType = getObjType(*id);

	if (objType == STRING){
		str tmpObjStr;
		BATiter mapi; 
		if ((sch = mvc_bind_schema(m, schema)) == NULL)
			throw(SQL, "sql.rdfShred", "3F000!schema missing");

		mBat = mvc_bind(m, schema, "map0", "lexical",0);
		mapi = bat_iterator(mBat); 

		pos = (*id) - (objType*2 + 1) *  RDF_MIN_LITERAL;   /* Get the position of the string in the map bat */
		tmpObjStr = (str) BUNtail(mapi, BUNfirst(mBat) + pos);

		*ret = GDKstrdup(tmpObjStr);

	}
	else if (objType == URI || objType == BLANKNODE){
		lmapBatId = BBPindex(bnamelBat);
		rmapBatId = BBPindex(bnamerBat);

		if (lmapBatId == 0 || rmapBatId == 0){
			throw(SQL, "sql.SQLrdfidtostr", "The lmap/rmap Bats should be built already");
		}
		
		if ((lmapBat= BATdescriptor(lmapBatId)) == NULL) {
			throw(MAL, "rdf.RDFreorganize", RUNTIME_OBJECT_MISSING);
		}

		if ((rmapBat= BATdescriptor(rmapBatId)) == NULL) {
			throw(MAL, "rdf.RDFreorganize", RUNTIME_OBJECT_MISSING);
		}

		pos = BUNfnd(lmapBat,id);
		if (pos == BUN_NONE)	//this id is not converted to a new id
			origId = id; 
		else
			origId = (oid *) Tloc(rmapBat, pos);
		
		/*First convert the id to the original tokenizer odi */
		rethrow("sql.rdfidtostr", msg, takeOid(*origId, ret));
	} else {
		//throw(SQL, "sql.SQLrdfidtostr", "This Id cannot convert to str");
		getStringFormatValueFromOid(*id, objType, ret);  
	}

	if (msg != MAL_SUCCEED){
		throw(SQL, "sql.SQLrdfidtostr", "Problem in retrieving str from oid");
	}

	return msg; 
}


str
SQLrdfidtostr_bat(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci){
	str msg; 
	mvc *m = NULL; 
	BAT *lmapBat = NULL, *rmapBat = NULL, *mBat = NULL; 
	bat lmapBatId, rmapBatId;
	str bnamelBat = "map_to_tknz_left";
	str bnamerBat = "map_to_tknz_right";
	char *schema = "rdf";
	sql_schema *sch;
	BUN pos; 
	oid *origId; 
	ObjectType objType; 
	str tmpObjStr;
	BATiter mapi; 
	BAT *srcBat = NULL, *desBat = NULL; 
	BATiter srci; 
	BUN p, q; 
	bat *srcbid, *desbid; 
	oid *id; 
	str s; 
	srcbid = (bat *)getArgReference(stk,pci,1);
	desbid = (bat *) getArgReference(stk, pci, 0); 

	rethrow("sql.rdfidtostr", msg, getSQLContext(cntxt, mb, &m, NULL));
	
	if ((srcBat = BATdescriptor(*srcbid)) == NULL){
		throw(MAL, "rdf.RDFreorganize", RUNTIME_OBJECT_MISSING);
	}
	srci = bat_iterator(srcBat); 
	
	desBat = BATnew(TYPE_void, TYPE_str, BATcount(srcBat) + 1, TRANSIENT);
	BATseqbase(desBat, 0);
	
	/* Init the BATs for looking up the URIs*/
	lmapBatId = BBPindex(bnamelBat);
	rmapBatId = BBPindex(bnamerBat);

	if (lmapBatId == 0 || rmapBatId == 0){
		throw(SQL, "sqlbat.SQLrdfidtostr_bat", "The lmap/rmap Bats should be built already");
	}
	
	if ((lmapBat= BATdescriptor(lmapBatId)) == NULL) {
		throw(MAL, "sqlbat.SQLrdfidtostr_bat", RUNTIME_OBJECT_MISSING);
	}

	if ((rmapBat= BATdescriptor(rmapBatId)) == NULL) {
		throw(MAL, "sqlbat.SQLrdfidtostr_bat", RUNTIME_OBJECT_MISSING);
	}

	/* Init the map BAT for looking up the literal values*/
	if ((sch = mvc_bind_schema(m, schema)) == NULL)
		throw(SQL, "sql.rdfShred", "3F000!schema missing");

	mBat = mvc_bind(m, schema, "map0", "lexical",0);
	mapi = bat_iterator(mBat); 


	BATloop(srcBat, p, q){
		id = (oid *)BUNtloc(srci, p);
		if (*id == oid_nil){
			desBat = BUNappend(desBat, str_nil, TRUE);
			continue; 
		}
		objType = getObjType(*id);

		if (objType == STRING){

			pos = (*id) - (objType*2 + 1) *  RDF_MIN_LITERAL;   /* Get the position of the string in the map bat */
			tmpObjStr = (str) BUNtail(mapi, BUNfirst(mBat) + pos);

			s = GDKstrdup(tmpObjStr);
		}
		else if (objType == URI || objType == BLANKNODE){

			pos = BUNfnd(lmapBat,id);
			if (pos == BUN_NONE)	//this id is not converted to a new id
				origId = id; 
			else
				origId = (oid *) Tloc(rmapBat, pos);
			
			/*First convert the id to the original tokenizer odi */
			rethrow("sql.rdfidtostr", msg, takeOid(*origId, &s));
		} else {
			//throw(SQL, "sql.SQLrdfidtostr", "This Id cannot convert to str");
			getStringFormatValueFromOid(*id, objType, &s);  
		}


		if (msg != MAL_SUCCEED){
			throw(SQL, "sql.SQLrdfidtostr", "Problem in retrieving str from oid");
		}

		//Append to desBAT
		desBat = BUNappend(desBat, s, TRUE);

	}
	
	*desbid = desBat->batCacheid;
	BBPkeepref(*desbid);
	
	BBPunfix(lmapBat->batCacheid);
	BBPunfix(rmapBat->batCacheid);
	BBPunfix(mBat->batCacheid);

	return msg; 
}

#else

str
SQLrdfidtostr(str *ret, oid *id){
	str msg; 
	BAT *lmapBat = NULL, *rmapBat = NULL; 
	bat lmapBatId, rmapBatId;
	str bnamelBat = "map_to_tknz_left";
	str bnamerBat = "map_to_tknz_right";
	BUN pos; 
	oid *origId; 

	lmapBatId = BBPindex(bnamelBat);
	rmapBatId = BBPindex(bnamerBat);

	if (lmapBatId == 0 || rmapBatId == 0){
		throw(SQL, "sql.SQLrdfidtostr", "The lmap/rmap Bats should be built already");
	}
	
	if ((lmapBat= BATdescriptor(lmapBatId)) == NULL) {
		throw(MAL, "rdf.RDFreorganize", RUNTIME_OBJECT_MISSING);
	}

	if ((rmapBat= BATdescriptor(rmapBatId)) == NULL) {
		throw(MAL, "rdf.RDFreorganize", RUNTIME_OBJECT_MISSING);
	}

	pos = BUNfnd(lmapBat,id);
	if (pos == BUN_NONE)	//this id is not converted to a new id
		origId = id; 
	else
		origId = (oid *) Tloc(rmapBat, pos);
	
	rethrow("SQLrdfidtostr", msg, takeOid(*origId, ret));

	//printf("String for "BUNFMT" is: %s \n",*id, *ret); 

	return msg; 
}
#endif

#if 0
str
SQLrdfstrtoid(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci){
	str msg; 
	mvc *m = NULL; 
	BAT *mapBat = NULL; 
	bat mapBatId;
	str bnameBat = "tknzr_to_map";
	oid *origId = NULL; 
	oid *id = NULL; 
	oid *ret = getArgReference_oid(stk, pci, 0);
	str *s = (str *)getArgReference(stk,pci,1);

	rethrow("sql.rdfstrtoid", msg, getSQLContext(cntxt, mb, &m, NULL));

	mapBatId = BBPindex(bnameBat);

	if ((mapBat= BATdescriptor(mapBatId)) == NULL) {
		throw(MAL, "SQLrdfstrtoid", RUNTIME_OBJECT_MISSING);
	}
	
	VALset(getArgReference(stk, pci, 1), TYPE_str, *s);

	rethrow("sql.rdfstrtoid", msg, TKNZRlocate(cntxt,mb,stk,pci));

	if (msg != MAL_SUCCEED){
		throw(SQL, "SQLrdfstrtoid", "Problem in locating string: %s\n", msg);
	}

	origId = (oid *) getArgReference(stk, pci, 0);
	
	if (*origId == oid_nil){
		throw(SQL, "SQLrdfstrtoid","String %s is not stored", *s);
	}


	id = (oid *) Tloc(mapBat, *origId); 

	if (id != NULL){
		*ret = *id; 
	}else{
		*ret = BUN_NONE; 
		throw(SQL, "SQLrdfstrtoid","No Id found for string %s", *s);
	}

	
	return msg; 
}

#else

str
SQLrdfstrtoid(oid *ret, str *s){
	str msg; 
	BAT *mapBat = NULL; 
	bat mapBatId;
	str bnameBat = "tknzr_to_map";
	oid origId; 
	oid *id; 

	//printf("Get the encoded id for the string %s\n", *s); 

	mapBatId = BBPindex(bnameBat);

	if ((mapBat= BATdescriptor(mapBatId)) == NULL) {
		throw(MAL, "SQLrdfstrtoid", RUNTIME_OBJECT_MISSING);
	}
	
	rethrow("sql.rdfstrtoid", msg, TKNRstringToOid(&origId, s));

	if (msg != MAL_SUCCEED){
		throw(SQL, "SQLrdfstrtoid", "Problem in locating string: %s\n", msg);
	}

	if (origId == oid_nil){
		throw(SQL, "SQLrdfstrtoid","String %s is not stored", *s); 
	}

	id = (oid *) Tloc(mapBat, origId); 

	if (id == NULL){
		*ret = BUN_NONE; 
		throw(SQL, "SQLrdfstrtoid","No Id found for string %s", *s); 
	}
	else
		*ret = *id; 
	
	return msg; 
}

#endif 

str 
SQLrdfScan(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci){
	str msg; 
	int ret; 
	mvc *m = NULL; 

	str *params = (str *)getArgReference(stk,pci,1);	
	str *schema = (str *)getArgReference(stk,pci,2);	

	rethrow("sql.rdfScan", msg, getSQLContext(cntxt, mb, &m, NULL));

	rethrow("sql.rdfScan", msg, RDFscan(*params, *schema));

	(void) ret; 

	return MAL_SUCCEED; 
}

str
SQLrdfRetrieveSubschema(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
#ifdef HAVE_RAPTOR
	sql_schema	*schOld, *schNew;
	str		*oldSchema = (str *) getArgReference(stk,pci,1); // schema name (old)
	int		*count = (int *) getArgReference(stk,pci,2); // number of tables in the sub schema
	str		*keyword = (str *) getArgReference(stk,pci,3); // keyword to find the root table
	mvc		*m = NULL;
	str		msg;
	int		i;

	BAT		*table_id_freq_id, *table_id_freq_name, *table_id_freq_freq;
	BAT		*adjacency_list_from, *adjacency_list_to, *adjacency_list_freq;
	BATiter		table_id_freq_name_i;
	BUN		p, q;

	BUN		tableCount = 0;
	int		*table_id = NULL;
	str		*table_name = NULL;
	int		*table_freq = NULL;
	int		*adjacency_from = NULL;
	int		*adjacency_to = NULL;
	int		*adjacency_freq = NULL;
	BUN		adjacencyCount = 0;

	int		root;
	int		countActual;
	int		*nodes = NULL;
	str		name; // new schema name

	// init
	rethrow("sql.rdfRetrieveSubschema", msg, getSQLContext(cntxt, mb, &m, NULL));

	if ((schOld = mvc_bind_schema(m, *oldSchema)) == NULL)
		throw(SQL, "sql.rdfRetrieveSubschema", "3F000!schema missing");

	table_id_freq_id = mvc_bind(m, "sys", "table_id_freq", "id", 0);
	table_id_freq_name = mvc_bind(m, "sys", "table_id_freq", "name", 0);
	table_id_freq_freq = mvc_bind(m, "sys", "table_id_freq", "frequency", 0);
	adjacency_list_from = mvc_bind(m, "sys", "adjacency_list", "from_id", 0);
	adjacency_list_to = mvc_bind(m, "sys", "adjacency_list", "to_id", 0);
	adjacency_list_freq = mvc_bind(m, "sys", "adjacency_list", "frequency", 0);

	tableCount = BATcount(table_id_freq_id);
	table_name = GDKmalloc(sizeof(str) * tableCount);

	if (!table_name) {
		BBPreclaim(table_id_freq_id);
		BBPreclaim(table_id_freq_name);
		BBPreclaim(table_id_freq_freq);
		BBPreclaim(adjacency_list_from);
		BBPreclaim(adjacency_list_to);
		BBPreclaim(adjacency_list_freq);
		throw(SQL, "sql.rdfRetrieveSubschema", "ERROR: Couldn't GDKmalloc table_name array!\n");
	}

	table_id = (int*) Tloc(table_id_freq_id, BUNfirst(table_id_freq_id));
	table_freq = (int*) Tloc(table_id_freq_freq, BUNfirst(table_id_freq_freq));
	table_id_freq_name_i = bat_iterator(table_id_freq_name);

	tableCount = 0;
	BATloop(table_id_freq_name, p, q) {
		table_name[tableCount++] = (str) BUNtail(table_id_freq_name_i, p);
	}

	adjacencyCount = BATcount(adjacency_list_from);

	adjacency_from = (int*) Tloc(adjacency_list_from, BUNfirst(adjacency_list_from));
	adjacency_to = (int*) Tloc(adjacency_list_to, BUNfirst(adjacency_list_to));
	adjacency_freq = (int*) Tloc(adjacency_list_freq, BUNfirst(adjacency_list_freq));

	// TODO find root node using the keyword
	root = 0;
	(void) keyword;

	// call retrieval function
	countActual = 0;
	nodes = retrieval(root, *count, &countActual, table_id, table_name, table_freq, tableCount, adjacency_from, adjacency_to, adjacency_freq, adjacencyCount);

	// create schema
	name = "s123"; // TODO create schema name
	schNew = mvc_create_schema(m, name, schOld->auth_id, schOld->owner);

	for (i = 0; i < countActual; ++i) {
		sql_table *t;
		char query[500];
		sql_table *view;
		node *n;

		// get chosen table
		t = mvc_bind_table(m, schOld, table_name[nodes[i]]);
		assert(t != NULL); // else: inconsistency in my data!

		// create view
		sprintf(query, "select * from %s.%s;", *oldSchema, table_name[nodes[i]]);
		view = mvc_create_view(m, schNew, table_name[nodes[i]], SQL_PERSIST, query, 0);

		// loop through columns and copy them
		for (n = t->columns.set->h; n; n = n->next) {
			sql_column *c = n->data;

			mvc_copy_column(m, view, c);
		}
	}

	BBPreclaim(table_id_freq_id);
	BBPreclaim(table_id_freq_name);
	BBPreclaim(table_id_freq_freq);
	BBPreclaim(adjacency_list_from);
	BBPreclaim(adjacency_list_to);
	BBPreclaim(adjacency_list_freq);

	GDKfree(table_id);
	GDKfree(table_name);
	GDKfree(table_freq);
	GDKfree(adjacency_from);
	GDKfree(adjacency_to);
	GDKfree(adjacency_freq);

	return MAL_SUCCEED;
#else
	(void) cntxt; (void) mb; (void) stk; (void) pci;
	throw(SQL, "sql.rdfRetrieveSubschema", "RDF support is missing from MonetDB5");
#endif /* HAVE_RAPTOR */
}

static
void test_intersection(void){

	oid** listoid; 
	int* listcount; 
	oid* interlist; 
	int num = 4; 
	int internum = 0; 
	int i;
	oid list1[6] = {1, 3, 4, 5, 7, 11}; 
	oid list2[7] = {1, 3, 5, 7, 8, 9, 11};
	oid list3[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9 ,11};
	oid list4[5] = {2, 3, 7, 9, 11}; 

	listoid = (oid**)malloc(sizeof(oid*) * num); 
	listcount = (int *) malloc(sizeof(int) * num); 
	
	listoid[0] = list1; 
	listcount[0] = 6; 
	listoid[1] = list2;
	listcount[1] = 7;
	listoid[2] = list3; 
	listcount[2] = 10; 
	listoid[3] = list4;
	listcount[3] = 5;

	intersect_oidsets(listoid, listcount, num, &interlist, &internum);
	printf("Intersection list: \n"); 
	for (i = 0; i < internum; i++){
		printf(" " BUNFMT, interlist[i]);
	}
	printf("\n"); 
}

SimpleCSset 	*global_csset = NULL; 
PropStat 	*global_p_propstat = NULL; 
PropStat 	*global_c_propstat = NULL; 
BAT		*global_mbat = NULL;
BATiter 	global_mapi; 

str SQLrdfdeserialize(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci){
	
	SimpleCSset *csset; 

	printf("De-serialize simple CSset from Bats\n"); 

	csset = dumpBat_to_CSset(); 
		
	print_simpleCSset(csset); 

	test_intersection(); 

	free_simpleCSset(csset); 
	
	(void) cntxt; (void) mb; (void) stk; (void) pci;

	return MAL_SUCCEED; 
}

/*
 * Preparation for executing sparql queries
 * */

str SQLrdfprepare(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci){
	char *schema = "rdf"; 
	char *sschema = "sys";
	str msg; 
	mvc *m = NULL;
	int ret; 

	(void) ret;
	rethrow("sql.rdfShred", msg, getSQLContext(cntxt, mb, &m, NULL));
	
	//Load map bat
	printf("Load dictionary Bat\n");
	global_mbat = mvc_bind(m, schema, "map0", "lexical",0);
	global_mapi = bat_iterator(global_mbat);

	
	//Load ontologies
	
	printf("Load ontologies\n"); 
	{	
     		BAT *auri = mvc_bind(m, sschema, "ontattributes","muri",0);
     		BAT *aattr = mvc_bind(m, sschema, "ontattributes","mattr",0);
     		BAT *muri = mvc_bind(m, sschema, "ontmetadata","muri",0);
     		BAT *msuper = mvc_bind(m, sschema, "ontmetadata","msubclassof",0);
     		BAT *mlabel = mvc_bind(m, sschema, "ontmetadata","mlabel",0);

		RDFloadsqlontologies(&ret, &(auri->batCacheid), 
				&(aattr->batCacheid),
				&(muri->batCacheid),
				&(msuper->batCacheid),
				&(mlabel->batCacheid));

		BBPreclaim(auri);
		BBPreclaim(aattr);
		BBPreclaim(muri);
		BBPreclaim(msuper);
		BBPreclaim(mlabel); 

	}

	//Open Tokenizer
	printf("Open tokenizer with schema %s\n", schema); 
	if (TKNZRopen (NULL, &schema) != MAL_SUCCEED) {
		throw(RDF, "rdf.rdfschema",
		"could not open the tokenizer\n");
	}
	
	printf("Build global csset\n");
	//Build global cs set from persistent BATs
	global_csset = dumpBat_to_CSset(); 

	//Build propstat for props in final CSs
	global_p_propstat = getPropStat_P_simpleCSset(global_csset); 

	global_c_propstat = getPropStat_C_simpleCSset(global_csset); 

	print_simpleCSset(global_csset);

	printf("Done preparation\n"); 

	(void) cntxt; (void) mb; (void) stk; (void) pci;
			
	return MAL_SUCCEED;
}

