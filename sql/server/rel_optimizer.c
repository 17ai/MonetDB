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


/*#define DEBUG*/

#include "monetdb_config.h"
#include "rel_optimizer.h"
#include "rel_exp.h"
#include "rel_prop.h"
#include "rel_dump.h"
#include "rel_select.h"
#include "rel_updates.h"
#include "rel_planner.h"
#include "sql_env.h"

#include "mtime.h"
#include "mal_client.h"

#define new_func_list(sa) sa_list(sa)
#define new_col_list(sa) sa_list(sa)


typedef struct table_pkeys
{
	str table_name;
	struct list *pkey_column_names;
} table_pkeys;

typedef struct sel_predicate
{
	sht cmp_type; // cmp_gt = 0, cmp_gte = 1, cmp_lte = 2, cmp_lt = 3, cmp_equal = 4, cmp_notequal = 5, cmp_filter = 6, cmp_or = 7, cmp_in = 8, cmp_notin = 9, cmp_all = 10, cmp_project = 11  (up to here taken from sql/rel.txt), cmp_range = 12, 13, 14, 15 (both not equal, first not equal second equal, first equal second not equal, both equal, respectively).
	sql_column* column; // column of the selection predicate
	ValRecord** values; // array of values (valrecord pointers) compared. Could be of different cardinality. E.g. 1 if cmp_equal, 2 if cmp_range, anything if cmp_in, etc.
	int num_values; // length of the values array.
} sel_predicate;


typedef struct global_props {
	int cnt[MAXOPS];
} global_props;

typedef sql_rel *(*rewrite_fptr)(int *changes, mvc *sql, sql_rel *rel);
typedef int (*find_prop_fptr)(mvc *sql, sql_rel *rel);

static sql_subfunc *find_func( mvc *sql, char *name, list *exps );
int is_table_in_list_table_pkeys(list *l, sql_table* st);
int is_column_in_list_columns(list *l, sql_column* c);
int is_column_of_table_in_list_table_pkeys(list *l, sql_table* st, sql_column* c);
list* extract_column_names_from_list_of_columns(list* list_of_columns);
list* collect_PERPAD(mvc *sql, sql_rel *rel);
lng get_enum_step_length(sql_column* c);
sel_predicate** convert_all_into_in_clause_except_cmp_equal(list *list_of_PERPAD);
int enumerate_pkey_space(str** ret, sel_predicate** sps, int sps_enum_start, int num_PERPAD, int* is_pkey_to_be_enumerated);
int* enumerate_and_insert_into_temp_table(mvc *sql, sel_predicate** sps, int num_PERPAD);
str SQLstatementIntern(Client c, str *expr, str nme, int execute, bit output);
str VAL2str(ValRecord* valp);
void find_out_pkey_space_for_unavailable_required_derived_metadata(mvc* sql, list* list_of_PERPAD, int* is_pkey_to_be_enumerated, int num_pkeys_to_be_enumerated);
void compute_and_insert_unavailable_required_derived_metadata(mvc* sql, sel_predicate** sps, int num_PERPAD, int* is_pkey_to_be_enumerated, int num_pkeys_to_be_enumerated);
str* get_pkey_bound_to_dataview(str schema_name, str dmdt_name);
str form_pkey_select_str(sel_predicate** sps, int num_PERPAD, str* pkey_bound_to_dataview, str* select_str_per_pkey);
str get_non_pkey_select_str(str schema_name, str dmdt_name);

list *discovered_table_pkeys;

/* The important task of the relational optimizer is to optimize the
   join order. 

   The current implementation chooses the join order based on 
   select counts, ie if one of the join sides has been reduced using
   a select this join is choosen over one without such selections. 
 */

/* currently we only find simple column expressions */
static void *
name_find_column( sql_rel *rel, char *rname, char *name, int pnr, sql_rel **bt ) 
{
	sql_exp *alias = NULL;
	sql_column *c = NULL;

	switch (rel->op) {
	case op_basetable: {
		node *cn;
		sql_table *t = rel->l;

		if (rel->exps) {
			sql_exp *e;
		       
			if (rname)
				e = exps_bind_column2(rel->exps, rname, name);
			else
				e = exps_bind_column(rel->exps, name, NULL);
			if (!e || e->type != e_column) 
				return NULL;
			if (e->l)
				rname = e->l;
			name = e->r;
		}
		if (rname && strcmp(t->base.name, rname) != 0)
			return NULL;
		for (cn = t->columns.set->h; cn; cn = cn->next) {
			sql_column *c = cn->data;
			if (strcmp(c->base.name, name) == 0) {
				*bt = rel;
				if (pnr < 0 || (c->t->p &&
				    list_position(c->t->p->tables.set, c->t) == pnr))
					return c;
			}
		}
		for (cn = t->idxs.set->h; cn; cn = cn->next) {
			sql_idx *c = cn->data;
			if (strcmp(c->base.name, name+1 /* skip % */) == 0) {
				*bt = rel;
				if (pnr < 0 || (c->t->p &&
				    list_position(c->t->p->tables.set, c->t) == pnr))
					return c;
			}
		}
		break;
	}
	case op_table:
		/* table func */
		return NULL;
	case op_ddl: 
		if (is_updateble(rel))
			return name_find_column( rel->l, rname, name, pnr, bt);
		return NULL;
	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 
	case op_semi: 
	case op_anti: 
		/* first right (possible subquery) */
		c = name_find_column( rel->r, rname, name, pnr, bt);
		if (!c) 
			c = name_find_column( rel->l, rname, name, pnr, bt);
		return c;
	case op_select:
	case op_topn:
	case op_sample:
		return name_find_column( rel->l, rname, name, pnr, bt);
	case op_union:
	case op_inter:
	case op_except:

		if (pnr >= 0 || pnr == -2) {
			/* first right (possible subquery) */
			c = name_find_column( rel->r, rname, name, pnr, bt);
			if (!c) 
				c = name_find_column( rel->l, rname, name, pnr, bt);
			return c;
		}
		return NULL;

	case op_project:
	case op_groupby:
		if (!rel->exps)
			break;
		if (rname)
			alias = exps_bind_column2(rel->exps, rname, name);
		else
			alias = exps_bind_column(rel->exps, name, NULL);
		if (is_groupby(rel->op) && alias && alias->type == e_column && rel->r) {
			if (alias->l)
				alias = exps_bind_column2(rel->r, alias->l, alias->r);
			else
				alias = exps_bind_column(rel->r, alias->r, NULL);
		}
		if (is_groupby(rel->op) && !alias && rel->l) {
			/* Group by column not found as alias in projection 
			 * list, fall back to check plain input columns */
			return name_find_column( rel->l, rname, name, pnr, bt);
		}
		break;
	case op_insert:
	case op_update:
	case op_delete:
		break;
	}
	if (alias) { /* we found an expression with the correct name, but
			we need sql_columns */
		if (rel->l && alias->type == e_column) /* real alias */
			return name_find_column(rel->l, alias->l, alias->r, pnr, bt);
	}
	return NULL;
}

static sql_column *
exp_find_column( sql_rel *rel, sql_exp *exp, int pnr )
{
	if (exp->type == e_column) { 
		sql_rel *bt = NULL;
		return name_find_column(rel, exp->l, exp->r, pnr, &bt);
	}
	return NULL;
}

static sql_column *
exp_find_column_( sql_rel *rel, sql_exp *exp, int pnr, sql_rel **bt )
{
	if (exp->type == e_column) 
		return name_find_column(rel, exp->l, exp->r, pnr, bt);
	return NULL;
}

/* find column for the select/join expression */
static sql_column *
sjexp_col(sql_exp *e, sql_rel *r) 
{
	sql_column *res = NULL;

	if (e->type == e_cmp && !is_complex_exp(e->flag)) {
		res = exp_find_column(r, e->l, -2);
		if (!res)
			res = exp_find_column(r, e->r, -2);
	}
	return res;
}

static sql_exp *
list_find_exp( list *exps, sql_exp *e)
{
	sql_exp *ne = NULL;

	assert(e->type == e_column);
	if ((e->l && (ne=exps_bind_column2(exps, e->l, e->r)) != NULL) ||
	    ((ne=exps_bind_column(exps, e->r, NULL)) != NULL))
		return ne;
	return NULL;
}

static int
kc_column_cmp(sql_kc *kc, sql_column *c)
{
	/* return on equality */
	return !(c == kc->c);
}

static int
join_properties(mvc *sql, sql_rel *rel) 
{
	if (rel->exps) {
		list *join_cols = new_col_list(sql->sa);
		node *en;

		/* simply using the expressions should also work ! */
		for ( en = rel->exps->h; en; en = en->next ) {
			sql_exp *e = en->data;

			if (e->type == e_cmp && e->flag == cmp_equal) {
				sql_column *lc = exp_find_column(rel, e->l, -2);
				sql_column *rc = exp_find_column(rel, e->r, -2);

				if (lc && rc) {
					append(join_cols, lc);
					append(join_cols, rc);
				}
			}
		}
	}
	return 0;
}

static void
rel_properties(mvc *sql, global_props *gp, sql_rel *rel) 
{
	gp->cnt[(int)rel->op]++;
	switch (rel->op) {
	case op_basetable:
	case op_table:
		break;
	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 

	case op_semi: 
	case op_anti: 

	case op_union: 
	case op_inter: 
	case op_except: 
		rel_properties(sql, gp, rel->l);
		rel_properties(sql, gp, rel->r);
		break;
	case op_project:
	case op_select:
	case op_groupby:
	case op_topn:
	case op_sample:
	case op_ddl:
		if (rel->l)
			rel_properties(sql, gp, rel->l);
		break;
	case op_insert:
	case op_update:
	case op_delete:
		if (rel->r) 
			rel_properties(sql, gp, rel->r);
		break;
	}

	switch (rel->op) {
	case op_basetable:
	case op_table:
		if (!find_prop(rel->p, PROP_COUNT))
			rel->p = prop_create(sql->sa, PROP_COUNT, rel->p);
		break;
	case op_join: 
		join_properties(sql, rel);
		break;
	case op_left: 
	case op_right: 
	case op_full: 

	case op_semi: 
	case op_anti: 

	case op_union: 
	case op_inter: 
	case op_except: 
		break;

	case op_project:
	case op_groupby:
	case op_topn:
	case op_sample:
	case op_select:
		break;

	case op_insert:
	case op_update:
	case op_delete:
	case op_ddl:
		break;
	}
}

static void
get_relations(sql_rel *rel, list *rels)
{
	if (!rel_is_ref(rel) && rel->op == op_join && rel->exps == NULL) {
		sql_rel *l = rel->l;
		sql_rel *r = rel->r;
		
		get_relations(l, rels);
		get_relations(r, rels);
		rel->l = NULL;
		rel->r = NULL;
		rel_destroy(rel);
	} else {
		append(rels, rel);
	}
}

static int
exp_count(int *cnt, int seqnr, sql_exp *e) 
{
	(void)seqnr;
	if (!e)
		return 0;
	if (find_prop(e->p, PROP_JOINIDX))
		*cnt += 100;
	if (find_prop(e->p, PROP_HASHCOL)) 
		*cnt += 100;
	if (find_prop(e->p, PROP_HASHIDX)) 
		*cnt += 100;
	switch(e->type) {
	case e_cmp:
		if (!is_complex_exp(e->flag)) {
			exp_count(cnt, seqnr, e->l); 
			exp_count(cnt, seqnr, e->r);
			if (e->f)
				exp_count(cnt, seqnr, e->f);
		}	
		switch (get_cmp(e)) {
		case cmp_equal:
			*cnt += 90;
			return 90;
		case cmp_notequal:
			*cnt += 7;
			return 7;
		case cmp_gt:
		case cmp_gte:
		case cmp_lt:
		case cmp_lte:
			*cnt += 6;
			if (e->f){ /* range */
				*cnt += 6;
				return 12;
			}
			return 6;
		case cmp_filter:
			*cnt += 2;
			return 2;
		case cmp_in: 
		case cmp_notin: {
			list *l = e->r;
			int c = 9 - 10*list_length(l);
			*cnt += c;
			return c;
		}
		case cmp_or: /* prefer or over functions */
			*cnt += 3;
			return 3;
		default:
			return 0;
		}
	case e_column: 
		*cnt += 20;
		return 20;
	case e_atom:
		*cnt += 10;
		return 10;
	case e_func:
		/* functions are more expensive, depending on the number of columns involved. */ 
		if (e->card == CARD_ATOM)
			return 0;
		*cnt -= 5*list_length(e->l);
		return 5*list_length(e->l);
	case e_convert:
		/* functions are more expensive, depending on the number of columns involved. */ 
		if (e->card == CARD_ATOM)
			return 0;
	default:
		*cnt -= 5;
		return -5;
	}
}

static int
exp_keyvalue(sql_exp *e) 
{
	int cnt = 0;
	exp_count(&cnt, 0, e);
	return cnt;
}

static sql_rel *
find_rel(list *rels, sql_exp *e)
{
	node *n = list_find(rels, e, (fcmp)&rel_has_exp);
	if (n) 
		return n->data;
	return NULL;
}

static int
joinexp_cmp(list *rels, sql_exp *h, sql_exp *key)
{
	sql_rel *h_l;
	sql_rel *h_r;
	sql_rel *key_l;
	sql_rel *key_r;

	assert (!h || !key || (h->type == e_cmp && key->type == e_cmp));
	if (is_complex_exp(h->flag) || is_complex_exp(key->flag))
		return -1;
	h_l = find_rel(rels, h->l);
	h_r = find_rel(rels, h->r);
	key_l = find_rel(rels, key->l);
	key_r  = find_rel(rels, key->r);

	if (h_l == key_l && h_r == key_r)
		return 0;
	if (h_r == key_l && h_l == key_r)
		return 0;
        return -1;
}

static sql_exp *
joinexp_col(sql_exp *e, sql_rel *r)
{
	if (e->type == e_cmp) {
		if (rel_has_exp(r, e->l) >= 0) 
			return e->l;
		return e->r;
	}
	assert(0);
	return NULL;
}

static sql_column *
table_colexp(sql_exp *e, sql_rel *r)
{
	sql_table *t = r->l;

	if (e->type == e_column) {
		char *name = e->name;
		node *cn;

		if (r->exps) { /* use alias */
			for (cn = r->exps->h; cn; cn = cn->next) {
				sql_exp *ce = cn->data;
				if (strcmp(ce->name, name) == 0) {
					name = ce->r;
					break;
				}
			}
		}
		for (cn = t->columns.set->h; cn; cn = cn->next) {
			sql_column *c = cn->data;
			if (strcmp(c->base.name, name) == 0) 
				return c;
		}
	}
	return NULL;
}

int
exp_joins_rels(sql_exp *e, list *rels)
{
	sql_rel *l = NULL, *r = NULL;

	assert (e->type == e_cmp);
		
	if (e->flag == cmp_or) {
		l = NULL;
	} else if (e->flag == cmp_in || e->flag == cmp_notin || get_cmp(e) == cmp_filter) {
		list *lr = e->r;

		l = find_rel(rels, e->l);
		if (lr && lr->h)
			r = find_rel(rels, lr->h->data);
	} else {
		l = find_rel(rels, e->l);
		r = find_rel(rels, e->r);
	}

	if (l && r)
		return 0;
	return -1;
}

static list *
matching_joins(sql_allocator *sa, list *rels, list *exps, sql_exp *je) 
{
	sql_rel *l, *r;

	assert (je->type == e_cmp);
		
	l = find_rel(rels, je->l);
	r = find_rel(rels, je->r);
	if (l && r) {
		list *res;
		list *n_rels = new_rel_list(sa);	

		append(n_rels, l);
		append(n_rels, r);
		res = list_select(exps, n_rels, (fcmp) &exp_joins_rels, (fdup)NULL);
		return res; 
	}
	return new_rel_list(sa);
}

static int
sql_column_kc_cmp(sql_column *c, sql_kc *kc)
{
	/* return on equality */
	return (c->colnr - kc->c->colnr);
}

static sql_idx *
find_fk_index(sql_table *l, list *lcols, sql_table *r, list *rcols)
{
	if (l->idxs.set) {
		node *in;
	   	for(in = l->idxs.set->h; in; in = in->next){
	    		sql_idx *li = in->data;
			if (li->type == join_idx) {
		        	sql_key *rk = &((sql_fkey*)li->key)->rkey->k;
				fcmp cmp = (fcmp)&sql_column_kc_cmp;

              			if (rk->t == r && 
				    list_match(lcols, li->columns, cmp) == 0 &&
				    list_match(rcols, rk->columns, cmp) == 0) {
					return li;
				}
			}
		}
	}
	return NULL;
}

static sql_rel *
find_basetable( sql_rel *r)
{
	if (!r)
		return NULL;
	switch(r->op) {
	case op_basetable:	
		return r;
	case op_project:
	case op_select:
		return find_basetable(r->l);
	default:
		return NULL;
	}
}

int is_table_in_list_table_pkeys(list *l, sql_table* st)
{
	node *n = NULL;
	
	if (l && st) {
		for (n = l->h; n; n = n->next) 
		{
			table_pkeys *tp = n->data;
			if (strcmp(tp->table_name, st->base.name) == 0)
				return TRUE;
		}
	}
	return FALSE;
}

int is_column_in_list_columns(list *l, sql_column* c)
{
	node *n = NULL;
	
	if (l && c) {
		for (n = l->h; n; n = n->next) 
		{
			str s = n->data;
			if (strcmp(s, c->base.name) == 0)
				return TRUE;
		}
	}
	return FALSE;
}

int is_column_of_table_in_list_table_pkeys(list *l, sql_table* st, sql_column* c)
{
	node *n = NULL;
	
	if (l && st && c) {
		for (n = l->h; n; n = n->next) 
		{
			table_pkeys *tp = n->data;
			if (strcmp(tp->table_name, st->base.name) == 0)
			{
				return is_column_in_list_columns(tp->pkey_column_names, c);
			}
		}
	}
	return FALSE;
}


list* extract_column_names_from_list_of_columns(list* list_of_sql_kc)
{
	node *n = NULL;
	list* list_of_str = list_create(NULL);
	
	if (list_of_sql_kc) {
		for (n = list_of_sql_kc->h; n; n = n->next) 
		{
			sql_kc* skc = n->data;
			list_append(list_of_str, GDKstrdup(skc->c->base.name));
		}
	}
	return list_of_str;
}


list* collect_PERPAD(mvc *sql, sql_rel *rel)
{
	node *n = NULL;
	int i;
	list *res = NULL;
	list *first = NULL, *second = NULL;
	
	if(rel == NULL)
		return NULL;
	
	printf("=====enter: collect_PERPAD\n");
	
	switch (rel->op) 
	{
		case op_basetable:
		case op_table:
			return NULL;
		case op_join: 
		case op_left: 
		case op_right: 
		case op_full: 
			
		case op_semi: 
		case op_anti: 
			
		case op_union: 
		case op_inter: 
		case op_except: 
			first = collect_PERPAD(sql, rel->l);
			second = collect_PERPAD(sql, rel->r);
			if(first == NULL && second == NULL)
				return NULL;
			else if(first == NULL)
				return list_merge(second, first, (fdup)NULL);
			else 
				return list_merge(first, second, (fdup)NULL);
		case op_project:
			return collect_PERPAD(sql, rel->l);
		case op_select:
			printf("op_select!\n");
			if(rel->exps == NULL)
				break;
			
			if(rel->exps->h == NULL)
				break;
			
			
			res = list_create(NULL);
			
			for (n = rel->exps->h, i = 0; n; n = n->next, i++) 
			{
				sql_exp *e = n->data;
			
				printf("i: %d\n", i);
				if(e == NULL)
					continue;
				
				if(e->type == e_cmp)
				{
					sql_exp *el = (sql_exp*) e->l;
					sql_exp *er = (sql_exp*) e->r;
					sql_exp *ef = NULL;
					sql_exp *pivot = NULL;
					
					if(e->f)
					{
						ef = (sql_exp*) e->f;
					}
					
					if(el->type == e_column || er->type == e_column)
					{
						sql_column *c;
						atom *a;
						
						if(el->type != e_column)
						{
							pivot = el;
							el = er;
							er = pivot;
						}
						
						c = exp_find_column(rel, el, -1);
						if(c == NULL)
							continue;
						printf("column: %s & table: %s & er_type: %d & atom: %s & atom_r: %s\n", c->base.name, c->t->base.name, er->type, er->name, er->rname);
						
						if(find_prop(el->p, PROP_HASHCOL))
						{
							sql_ukey *su = (sql_ukey*) ((prop*)el->p)->value;
							if(!is_table_in_list_table_pkeys(discovered_table_pkeys, c->t))
							{
								// TODO: add the table only if it is a partially materialized view
								table_pkeys *tp = (table_pkeys*) GDKmalloc(sizeof(table_pkeys));
								tp->table_name = GDKstrdup(c->t->base.name);
								tp->pkey_column_names = extract_column_names_from_list_of_columns(su->k.columns);
								list_append(discovered_table_pkeys, tp);
							}
						}
						
						if(is_column_of_table_in_list_table_pkeys(discovered_table_pkeys, c->t, c))
						{
							
							sel_predicate* sp = (sel_predicate*) GDKmalloc(sizeof(sel_predicate));
							
							sp->cmp_type = e->flag;
							sp->column = c;
							sp->values = (ValRecord**) GDKmalloc(sizeof(ValRecord*));
							
							printf("pkey!\n");
							
							/* if it is a range */
							if(ef)
							{
								if(ef->type == e_convert)
									ef = ef->l;
								if (is_atom(ef->type) && (a = exp_value(ef, sql->args, sql->argc)) != NULL) 
								{
									ValRecord** vr = NULL;
									if(a->isnull)
										printf("ERROR: range boundary is given as NULL\n");
									
									/* TODO: how to understand which type of range it is */
									sp->cmp_type = 15;
									sp->num_values = 2;
									vr = (ValRecord**) GDKrealloc(sp->values, 2*sizeof(ValRecord*));
									if(vr == NULL)
									{
										printf("ERROR: can not allocate memory\n");
									}
									sp->values = vr;
									/* TODO: check if ef is always the upper boundary of the range */
									sp->values[1] = &(a->data);
									printf("atom2sql: %s\n", atom2sql(a));
								}
								else
								{
									printf("ERROR: NOT atom or NO value in atom!\n");
								}
							}
							
							if(er->type == e_convert)
								er = er->l;
							
							
							
							if (is_atom(er->type) && (a = exp_value(er, sql->args, sql->argc)) != NULL) 
							{
								
								if(a->isnull)
									printf("ERROR: primary key should not be NULL\n");
								
								
								switch(sp->cmp_type)
								{
									case 0:
									case 1:
									case 2:
									case 3:
									case 4:
									case 5:
										sp->num_values = 1;
										sp->values[0] = &(a->data);
										break;
									case 6:
									case 7:
										/* not handled (yet) */
										printf("ERROR: case not handled yet!\n");
										break;
									case 8:
									case 9:
										/* TODO: handle IN and NOT IN */
										printf("ERROR: case not handled yet!\n");
										break;
									case 10:
									case 11:
										/* not handled (yet) */
										printf("ERROR: case not handled yet!\n");
										break;
									case 12:
									case 13:
									case 14:
									case 15:
										sp->values[0] = &(a->data);
										break;
								}
								
								printf("atom2sql: %s\n", atom2sql(a));
							}
							else
							{
								printf("ERROR: NOT atom or NO value in atom!\n");
							}
							
							
							
							res = list_append(res, sp);
						}
						
						
						
						
						
					
					}
					else
						printf("ERROR: unexpected el->type: %d & er->type: %d\n", el->type, er->type);	
				}
				else 
					printf("ERROR: unexpected e->type: %d\n", e->type);
				
				/* collect the PERPAD */
				
				// how to get which table the corresponding attribute belongs to? All the selections has to be pushed down at this point. So, below the selection should be a base table. That is the table we look for.
				// how to recognize if this attribute is a pkey of a specific table? It is the pkey of the base table below, if it has the PROP_HASHCOL property.
				
				
			}
			return res;
		case op_groupby:
		case op_topn:
		case op_sample:
			return collect_PERPAD(sql, rel->l);
		case op_ddl:
			break;
		case op_insert:
		case op_update:
		case op_delete:
			break;
	}
	
	return NULL;
}


lng get_enum_step_length(sql_column* c)
{
	if(strcmp(c->t->base.name, "windowmetadata") == 0 && strcmp(c->base.name, "window_start_ts") == 0)
		return 3600000;
	else return 0;
}

sel_predicate** convert_all_into_in_clause_except_cmp_equal(list *list_of_PERPAD)
{
	int i;
	node *n = NULL;
	int num_sp;
	sel_predicate** sps;
	
	if(list_of_PERPAD == NULL)
		return NULL;
	
	num_sp = list_length(list_of_PERPAD);
	sps = (sel_predicate**) GDKmalloc(num_sp*sizeof(sel_predicate*));
	
	
	
	for(i = 0; i < num_sp; i++)
		sps[i] = (sel_predicate*) GDKmalloc(sizeof(sel_predicate));
	
	
	for (n = list_of_PERPAD->h, i = 0; n; n = n->next, i++) 
	{
		lng step_length;
		sel_predicate *sp = n->data;
		
		switch(sp->cmp_type)
		{
			case 0:
			case 1:
			case 2:
			case 3:
				/* not handled (yet) */
				printf("ERROR: conversion case not handled yet!\n");
				break;
			case 4:
				/* do not touch cmp_equal */
				sps[i]->cmp_type = sp->cmp_type;
				sps[i]->column = sp->column;
				sps[i]->values = sp->values;
				sps[i]->num_values = sp->num_values;
				
				break;
			case 5:
				/* not handled (yet) */
				printf("ERROR: conversion case not handled yet!\n");
				break;
			case 6:
			case 7:
				/* not handled (yet) */
				printf("ERROR: conversion case not handled yet!\n");
				break;
			case 8:
				/* already in clause */
				sps[i]->cmp_type = sp->cmp_type;
				sps[i]->column = sp->column;
				sps[i]->values = sp->values;
				sps[i]->num_values = sp->num_values;
				
				break;
			case 9:
				/* not handled (yet) */
				printf("ERROR: conversion case not handled yet!\n");
				break;
			case 10:
			case 11:
				/* not handled (yet) */
				printf("ERROR: conversion case not handled yet!\n");
				break;
			case 12:
			case 13:
			case 14:
			case 15:
				/* TODO: till now we assumed all ranges are 15 */
				switch(sp->values[0]->vtype)
				{
					case TYPE_str: /* should be timestamp, because enumerating a string range is not realistic */
					{
						timestamp *tl = (timestamp*) GDKmalloc(sizeof(timestamp)); 
						timestamp *th = (timestamp*) GDKmalloc(sizeof(timestamp)); 
						timestamp *tr_tl = (timestamp*) GDKmalloc(sizeof(timestamp)); 
						timestamp *tr_th = (timestamp*) GDKmalloc(sizeof(timestamp)); 
						timestamp *current = (timestamp*) GDKmalloc(sizeof(timestamp));
						timestamp *next = (timestamp*) GDKmalloc(sizeof(timestamp));
						lng range_diff = 0;
						int j;
						str str_time_unit = "hour";
						
						MTIMEtimestamp_fromstr(tl, &(sp->values[0]->val.sval));
						MTIMEtimestamp_fromstr(th, &(sp->values[1]->val.sval));
						step_length = get_enum_step_length(sp->column); /* TODO: window_unit should somehow have an effect here */
						
						/* ceiling tl, floor th */
						timestamp_trunc(tr_tl, tl, &str_time_unit); /* TODO: "str" should be specified somehow differently */
						timestamp_trunc(tr_th, th, &str_time_unit); /* TODO: "str" should be specified somehow differently */
						if(tr_tl->days == tl->days && tr_tl->msecs == tl->msecs)
							tl = tr_tl;
						else
							MTIMEtimestamp_add(tl, tr_tl, &step_length);
						th = tr_th;
						
						MTIMEtimestamp_diff(&range_diff, th, tl);
						sps[i]->num_values = (range_diff / step_length) + 1;
						sps[i]->values = (ValRecord**) GDKmalloc(sps[i]->num_values * sizeof(ValRecord*));
						
						sps[i]->column = sp->column;
						sps[i]->cmp_type = 8; /* cmp_in */
						
						*next = *tl;
						for(j = 0; j < sps[i]->num_values; j++)
						{
							int buf_size = 24;
							str buf = (str) GDKmalloc(buf_size * sizeof(char));
							*current = *next;
							sps[i]->values[j] = (ValRecord*) GDKmalloc(sizeof(ValRecord));
							sps[i]->values[j]->vtype = TYPE_str; /* keep enumerated timestamps as str again */
							
							timestamp_tostr(&buf, &buf_size, current);
							sps[i]->values[j]->val.sval = GDKstrdup(buf);
							
							MTIMEtimestamp_add(next, current, &step_length);
						}
						assert(current->days == th->days && current->msecs == th->msecs);
						
						break;
					}
					
					default:
						printf("ERROR: range for this type is not possible or not handled (yet)!\n");
						break;
				}
				break;
		}
	}
	
	return sps;
}

/* recursive function to enumerate the pkey space */
int enumerate_pkey_space(str** ret, sel_predicate** sps, int sps_enum_start, int num_PERPAD, int* is_pkey_to_be_enumerated)
{
// 	ret = (str**) GDKmalloc(sizeof(str*));
	
	if(!is_pkey_to_be_enumerated[sps_enum_start])
	{
		if(num_PERPAD == sps_enum_start + 1)
		{
			ret[0] = NULL;
			return 0;
		}
		else
			return enumerate_pkey_space(ret, sps, sps_enum_start+1, num_PERPAD, is_pkey_to_be_enumerated);
	}
	else
	{
		str* already_enumerated = NULL;
// 		str* enumerated;
		int num_already_enumerated;
		
		if(num_PERPAD == sps_enum_start + 1)
		{
			num_already_enumerated = 1;
		}
		else
		{
			num_already_enumerated = enumerate_pkey_space(&already_enumerated, sps, sps_enum_start+1, num_PERPAD, is_pkey_to_be_enumerated);
		}
		
		ret[0] = (str*) GDKmalloc(sps[sps_enum_start]->num_values * num_already_enumerated * sizeof(str));
		
		switch(sps[sps_enum_start]->values[0]->vtype)
		{
			case TYPE_str: /* should be timestamp, because enumerating a string range is not realistic */
			{
				int i, j;
				for(i = 0; i < sps[sps_enum_start]->num_values; i++)
				{
					for(j = 0; j < num_already_enumerated; j++)
					{
						str buf = (str)GDKmalloc(512*sizeof(char));
						if(already_enumerated)
						{
							sprintf(buf, "\'%s\', %s", sps[sps_enum_start]->values[i]->val.sval, already_enumerated[j]);
						}
						else
						{
							sprintf(buf, "\'%s\'", sps[sps_enum_start]->values[i]->val.sval);
						}
						ret[0][i * num_already_enumerated + j] = GDKstrdup(buf);
						GDKfree(buf);
					}
				}
				
				
			}
			break;
			default:
				printf("ERROR: enumerating pkey space: range for this type is not possible or not handled (yet)!\n");
				break;
		}
		
// 		*ret = enumerated;
		return sps[sps_enum_start]->num_values * num_already_enumerated;
	}
}

/* 
 Fill in this type of query:
 CREATE TEMP TABLE tt (d date) ON COMMIT PRESERVE ROWS;
 INSERT INTO tt VALUES ('2013-01-08'), ('2013-01-09'), ('2013-01-10'), ('2013-01-11'), ('2013-01-12'), ('2013-01-13'), ('2013-01-14'), ('2013-01-15'), ('2013-01-16'), ('2013-01-17'), ('2013-01-18'), ('2013-01-19'), ('2013-01-20'), ('2013-01-21'), ('2013-01-22');
 */
int* enumerate_and_insert_into_temp_table(mvc *sql, sel_predicate** sps, int num_PERPAD)
{
	/* all selection predicates in sps are either IN clause or point query (cmp_equal) */
	
	int i, j, num_pkeys_to_be_enumerated = 0;
	int *is_pkey_to_be_enumerated = (int*) GDKmalloc(num_PERPAD * sizeof(int));
	char temp_column_name = 97;
	str temp_table_name = "tt";
	str temp_table_name_res = "tt_res";
	str s, q, buf2, q2;
	str* enumerated_pkeys = NULL;
	int num_enumerated_pkeys;
	int enum_char_length = 0;
	Client cntxt;
	
	if(num_PERPAD == 0 || sps == NULL)
		return NULL;
	
	/* how many and which sp are IN clause? */
	for(i = 0; i < num_PERPAD; i++)
	{
		if(sps[i]->cmp_type == 8)
		{
			is_pkey_to_be_enumerated[i] = 1;
			num_pkeys_to_be_enumerated++;
		}
		else
			is_pkey_to_be_enumerated[i] = 0;
	}
	
	s = "CREATE TEMP TABLE %s (";
	j = 0;
	for(i = 0; i < num_PERPAD; i++)
	{
		if(is_pkey_to_be_enumerated[i])
		{
			str buf = (str)GDKmalloc(512*sizeof(char));
			if(j == 0)
				sprintf(buf, "%s%c %s", s, temp_column_name, sps[i]->column->type.type->sqlname);
			else
				sprintf(buf, "%s,%c %s", s, temp_column_name, sps[i]->column->type.type->sqlname);
			s = GDKstrdup(buf);
			GDKfree(buf);
			temp_column_name++;
			j++;
		}
	}
	
	buf2 = (str)GDKmalloc(BUFSIZ*sizeof(char));
	sprintf(buf2, "%s%s", s, ") ON COMMIT PRESERVE ROWS;\n");
	
	q = (str)GDKmalloc(BUFSIZ*sizeof(char));
	sprintf(q, buf2, temp_table_name);
	
	q2 = (str)GDKmalloc(BUFSIZ*sizeof(char));
	sprintf(q2, buf2, temp_table_name_res);
	
	printf("q1: %s", q);
	printf("q2: %s", q2);
	
	cntxt = MCgetClient(sql->clientid);
	
	/* TODO: how long will this temp table stay? There might be a new query trying to recreate it. */
	if(SQLstatementIntern(cntxt,&q,"pmv.create_temp_table",TRUE,FALSE)!= MAL_SUCCEED)
	{/* insert into query not succeeded, what to do */
		printf("***query didnt work: %s\n", q);
		return NULL;
	}
	
	/* TODO: how long will this temp table stay? There might be a new query trying to recreate it. */
	if(SQLstatementIntern(cntxt,&q2,"pmv.create_temp_table_res",TRUE,FALSE)!= MAL_SUCCEED)
	{/* insert into query not succeeded, what to do */
		printf("***query didnt work: %s\n", q);
		return NULL;
	}
	
	if(mvc_commit(sql, 0, NULL) < 0)
	{/* committing failed */
// 		throw(MAL,"pmv.create_temp_table", "committing failed\n");
		printf("***commit didnt work: %s\n", q);
		return NULL;
	}
	
	GDKfree(s);
	GDKfree(q);
	
	/* do the insert into temp table */
	s = (str)GDKmalloc(512*sizeof(char));
	sprintf(s, "INSERT INTO %s VALUES ", temp_table_name);
	
	
	num_enumerated_pkeys = enumerate_pkey_space(&enumerated_pkeys, sps, 0, num_PERPAD, is_pkey_to_be_enumerated);
	
	assert(num_enumerated_pkeys > 0 && enumerated_pkeys != NULL); //otherwise the query is not healthy.
	
	enum_char_length = num_enumerated_pkeys * (strlen(enumerated_pkeys[0]) + 4) + strlen(s);
	
	for(i = 0; i < num_enumerated_pkeys; i++)
	{
		str buf = (str)GDKmalloc(enum_char_length*sizeof(char));
		if(i == num_enumerated_pkeys - 1)
		{
			sprintf(buf, "%s(%s);\n", s, enumerated_pkeys[i]);
		}
		else
		{
			sprintf(buf, "%s(%s), ", s, enumerated_pkeys[i]);
		}
		s = GDKstrdup(buf);
		GDKfree(buf);
	}
	
	printf("q2: %s", s);
	
	if(SQLstatementIntern(cntxt,&s,"pmv.insert_into_temp_table",TRUE,FALSE)!= MAL_SUCCEED)
	{/* insert into query not succeeded, what to do */
		printf("***query didnt work: %s\n", q);
		return NULL;
	}
	
	GDKfree(s);
	return is_pkey_to_be_enumerated;
	
}

str VAL2str(ValRecord* valp)
{
	char buf[BUFSIZ];
	
	if(valp == NULL)
		return NULL;
	
	switch (valp->vtype) {
		case TYPE_lng:
			sprintf(buf, LLFMT, valp->val.lval);
			break;
		case TYPE_wrd:
			sprintf(buf, SSZFMT, valp->val.wval);
			break;
		case TYPE_oid:
			sprintf(buf, OIDFMT "@0", valp->val.oval);
			break;
		case TYPE_int:
			sprintf(buf, "%d", valp->val.ival);
			break;
		case TYPE_sht:
			sprintf(buf, "%d", valp->val.shval);
			break;
		case TYPE_bte:
			sprintf(buf, "%d", valp->val.btval);
			break;
		case TYPE_bit:
			if (valp->val.btval)
				return GDKstrdup("true");
			return GDKstrdup("false");
		case TYPE_flt:
			sprintf(buf, "%f", valp->val.fval);
			break;
		case TYPE_dbl:
			sprintf(buf, "%f", valp->val.dval);
			break;
		case TYPE_str:
			if (valp->val.sval)
				sprintf(buf, "\'%s\'", valp->val.sval);
			else
				sprintf(buf, "NULL");
			break;
		default:
			printf("ERROR: this type is not handled by VAL2str!\n");
			break;
	}
	return GDKstrdup(buf);
}

/* form and run this kind of query:
 * 
 INSERT INTO tt_res SELECT * FROM tt LEFT OUTER JOIN (SELECT d_st FROM days WHERE d_st >= '2013-01-08' AND d_st <= '2013-01-22') AS dd ON tt.d = dd.d_st;*
 */
void find_out_pkey_space_for_unavailable_required_derived_metadata(mvc* sql, list* list_of_PERPAD, int* is_pkey_to_be_enumerated, int num_pkeys_to_be_enumerated)
{
	/* Form the sub-query. That has the PERPAD, but like in the original query */
	
	int i,j;
	node *n = NULL;
	int num_sp;
	str s, table_name, buf2, q, schema_name, r;
	char temp_column_name;
	char temp_column_name_start = 97;
	str temp_table_name = "tt";
	str temp_table_name_res = "tt_res";
	Client cntxt;
		
	if(list_of_PERPAD == NULL || is_pkey_to_be_enumerated == NULL)
		return;
	
	num_sp = list_length(list_of_PERPAD);
	
	s = "SELECT ";
	
	for (n = list_of_PERPAD->h, i = 0, j = 0; n; n = n->next, i++) 
	{
		sel_predicate *sp = n->data;
		
		if(is_pkey_to_be_enumerated[i])
		{
			str buf = (str)GDKmalloc(num_sp*64*sizeof(char));
			j++;
			if(j == num_pkeys_to_be_enumerated)
				sprintf(buf, "%s %s", s, sp->column->base.name);
			else
				sprintf(buf, "%s %s,", s, sp->column->base.name);
			s = GDKstrdup(buf);
			GDKfree(buf);
		}
	}
	
	table_name = ((sel_predicate*)list_of_PERPAD->h->data)->column->t->base.name;
	schema_name = ((sel_predicate*)list_of_PERPAD->h->data)->column->t->s->base.name;
	
	buf2 = (str)GDKmalloc((strlen(s) + 128)*sizeof(char));
	sprintf(buf2, "%s FROM %s.%s WHERE ", s, schema_name, table_name);
	s = GDKstrdup(buf2);
	GDKfree(buf2);
	
	for (n = list_of_PERPAD->h, i = 0, j = 0; n; n = n->next, i++) 
	{
		sel_predicate *sp = n->data;
		
		str buf = (str)GDKmalloc(num_sp*128*sizeof(char));
		
		switch(sp->cmp_type)
		{
			case 0:
			case 1:
			case 2:
			case 3:
				/* not handled (yet) */
				printf("ERROR: printing case not handled yet!\n");
				break;
			case 4:
				
				sprintf(buf, "%s %s = %s", s, sp->column->base.name, VAL2str(sp->values[0]));
				break;
			case 5:
				/* not handled (yet) */
				printf("ERROR: printing case not handled yet!\n");
				break;
			case 6:
			case 7:
				/* not handled (yet) */
				printf("ERROR: printing case not handled yet!\n");
				break;
			case 8:
				/* not handled (yet) */
				printf("ERROR: printing case not handled yet!\n");
				break;
			case 9:
				/* not handled (yet) */
				printf("ERROR: printing case not handled yet!\n");
				break;
			case 10:
			case 11:
				/* not handled (yet) */
				printf("ERROR: printing case not handled yet!\n");
				break;
			case 12:
				sprintf(buf, "%s %s > %s AND %s < %s", s, sp->column->base.name, VAL2str(sp->values[0]), sp->column->base.name, VAL2str(sp->values[1]));
				break;
			case 13:
				sprintf(buf, "%s %s > %s AND %s <= %s", s, sp->column->base.name, VAL2str(sp->values[0]), sp->column->base.name, VAL2str(sp->values[1]));
				break;
			case 14:
				sprintf(buf, "%s %s >= %s AND %s < %s", s, sp->column->base.name, VAL2str(sp->values[0]), sp->column->base.name, VAL2str(sp->values[1]));
				break;
			case 15:
				sprintf(buf, "%s %s >= %s AND %s <= %s", s, sp->column->base.name, VAL2str(sp->values[0]), sp->column->base.name, VAL2str(sp->values[1]));
				break;
		}
		
		if(i != num_sp - 1)
		{
			s = GDKstrdup(buf);
			sprintf(buf, "%s AND ", s);
		}
		s = GDKstrdup(buf);
		GDKfree(buf);
	}
	
	printf("subquery: %s\n", s);
	
	
	/* form the query */
	q = "INSERT INTO %s SELECT ";
	
	temp_column_name = temp_column_name_start;
	for (n = list_of_PERPAD->h, i = 0, j = 0; n; n = n->next, i++) 
	{
		if(is_pkey_to_be_enumerated[i])
		{
			str buf = (str)GDKmalloc((BUFSIZ + num_pkeys_to_be_enumerated * 128)*sizeof(char));
			j++;
			if(j == num_pkeys_to_be_enumerated)
				sprintf(buf, "%s%c ", q, temp_column_name);
			else
				sprintf(buf, "%s%c, ", q, temp_column_name);
			q = GDKstrdup(buf);
			GDKfree(buf);
			temp_column_name++;
			
		}
	}
	
	r = "FROM %s LEFT OUTER JOIN (%s) AS aa ON ";
	buf2 = (str)GDKmalloc((strlen(q) + strlen(r))*sizeof(char));
	sprintf(buf2, "%s%s", q, r);
	q = GDKstrdup(buf2);
	GDKfree(buf2);
	
	temp_column_name = temp_column_name_start;
	for (n = list_of_PERPAD->h, i = 0, j = 0; n; n = n->next, i++) 
	{
		sel_predicate *sp = n->data;
		
		if(is_pkey_to_be_enumerated[i])
		{
			str buf = (str)GDKmalloc((BUFSIZ + num_pkeys_to_be_enumerated * 128)*sizeof(char));
			j++;
			if(j == num_pkeys_to_be_enumerated)
				sprintf(buf, "%s %s.%c = aa.%s;\n", q, temp_table_name, temp_column_name, sp->column->base.name);
			else
				sprintf(buf, "%s %s.%c = aa.%s AND ", q, temp_table_name, temp_column_name, sp->column->base.name);
			q = GDKstrdup(buf);
			GDKfree(buf);
			temp_column_name++;
			
		}
	}
	
	buf2 = (str)GDKmalloc((strlen(q) + 128 + strlen(s))*sizeof(char));
	sprintf(buf2, q, temp_table_name_res, temp_table_name, s);
	q = GDKstrdup(buf2);
	GDKfree(buf2);
	
	printf("query: %s\n", q);
	
	
	cntxt = MCgetClient(sql->clientid);
	
	/* TODO: how long will this temp table stay? There might be a new query trying to recreate it. */
	if(SQLstatementIntern(cntxt,&q,"pmv.left_outer_join",TRUE,FALSE)!= MAL_SUCCEED)
	{/* insert into query not succeeded, what to do */
		printf("***query didnt work: %s\n", q);
		return;
	}
	
	if(mvc_commit(sql, 0, NULL) < 0)
	{/* committing failed */
		// 		throw(MAL,"pmv.create_temp_table", "committing failed\n");
		printf("***commit didnt work: %s\n", q);
		return;
	}
	
	GDKfree(s);
	GDKfree(q);
}

str* get_pkey_bound_to_dataview(str schema_name, str dmdt_name)
{
	str* ret = (str*)GDKmalloc(3*sizeof(str));
	ret[0] = "station";
	ret[1] = "channel";
	ret[2] = "1";
	
	if(strcmp(schema_name, "mseed") == 0 && strcmp(dmdt_name, "windowmetadata") == 0)
		return ret;
	return NULL;
}

str form_pkey_select_str(sel_predicate** sps, int num_PERPAD, str* pkey_bound_to_dataview, str* select_str_per_pkey)
{
	str s = "";
	int i;
	str time_pkey_id = "1";
	for(i = 0; i < num_PERPAD; i++)
	{
		str buf = (str)GDKmalloc(BUFSIZ*sizeof(char));
		if(pkey_bound_to_dataview[i] == NULL || strcmp(pkey_bound_to_dataview[i], time_pkey_id) == 0)
		{
			if(i == num_PERPAD - 1)
				sprintf(buf, "%s%s AS %s", s, select_str_per_pkey[i], sps[i]->column->base.name);
			else
				sprintf(buf, "%s%s AS %s, ", s, select_str_per_pkey[i], sps[i]->column->base.name);
			
		}
		else
		{
			if(i == num_PERPAD - 1)
				sprintf(buf, "%s%s", s, select_str_per_pkey[i]);
			else
				sprintf(buf, "%s%s, ", s, select_str_per_pkey[i]);
		}
		s = GDKstrdup(buf);
		GDKfree(buf);
	}
	
	return s;
}

str get_non_pkey_select_str(str schema_name, str dmdt_name)
{
	if(strcmp(schema_name, "mseed") == 0 && strcmp(dmdt_name, "windowmetadata") == 0)
		return "MIN(sample_value) AS min_val, MAX(sample_value) AS max_val, AVG(sample_value) AS avg_val, SUM(sample_value) AS sum_val";
	else
		return NULL;
}

/* Form such a query and run:
 INSERT INTO mseed.windowmetadata
 SELECT station, channel, 3600 AS unit, t.a AS start_ts, MIN(sample_value) AS min_val, MAX(sample_value) AS max_val, AVG(sample_value) AS avg_val, stddev_samp(sample_value) AS std_dev
 FROM mseed.dataview AS v, tt_res AS t
 WHERE station = 'HGN' AND channel = 'BHZ' AND start_time < t.a + INTERVAL '1' HOUR AND end_time > t.a AND sys.date_trunc(sample_time, 'hour') = t.a
 GROUP BY station, channel, unit, start_ts;
 */
void compute_and_insert_unavailable_required_derived_metadata(mvc* sql, sel_predicate** sps, int num_PERPAD, int* is_pkey_to_be_enumerated, int num_pkeys_to_be_enumerated)
{
	// str q = "INSERT INTO mseed.windowmetadata SELECT station, channel, 3600 AS unit, t.a AS start_ts, MIN(sample_value) AS min_val, MAX(sample_value) AS max_val, AVG(sample_value) AS avg_val, stddev_samp(sample_value) AS std_dev FROM mseed.dataview, tt_res AS t WHERE station = 'HGN' AND channel = 'BHZ' AND start_time < t.a + INTERVAL '1' HOUR AND end_time > t.a AND sys.date_trunc(sample_time, 'hour') = t.a GROUP BY station, channel, unit, start_ts;";
	
	str* pkey_predicates_equal_to;
	str* select_str_per_pkey;
	str* pkey_bound_to_dataview;
	char temp_column_name;
	char temp_column_name_start = 97;
	str temp_table_name_res = "tt_res";
	str time_pkey_id = "1";
	int isnt_time_pkey = 1;
	int idx_time_pkey = -1;
	str s, r, q, u, buf2, schema_name, dmdt_name, pkey_select_str, non_pkey_select_str, from_join_temp_table_str, start_ts_str, interval_str, date_trunc_str, non_time_pkey_predicates_str, group_by_str;
	int i;
	Client cntxt;
	str msg;
	
	if(num_PERPAD == 0 || sps == NULL || is_pkey_to_be_enumerated == NULL)
		return;
	
	u = "INSERT INTO %s.%s SELECT %s, %s FROM mseed.dataview%sWHERE start_time < %s + INTERVAL %s AND end_time > %s AND sys.date_trunc(sample_time, \'%s\') = %s %s GROUP BY %s;";
	
	if(num_pkeys_to_be_enumerated == 0)
	{
		from_join_temp_table_str = " ";
	}
	else
	{
		/* add the temp table that contains results to from clause */
		str buf = (str)GDKmalloc(BUFSIZ*sizeof(char));
		sprintf(buf, ", %s AS t ", temp_table_name_res);
		from_join_temp_table_str = GDKstrdup(buf);
		GDKfree(buf);
	}
	
	schema_name = sps[0]->column->t->s->base.name;
	dmdt_name = sps[0]->column->t->base.name;
	
	pkey_predicates_equal_to = (str*)GDKmalloc(num_PERPAD*sizeof(str));
	select_str_per_pkey = (str*)GDKmalloc(num_PERPAD*sizeof(str));
	pkey_bound_to_dataview = get_pkey_bound_to_dataview(schema_name, dmdt_name);
	
	temp_column_name = temp_column_name_start;
	for(i = 0; i < num_PERPAD; i++)
	{ /* ASSUMPTION: bound pkey is never a time_pkey */
		if(pkey_bound_to_dataview[i] == NULL || (isnt_time_pkey = strcmp(pkey_bound_to_dataview[i], time_pkey_id)) == 0)
		{/* no bound to dataview */
			if(is_pkey_to_be_enumerated[i])
			{
				str buf = (str)GDKmalloc(BUFSIZ*sizeof(char));
				sprintf(buf, "t.%c", temp_column_name);
				select_str_per_pkey[i] = GDKstrdup(buf);
				GDKfree(buf);
				temp_column_name++;
				if(isnt_time_pkey == 0)
					idx_time_pkey = i;
			}
			else
			{
				if(isnt_time_pkey == 0)
				{
					str buf = (str)GDKmalloc(BUFSIZ*sizeof(char));
					sprintf(buf, "TIMESTAMP %s", VAL2str(sps[i]->values[0]));
					select_str_per_pkey[i] = GDKstrdup(buf);
					GDKfree(buf);
					idx_time_pkey = i;
				}
				else
					select_str_per_pkey[i] = GDKstrdup(VAL2str(sps[i]->values[0]));
			}
			pkey_predicates_equal_to[i] = NULL;
			isnt_time_pkey = 1;
		}
		else
		{/* bound to dataview */
			select_str_per_pkey[i] = pkey_bound_to_dataview[i];
			
			if(is_pkey_to_be_enumerated[i])
			{
				
				str buf = (str)GDKmalloc(BUFSIZ*sizeof(char));
				sprintf(buf, "t.%c", temp_column_name);
				pkey_predicates_equal_to[i] = GDKstrdup(buf);
				GDKfree(buf);
				temp_column_name++;
			}
			else
			{
				pkey_predicates_equal_to[i] = GDKstrdup(VAL2str(sps[i]->values[0]));
			}
		}
		
	}
	
	/* preparing the non_time_pkey_predicates_str */
	s = "";
	for(i = 0; i < num_PERPAD; i++)
	{
		if(pkey_predicates_equal_to[i])
		{
			str buf = (str)GDKmalloc(BUFSIZ*sizeof(char));
			sprintf(buf, "%sAND %s = %s ", s, pkey_bound_to_dataview[i], pkey_predicates_equal_to[i]);
			s = GDKstrdup(buf);
			GDKfree(buf);
		}
	}
	
	/* preparing the group_by_str */
	r = "";
	for(i = 0; i < num_PERPAD; i++)
	{
		str buf = (str)GDKmalloc(BUFSIZ*sizeof(char));
		
		if(pkey_bound_to_dataview[i] == NULL || strcmp(pkey_bound_to_dataview[i], time_pkey_id) == 0)
		{ /* no bound to dataview */
			if(i == num_PERPAD - 1)
				sprintf(buf, "%s%s", r, sps[i]->column->base.name);
			else
				sprintf(buf, "%s%s, ", r, sps[i]->column->base.name);
		}
		else
		{
			if(i == num_PERPAD - 1)
				sprintf(buf, "%s%s", r, pkey_bound_to_dataview[i]);
			else
				sprintf(buf, "%s%s, ", r, pkey_bound_to_dataview[i]);
		}
		
		r = GDKstrdup(buf);
		GDKfree(buf);
	}
	
// 	u = "INSERT INTO %s.%s SELECT %s, %s FROM mseed.dataview%sWHERE start_time < %s + INTERVAL %s AND end_time > %s AND sys.date_trunc(sample_time, \'%s\') = %s %s GROUP BY %s;";
	
	schema_name = schema_name;
	dmdt_name = dmdt_name;
	pkey_select_str = form_pkey_select_str(sps, num_PERPAD, pkey_bound_to_dataview, select_str_per_pkey);
	non_pkey_select_str = get_non_pkey_select_str(schema_name, dmdt_name);
	from_join_temp_table_str = from_join_temp_table_str;
	if(idx_time_pkey >= 0)
		start_ts_str = select_str_per_pkey[idx_time_pkey];
	else
	{
		printf("***no time_pkey!: %s\n", u);
		return;
	}
	
	/* TODO: somehow window_unit has to have an effect here */
	interval_str = "\'1\' HOUR";
	date_trunc_str = "hour";
	
	non_time_pkey_predicates_str = s;
	group_by_str = r;
	
	buf2 = (str)GDKmalloc(num_PERPAD*BUFSIZ*sizeof(char));
	sprintf(buf2, u, schema_name, dmdt_name, pkey_select_str, non_pkey_select_str, from_join_temp_table_str, start_ts_str, interval_str, start_ts_str, date_trunc_str, start_ts_str, non_time_pkey_predicates_str, group_by_str);
	q = GDKstrdup(buf2);
	GDKfree(buf2);
	
	printf("q: %s\n", q);
	
	cntxt = MCgetClient(sql->clientid);
	
	/* TODO: how long will this temp table stay? There might be a new query trying to recreate it. */
	if((msg = SQLstatementIntern(cntxt,&q,"pmv.insert_unavailable",TRUE,FALSE)) != MAL_SUCCEED)
	{/* insert into query not succeeded, what to do */
		printf("***query didnt work, %s: %s\n", msg, q);
		return;
	}
	
	if(mvc_commit(sql, 0, NULL) < 0)
	{/* committing failed */
		// 		throw(MAL,"pmv.create_temp_table", "committing failed\n");
		printf("***commit didnt work: %s\n", q);
		return;
	}
	
	GDKfree(q);

}


static bit 
has_actual_data_table(sql_rel *rel)
{
	switch (rel->op) {
		case op_basetable:
		case op_table:
			if(find_prop(rel->p, PROP_ACTUAL_DATA_NEEDED))
				return TRUE;
			break;
		case op_join: 
		case op_left: 
		case op_right: 
		case op_full: 
			
		case op_semi: 
		case op_anti: 
			
		case op_union: 
		case op_inter: 
		case op_except: 
			return (has_actual_data_table(rel->l) || has_actual_data_table(rel->r));
		case op_project:
		case op_select:
		case op_groupby:
		case op_topn:
		case op_sample:
		case op_ddl:
			return has_actual_data_table(rel->l);
		case op_insert:
		case op_update:
		case op_delete:
			break;
	}
	
	return FALSE;
}

static list *
order_join_expressions(sql_allocator *sa, list *dje, list *rels)
{
	list *res = sa_list(sa);
	node *n = NULL;
	int i, j, *keys, *pos, cnt = list_length(dje);

	keys = (int*)malloc(cnt*sizeof(int));
	pos = (int*)malloc(cnt*sizeof(int));
	for (n = dje->h, i = 0; n; n = n->next, i++) {
		sql_exp *e = n->data;

		keys[i] = exp_keyvalue(e);
		
		/* add some weight for the selections and remove some weight for the actual data table */
		if (e->type == e_cmp && !is_complex_exp(e->flag)) {
			sql_rel *l = find_rel(rels, e->l);
			sql_rel *r = find_rel(rels, e->r);

			if (l && is_select(l->op) && l->exps)
				keys[i] += list_length(l->exps)*10;
			if (r && is_select(r->op) && r->exps)
				keys[i] += list_length(r->exps)*10;
			if(has_actual_data_table(l))
				keys[i] += -10000;
			if(has_actual_data_table(r))
				keys[i] += -10000;
		}
		pos[i] = i;
	}
	/* sort descending */
	if (cnt > 1) 
		GDKqsort_rev(keys, pos, NULL, cnt, sizeof(int), sizeof(int), TYPE_int);
	for(j=0; j<cnt; j++) {
		for(n = dje->h, i = 0; i != pos[j]; n = n->next, i++) 
			;
		list_append(res, n->data);
	}
	free(keys);
	free(pos);
	return res;
}

static sql_exp *
rel_find_column( sql_allocator *sa, sql_rel *rel, char *tname, char *cname )
{
	if (!rel)
		return NULL;

	if (rel->exps && (is_project(rel->op) || is_base(rel->op))) {
		sql_exp *e = exps_bind_column2(rel->exps, tname, cname);
		if (e)
			return exp_alias(sa, e->rname, exp_name(e), tname, cname, exp_subtype(e), e->card, has_nil(e), is_intern(e));
	}
	if (is_project(rel->op) && rel->l) {
		return rel_find_column(sa, rel->l, tname, cname);
	} else if (is_join(rel->op)) {
		sql_exp *e = rel_find_column(sa, rel->l, tname, cname);
		if (!e)
			e = rel_find_column(sa, rel->r, tname, cname);
		return e;
	} else if (is_set(rel->op) ||
		   is_sort(rel) ||
		   is_semi(rel->op) ||
		   is_select(rel->op)) {
		if (rel->l)
			return rel_find_column(sa, rel->l, tname, cname);
	}
	return NULL;
}

static list *
find_fk( mvc *sql, list *rels, list *exps) 
{
	node *djn;
	list *sdje, *aje, *dje;

	/* first find the distinct join expressions */
	aje = list_select(exps, (void*)1, (fcmp) &exp_is_join, (fdup)NULL);
	dje = list_distinct2(aje, rels, (fcmp2) &joinexp_cmp, (fdup)NULL);
	for(djn=dje->h; djn; djn = djn->next) {
		/* equal join expressions */
		sql_idx *idx = NULL;
		sql_exp *je = djn->data, *le = je->l, *re = je->r; 

		if (is_complex_exp(je->flag))
			break;
		if (!find_prop(je->p, PROP_JOINIDX)) {
			int swapped = 0;
			list *aaje = matching_joins(sql->sa, rels, aje, je);
			list *eje = list_select(aaje, (void*)1, (fcmp) &exp_is_eqjoin, (fdup)NULL);
			sql_rel *lr = find_rel(rels, le), *olr = lr;
			sql_rel *rr = find_rel(rels, re), *orr = rr;
			sql_rel *bt = NULL;
			char *iname;

			sql_table *l, *r;
			list *lexps = list_map(eje, lr, (fmap) &joinexp_col);
			list *rexps = list_map(eje, rr, (fmap) &joinexp_col);
			list *lcols, *rcols;
			
			lr = find_basetable(lr);
			rr = find_basetable(rr);
			if (!lr || !rr) 
				continue;
			l = lr->l;
			r = rr->l;
			lcols = list_map(lexps, lr, (fmap) &table_colexp);
			rcols = list_map(rexps, rr, (fmap) &table_colexp);
			if (list_length(lcols) != list_length(rcols)) {
				lcols->destroy = NULL;
				rcols->destroy = NULL;
				continue;
			}

			idx = find_fk_index(l, lcols, r, rcols); 
			if (!idx) {
				idx = find_fk_index(r, rcols, l, lcols); 
				swapped = 1;
			} 

			if (idx && (iname = sa_strconcat( sql->sa, "%", idx->base.name)) != NULL &&
				   ((!swapped && name_find_column(olr, NULL, iname, -2, &bt) == NULL) ||
			            ( swapped && name_find_column(orr, NULL, iname, -2, &bt) == NULL))) 
				idx = NULL;

			if (idx) { 
				prop *p;
				node *n;
				sql_exp *t = NULL, *i = NULL;
	
				if (list_length(lcols) > 1 || !mvc_debug_on(sql, 512)) { 

					/* Add join between idx and TID */
					if (swapped) {
						sql_exp *s = je->l, *l = je->r;

						t = rel_find_column(sql->sa, lr, s->l, TID);
						i = rel_find_column(sql->sa, rr, l->l, iname);
						assert(t && i);
						je = exp_compare(sql->sa, i, t, cmp_equal);
					} else {
						sql_exp *s = je->r, *l = je->l;

						t = rel_find_column(sql->sa, rr, s->l, TID);
						i = rel_find_column(sql->sa, lr, l->l, iname);
						assert(t && i);
						je = exp_compare(sql->sa, i, t, cmp_equal);
					}

					/* Remove all join expressions */
					for (n = eje->h; n; n = n->next) 
						list_remove_data(exps, n->data);
					append(exps, je);
					djn->data = je;
				} else if (swapped) { /* else keep je for single column expressions */
					je = exp_compare(sql->sa, je->r, je->l, cmp_equal);
					/* Remove all join expressions */
					for (n = eje->h; n; n = n->next) 
						list_remove_data(exps, n->data);
					append(exps, je);
					djn->data = je;
				}
				je->p = p = prop_create(sql->sa, PROP_JOINIDX, je->p);
				p->value = idx;
			}
			lcols->destroy = NULL;
			rcols->destroy = NULL;
		}
	}

	/* sort expressions on weighted number of reducing operators */
	sdje = order_join_expressions(sql->sa, dje, rels);
	return sdje;
}

static sql_rel *
order_joins(mvc *sql, list *rels, list *exps)
{
	sql_rel *top = NULL, *l = NULL, *r = NULL;
	sql_exp *cje;
	node *djn;
	list *sdje, *n_rels = new_rel_list(sql->sa);
	int fnd = 0;

	/* find foreign keys and reorder the expressions on reducing quality */
	sdje = find_fk(sql, rels, exps);

	if (list_length(rels) > 2 && mvc_debug_on(sql, 256))
		return rel_planner(sql, rels, sdje);

	/* open problem, some expressions use more than 2 relations */
	/* For example a.x = b.y * c.z; */
	if (list_length(rels) >= 2 && sdje->h) {
		/* get the first expression */
		cje = sdje->h->data;

		/* find the involved relations */

		/* complex expressions may touch multiple base tables 
		 * Should be pushed up to extra selection.
		 * */
		if (cje->type != e_cmp || !is_complex_exp(cje->flag) || !find_prop(cje->p, PROP_HASHCOL) /*||
		   (cje->type == e_cmp && cje->f == NULL)*/) {
			l = find_one_rel(rels, cje->l);
			r = find_one_rel(rels, cje->r);
		}

		if (l && r && l != r) {
			list_remove_data(sdje, cje);
			list_remove_data(exps, cje);
		}
	}
	if (l && r && l != r) {
		list_remove_data(rels, l);
		list_remove_data(rels, r);
		list_append(n_rels, l);
		list_append(n_rels, r);

		/* Create a relation between l and r. Since the calling 
	   	   functions rewrote the join tree, into a list of expressions 
	   	   and a list of (simple) relations, there are no outer joins 
	   	   involved, we can simply do a crossproduct here.
	 	 */
		top = rel_crossproduct(sql->sa, l, r, op_join);
		rel_join_add_exp(sql->sa, top, cje);

		/* all other join expressions on these 2 relations */
		while((djn = list_find(exps, n_rels, (fcmp)&exp_joins_rels)) != NULL) {
			sql_exp *e = djn->data;

			rel_join_add_exp(sql->sa, top, e);
			list_remove_data(exps, e);
		}
		/* Remove other joins on the current 'n_rels' set in the distinct list too */
		while((djn = list_find(sdje, n_rels, (fcmp)&exp_joins_rels)) != NULL) 
			list_remove_data(sdje, djn->data);
		fnd = 1;
	}
	/* build join tree using the ordered list */
	while(list_length(exps) && fnd) {
		fnd = 0;
		/* find the first expression which could be added */
		for(djn = sdje->h; djn && !fnd && rels->h; djn = (!fnd)?djn->next:NULL) {
			node *ln, *rn, *en;
			
			cje = djn->data;
			ln = list_find(n_rels, cje->l, (fcmp)&rel_has_exp);
			rn = list_find(n_rels, cje->r, (fcmp)&rel_has_exp);

			if (ln || rn) {
				/* remove the expression from the lists */
				list_remove_data(sdje, cje);
				list_remove_data(exps, cje);
			}
			if (ln && rn) {
				assert(0);
				/* create a selection on the current */
				l = ln->data;
				r = rn->data;
				rel_join_add_exp(sql->sa, top, cje);
				fnd = 1;
			} else if (ln || rn) {
				if (ln) {
					l = ln->data;
					r = find_rel(rels, cje->r);
				} else {
					l = rn->data;
					r = find_rel(rels, cje->l);
				}
				list_remove_data(rels, r);
				append(n_rels, r);

				/* create a join using the current expression */
				top = rel_crossproduct(sql->sa, top, r, op_join);
				rel_join_add_exp(sql->sa, top, cje);

				/* all join expressions on these tables */
				while((en = list_find(exps, n_rels, (fcmp)&exp_joins_rels)) != NULL) {
					sql_exp *e = en->data;
					rel_join_add_exp(sql->sa, top, e);
					list_remove_data(exps, e);
				}
				/* Remove other joins on the current 'n_rels' 
				   set in the distinct list too */
				while((en = list_find(sdje, n_rels, (fcmp)&exp_joins_rels)) != NULL) 
					list_remove_data(sdje, en->data);
				fnd = 1;
			}
		}
	}
	if (list_length(rels)) { /* more relations */
		node *n;
		for(n=rels->h; n; n = n->next) {
			if (top)
				top = rel_crossproduct(sql->sa, top, n->data, op_join);
			else 
				top = n->data;
		}
	}
	if (list_length(exps)) { /* more expressions (add selects) */
		node *n;
		set_processed(top);
		top = rel_select(sql->sa, top, NULL);
		for(n=exps->h; n; n = n->next) {
			sql_exp *e = n->data;

			/* find the involved relations */

			/* complex expressions may touch multiple base tables 
		 	 * Should be push up to extra selection. */
			/*
			l = find_one_rel(rels, e->l);
			r = find_one_rel(rels, e->r);

			if (l && r) 
			*/
			if (exp_is_join_exp(e) == 0)
				rel_join_add_exp(sql->sa, top->l, e);
			else
				rel_select_add_exp(top, e);
		}
	}
	return top;
}

static int
rel_neg_in_size(sql_rel *r)
{
	if (is_union(r->op) && r->nrcols == 0) 
		return -1 + rel_neg_in_size(r->l);
	if (is_project(r->op) && r->nrcols == 0) 
		return -1;
	return 0;
}

static list *
push_in_join_down(mvc *sql, list *rels, list *exps)
{
	node *n;
	int restart = 1;
	list *nrels;

	/* we should sort these first, ie small in's before large one's */
	nrels = list_sort(rels, (fkeyvalue)&rel_neg_in_size, (fdup)&rel_dup);

	/* we need to cleanup, the new refs ! */
	rels->destroy = (fdestroy)rel_destroy;
	list_destroy(rels);
	rels = nrels;

	/* one of the rels should be a op_union with nrcols == 0 */
	while(restart) {
	    for(n = rels->h; n; n = n->next) {
		sql_rel *r = n->data;
	
		restart = 0;
		if ((is_union(r->op) || is_project(r->op)) && r->nrcols == 0) {
			/* next step find expression on this relation */
			node *m;
			sql_rel *l = NULL;
			sql_exp *je = NULL;

			for(m = exps->h; !je && m; m = m->next) {
				sql_exp *e = m->data;

				if (e->type == e_cmp && e->flag == cmp_equal) {
					/* in values are on 
						the right of the join */
					if (rel_has_exp(r, e->r) >= 0) 
						je = e;
				}
			}
			/* with this expression find other relation */
			if (je && (l = find_rel(rels, je->l)) != NULL) {
				sql_rel *nr = rel_crossproduct(sql->sa, l, r, op_join);

				rel_join_add_exp(sql->sa, nr, je);
				list_append(rels, nr); 
				list_remove_data(rels, l);
				list_remove_data(rels, r);
				list_remove_data(exps, je);
				restart = 1;
				break;
			}

		}
	    }
	}
	return rels;
}

static sql_rel *
reorder_join(mvc *sql, sql_rel *rel)
{
	list *exps = rel->exps;
	list *rels;

	if (!exps) /* crosstable, ie order not important */
		return rel;
	rel->exps = NULL; /* should be all crosstables by now */
 	rels = new_rel_list(sql->sa);
	if (is_outerjoin(rel->op)) {
		int cnt = 0;
		/* try to use an join index also for outer joins */
		list_append(rels, rel->l);
		list_append(rels, rel->r);
		cnt = list_length(exps);
		rel->exps = find_fk(sql, rels, exps);
		if (list_length(rel->exps) != cnt) 
			rel->exps = order_join_expressions(sql->sa, exps, rels);
	} else { 
 		get_relations(rel, rels);
		if (list_length(rels) > 1) {
			rels = push_in_join_down(sql, rels, exps);
			rel = order_joins(sql, rels, exps);
		} else {
			rel->exps = exps;
			exps = NULL;
		}
	}
	return rel;
}

static list *
push_up_join_exps( sql_rel *rel) 
{
	if (rel_is_ref(rel))
		return NULL;

	switch(rel->op) {
	case op_join: {
		sql_rel *rl = rel->l;
		sql_rel *rr = rel->r;
		list *l, *r;

		if (rel_is_ref(rl) && rel_is_ref(rr)) {
			l = rel->exps;
			rel->exps = NULL;
			return l;
		}
		l = push_up_join_exps(rl);
		r = push_up_join_exps(rr);
		if (l && r) {
			l = list_merge(l, r, (fdup)NULL);
			r = NULL;
		}
		if (rel->exps) {
			if (l && !r)
				r = l;
			l = list_merge(rel->exps, r, (fdup)NULL);
		}
		rel->exps = NULL;
		return l;
	}
	default:
		return NULL;
	}
}

static sql_rel *
rel_join_order(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)*changes;
	if (is_join(rel->op) && rel->exps && !rel_is_ref(rel)) {
		if (rel->op == op_join)
			rel->exps = push_up_join_exps(rel);
		rel = reorder_join(sql, rel);
	}
	return rel;
}

/* exp_rename */
static sql_exp * exp_rename(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t);

static list *
exps_rename(mvc *sql, list *l, sql_rel *f, sql_rel *t) 
{
	node *n;
	list *nl = new_exp_list(sql->sa);

	for(n=l->h; n; n=n->next) {
		sql_exp *arg = n->data;

		arg = exp_rename(sql, arg, f, t);
		if (!arg) 
			return NULL;
		append(nl, arg);
	}
	return nl;
}

/* exp_rename */
static sql_exp *
exp_rename(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t) 
{
	sql_exp *ne = NULL, *l, *r, *r2;

	switch(e->type) {
	case e_column:
		if (e->l) { 
			ne = exps_bind_column2(f->exps, e->l, e->r);
			/* if relation name matches expressions relation name, find column based on column name alone */
		} else {
			ne = exps_bind_column(f->exps, e->r, NULL);
		}
		if (!ne)
			return e;
		e = NULL;
		if (ne->name && ne->r && ne->l) 
			e = rel_bind_column2(sql, t, ne->l, ne->r, 0);
		if (!e && ne->r)
			e = rel_bind_column(sql, t, ne->r, 0);
		sql->session->status = 0;
		sql->errstr[0] = 0;
		if (!e && exp_is_atom(ne))
			return ne;
		return e;
	case e_cmp: 
		if (e->flag == cmp_or) {
			list *l = exps_rename(sql, e->l, f, t);
			list *r = exps_rename(sql, e->r, f, t);
			if (l && r)
				ne = exp_or(sql->sa, l,r);
		} else if (e->flag == cmp_in || e->flag == cmp_notin || get_cmp(e) == cmp_filter) {
			sql_exp *l = exp_rename(sql, e->l, f, t);
			list *r = exps_rename(sql, e->r, f, t);
			if (l && r) {
				if (get_cmp(e) == cmp_filter) 
					ne = exp_filter(sql->sa, l, r, e->f, is_anti(e));
				else
					ne = exp_in(sql->sa, l, r, e->flag);
			}
		} else {
			l = exp_rename(sql, e->l, f, t);
			r = exp_rename(sql, e->r, f, t);
			if (e->f) {
				r2 = exp_rename(sql, e->f, f, t);
				if (l && r && r2)
					ne = exp_compare2(sql->sa, l, r, r2, e->flag);
			} else if (l && r) {
				ne = exp_compare(sql->sa, l, r, e->flag);
			}
		}
		break;
	case e_convert:
		l = exp_rename(sql, e->l, f, t);
		if (l)
			ne = exp_convert(sql->sa, l, exp_fromtype(e), exp_totype(e));
		break;
	case e_aggr:
	case e_func: {
		list *l = e->l, *nl = NULL;

		if (!l) {
			return e;
		} else {
			nl = exps_rename(sql, l, f, t);
			if (!nl)
				return NULL;
		}
		if (e->type == e_func)
			ne = exp_op(sql->sa, nl, e->f);
		else 
			ne = exp_aggr(sql->sa, nl, e->f, need_distinct(e), need_no_nil(e), e->card, has_nil(e));
		break;
	}	
	case e_atom:
	case e_psm:
		return e;
	}
	if (ne && e->p)
		ne->p = prop_copy(sql->sa, e->p);
	return ne;
}

/* push the expression down, ie translate colum references 
	from relation f into expression of relation t 
*/ 

static sql_exp * _exp_push_down(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t);

static list *
exps_push_down(mvc *sql, list *exps, sql_rel *f, sql_rel *t)
{
	node *n;
	list *nl = new_exp_list(sql->sa);

	for(n = exps->h; n; n = n->next) {
		sql_exp *arg = n->data, *narg = NULL;

		narg = _exp_push_down(sql, arg, f, t);
		if (!narg) 
			return NULL;
		if (arg->p)
			narg->p = prop_copy(sql->sa, arg->p);
		append(nl, narg);
	}
	return nl;
}

static sql_exp *
_exp_push_down(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t) 
{
	int flag = e->flag;
	sql_exp *ne = NULL, *l, *r, *r2;

	switch(e->type) {
	case e_column:
		if (is_union(f->op)) {
			int p = list_position(f->exps, rel_find_exp(f, e));

			return list_fetch(t->exps, p);
		}
		if (e->l) { 
			ne = rel_bind_column2(sql, f, e->l, e->r, 0);
			/* if relation name matches expressions relation name, find column based on column name alone */
		}
		if (!ne && !e->l)
			ne = rel_bind_column(sql, f, e->r, 0);
		if (!ne)
			return NULL;
		e = NULL;
		if (ne->name && ne->rname)
			e = rel_bind_column2(sql, t, ne->rname, ne->name, 0);
		if (!e && ne->name && !ne->rname)
			e = rel_bind_column(sql, t, ne->name, 0);
		if (!e && ne->name && ne->r && ne->l) 
			e = rel_bind_column2(sql, t, ne->l, ne->r, 0);
		if (!e && ne->r && !ne->l)
			e = rel_bind_column(sql, t, ne->r, 0);
		sql->session->status = 0;
		sql->errstr[0] = 0;
		if (e && flag)
			e->flag = flag;
		/* if the upper exp was an alias, keep this */ 
		if (e && ne->rname) 
			exp_setname(sql->sa, e, ne->rname, ne->name);
		return e;
	case e_cmp: 
		if (e->flag == cmp_or) {
			list *l = exps_push_down(sql, e->l, f, t);
			list *r = exps_push_down(sql, e->r, f, t);

			if (!l || !r) 
				return NULL;
			return exp_or(sql->sa, l, r);
		} else if (e->flag == cmp_in || e->flag == cmp_notin || get_cmp(e) == cmp_filter) {
			list *r;

			l = _exp_push_down(sql, e->l, f, t);
			r = exps_push_down(sql, e->r, f, t);
			if (!l || !r)
				return NULL;
			if (get_cmp(e) == cmp_filter) 
				return exp_filter(sql->sa, l, r, e->f, is_anti(e));
			return exp_in(sql->sa, l, r, e->flag);
		} else {
			l = _exp_push_down(sql, e->l, f, t);
			r = _exp_push_down(sql, e->r, f, t);
			if (e->f) {
				r2 = _exp_push_down(sql, e->f, f, t);
				if (l && r && r2)
					return exp_compare2(sql->sa, l, r, r2, e->flag);
			} else if (l && r) {
				if (l->card < r->card)
					return exp_compare(sql->sa, r, l, swap_compare((comp_type)e->flag));
				else
					return exp_compare(sql->sa, l, r, e->flag);
			}
		}
		return NULL;
	case e_convert:
		l = _exp_push_down(sql, e->l, f, t);
		if (l)
			return exp_convert(sql->sa, l, exp_fromtype(e), exp_totype(e));
		return NULL;
	case e_aggr:
	case e_func: {
		list *l = e->l, *nl = NULL;

		if (!l) {
			return e;
		} else {
			nl = exps_push_down(sql, l, f, t);
			if (!nl)
				return NULL;
		}
		if (e->type == e_func)
			return exp_op(sql->sa, nl, e->f);
		else 
			return exp_aggr(sql->sa, nl, e->f, need_distinct(e), need_no_nil(e), e->card, has_nil(e));
	}	
	case e_atom:
	case e_psm:
		return e;
	}
	return NULL;
}

static sql_exp *
exp_push_down(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t) 
{
	return _exp_push_down(sql, e, f, t);
}


/* some projections results are order dependend (row_number etc) */
static int 
project_unsafe(sql_rel *rel)
{
	sql_rel *sub = rel->l;
	node *n;

	if (need_distinct(rel) || rel->r /* order by */)
		return 1;
	if (!rel->exps)
		return 0;
	/* projects without sub and projects around ddl's cannot be changed */
	if (!sub || (sub && sub->op == op_ddl))
		return 1;
	for(n = rel->exps->h; n; n = n->next) {
		sql_exp *e = n->data;

		/* aggr func in project ! */
		if (e->type == e_func && e->card == CARD_AGGR)
			return 1;
	}
	return 0;
}

static int 
can_push_func(sql_exp *e, sql_rel *rel, int *must)
{
	if (!e)
		return 0;
	switch(e->type) {
	case e_cmp: {
		int mustl = 0, mustr = 0, mustf = 0;
		sql_exp *l = e->l, *r = e->r, *f = e->f;

		if (e->flag == cmp_or || e->flag == cmp_in || e->flag == cmp_notin || get_cmp(e) == cmp_filter) 
			return 0;
		return ((l->type == e_column || can_push_func(l, rel, &mustl)) && (*must = mustl)) || 
	               (!f && (r->type == e_column || can_push_func(r, rel, &mustr)) && (*must = mustr)) || 
		       (f && 
	               (r->type == e_column || can_push_func(r, rel, &mustr)) && 
		       (f->type == e_column || can_push_func(f, rel, &mustf)) && (*must = (mustr || mustf)));
	}
	case e_convert:
		return can_push_func(e->l, rel, must);
	case e_func: {
		list *l = e->l;
		node *n;
		int res = 1, lmust = 0;
		
		if (e->f){
			sql_subfunc *f = e->f;
			if (!f->func->s && !strcmp(f->func->base.name, "sql_div")) 
				return 0;
		}
		if (l) for (n = l->h; n && res; n = n->next)
			res &= can_push_func(n->data, rel, &lmust);
		if (!lmust)
			return 1;
		(*must) |= lmust;
		return res;
	}
	case e_column:
		if (rel && !rel_find_exp(rel, e)) 
			return 0;
		(*must) = 1;
	case e_atom:
	default:
		return 1;
	}
	return 0;
}

static int
exps_can_push_func(list *exps, sql_rel *rel) 
{
	node *n;

	for(n = exps->h; n; n = n->next) {
		sql_exp *e = n->data;
		int must = 0, mustl = 0, mustr = 0;

		if (is_join(rel->op) && ((can_push_func(e, rel->l, &mustl) && mustl) || (can_push_func(e, rel->r, &mustr) && mustr)))
			return 1;
		else if (is_select(rel->op) && can_push_func(e, NULL, &must) && must)
			return 1;
	}
	return 0;
}

/* (open) PROBLEM(S)
 * 
 * 1
 *  current always renaming gives problems with other expressions using expression 'ne' (union's
 *  sometimes share (correct or not) expressions on a shared referenced table).
 *
 *  not renaming gives problems with overloaded names (ie on the lower level an expression
 *  with the given name could already exist
 *
 * 2 
 *  creating projections for subqueries are empty, for now we just don't rewrite these.
 */
static sql_rel *
rel_push_func_down(int *changes, mvc *sql, sql_rel *rel) 
{
	if ((is_select(rel->op) || is_join(rel->op) || is_semi(rel->op)) && rel->l && rel->exps && !(rel_is_ref(rel))) {
		list *exps = rel->exps;

		if (exps_can_push_func(exps, rel)) {
			sql_rel *nrel;
			sql_rel *l = rel->l, *ol = l;
			sql_rel *r = rel->r, *or = r;
			node *n;

			/* we need a full projection, group by's and unions cannot be extended
 			 * with more expressions */
			if (l->op != op_project) { 
				if (is_subquery(l))
					return rel;
				rel->l = l = rel_project(sql->sa, l, 
					rel_projections(sql, l, NULL, 1, 1));
			}
			if (is_join(rel->op) && r->op != op_project) {
				if (is_subquery(r))
					return rel;
				rel->r = r = rel_project(sql->sa, r, 
					rel_projections(sql, r, NULL, 1, 1));
			}
 			nrel = rel_project(sql->sa, rel, rel_projections(sql, rel, NULL, 1, 1));
			for(n = exps->h; n; n = n->next) {
				sql_exp *e = n->data, *ne = NULL;
				int must = 0, mustl = 0, mustr = 0;

				if (e->type == e_column)
					continue;
				if ((is_join(rel->op) && ((can_push_func(e, l, &mustl) && mustl) || (can_push_func(e, r, &mustr) && mustr))) ||
				    (is_select(rel->op) && can_push_func(e, NULL, &must) && must)) {
					must = 0; mustl = 0; mustr = 0;
					if (e->type != e_cmp) { /* predicate */
						if ((is_join(rel->op) && ((can_push_func(e, l, &mustl) && mustl) || (can_push_func(e, r, &mustr) && mustr))) ||
					    	    (is_select(rel->op) && can_push_func(e, NULL, &must) && must)) {
							exp_label(sql->sa, e, ++sql->label);
							if (mustr)
								append(r->exps, e);
							else
								append(l->exps, e);
							e = exp_column(sql->sa, exp_relname(e), exp_name(e), exp_subtype(e), e->card, has_nil(e), is_intern(e));
							n->data = e;
							(*changes)++;
						}
					} else {
						ne = e->l;
						if ((is_join(rel->op) && ((can_push_func(ne, l, &mustl) && mustl) || (can_push_func(ne, r, &mustr) && mustr))) ||
					    	    (is_select(rel->op) && can_push_func(ne, NULL, &must) && must)) {
							exp_label(sql->sa, ne, ++sql->label);
							if (mustr)
								append(r->exps, ne);
							else
								append(l->exps, ne);
							ne = exp_column(sql->sa, NULL, exp_name(ne), exp_subtype(ne), ne->card, has_nil(ne), is_intern(ne));
							(*changes)++;
						}
						e->l = ne;

						must = 0; mustl = 0; mustr = 0;
						ne = e->r;
						if ((is_join(rel->op) && ((can_push_func(ne, l, &mustl) && mustl) || (can_push_func(ne, r, &mustr) && mustr))) ||
					    	    (is_select(rel->op) && can_push_func(ne, NULL, &must) && must)) {
							exp_label(sql->sa, ne, ++sql->label);
							if (mustr)
								append(r->exps, ne);
							else
								append(l->exps, ne);
							ne = exp_column(sql->sa, NULL, exp_name(ne), exp_subtype(ne), ne->card, has_nil(ne), is_intern(ne));
							(*changes)++;
						}
						e->r = ne;

						if (e->f) {
							must = 0; mustl = 0; mustr = 0;
							ne = e->f;
							if ((is_join(rel->op) && ((can_push_func(ne, l, &mustl) && mustl) || (can_push_func(ne, r, &mustr) && mustr))) ||
					            	    (is_select(rel->op) && can_push_func(ne, NULL, &must) && must)) {
								exp_label(sql->sa, ne, ++sql->label);
								if (mustr)
									append(r->exps, ne);
								else
									append(l->exps, ne);
								ne = exp_column(sql->sa, NULL, exp_name(ne), exp_subtype(ne), ne->card, has_nil(ne), is_intern(ne));
								(*changes)++;
							}
							e->f = ne;
						}
					}
				}
			}
			if (*changes) {
				rel = nrel;
			} else {
				if (l != ol)
					rel->l = ol;
				if (is_join(rel->op) && r != or)
					rel->r = or;
			}
		}
	}
	if (rel->op == op_project && rel->l && rel->exps) {
		sql_rel *pl = rel->l;

		if (is_join(pl->op) && exps_can_push_func(rel->exps, rel)) {
			node *n;
			sql_rel *l = pl->l, *r = pl->r;
			list *nexps;

			if (l->op != op_project) { 
				if (is_subquery(l))
					return rel;
				pl->l = l = rel_project(sql->sa, l, 
					rel_projections(sql, l, NULL, 1, 1));
			}
			if (is_join(rel->op) && r->op != op_project) {
				if (is_subquery(r))
					return rel;
				pl->r = r = rel_project(sql->sa, r, 
					rel_projections(sql, r, NULL, 1, 1));
			}
			nexps = new_exp_list(sql->sa);
			for ( n = rel->exps->h; n; n = n->next) {
				sql_exp *e = n->data;
				int mustl = 0, mustr = 0;

				if ((can_push_func(e, l, &mustl) && mustl) || 
				    (can_push_func(e, r, &mustr) && mustr)) {
					if (mustl)
						append(l->exps, e);
					else
						append(r->exps, e);
				} else
					append(nexps, e);
			}
			rel->exps = nexps;
			(*changes)++;
		}
	}
	return rel;
}


/*
 * Push Count inside crossjoin down, and multiply the results
 * 
 *     project (                                project(
 *          group by (                               crossproduct (
 *		crossproduct(                             project (
 *		     L,			 =>                    group by (
 *		     R                                              L
 *		) [ ] [ count NOT NULL ]                       ) [ ] [ count NOT NULL ]
 *          )                                             ),
 *     ) [ NOT NULL ]                                     project (
 *                                                              group by (
 *                                                                  R
 *                                                              ) [ ] [ count NOT NULL ]
 *                                                        )
 *                                                   ) [ sql_mul(.., .. NOT NULL) ]
 *                                              )
 */



static sql_rel *
rel_push_count_down(int *changes, mvc *sql, sql_rel *rel)
{
	sql_rel *r = rel->l;

	if (is_groupby(rel->op) && !rel_is_ref(rel) &&
            r && !r->exps && r->op == op_join && !(rel_is_ref(r)) &&
            ((sql_exp *) rel->exps->h->data)->type == e_aggr &&
            strcmp(((sql_subaggr *) ((sql_exp *) rel->exps->h->data)->f)->aggr->base.name, "count") == 0) {
/* TODO check for count(*) */
	    	sql_exp *nce, *oce;
		sql_rel *gbl, *gbr;		/* Group By */
		sql_rel *cp;			/* Cross Product */
		sql_subfunc *mult;
		list *args;
		char *rname = NULL, *name = NULL;
		sql_rel *srel;

		oce = rel->exps->h->data;
		if (oce->l) /* we only handle COUNT(*) */ 
			return rel;
		rname = oce->rname;
		name  = oce->name;

 		args = new_exp_list(sql->sa);
		srel = r->l;
		{
			sql_subaggr *cf = sql_bind_aggr(sql->sa, sql->session->schema, "count", NULL);
			sql_exp *cnt, *e = exp_aggr(sql->sa, NULL, cf, need_distinct(oce), need_no_nil(oce), oce->card, 0);

			exp_label(sql->sa, e, ++sql->label);
			cnt = exp_column(sql->sa, NULL, exp_name(e), exp_subtype(e), e->card, has_nil(e), is_intern(e));
			gbl = rel_groupby(sql, rel_dup(srel), NULL);
			rel_groupby_add_aggr(sql, gbl, e);
			append(args, cnt);
		}

		srel = r->r;
		{
			sql_subaggr *cf = sql_bind_aggr(sql->sa, sql->session->schema, "count", NULL);
			sql_exp *cnt, *e = exp_aggr(sql->sa, NULL, cf, need_distinct(oce), need_no_nil(oce), oce->card, 0);

			exp_label(sql->sa, e, ++sql->label);
			cnt = exp_column(sql->sa, NULL, exp_name(e), exp_subtype(e), e->card, has_nil(e), is_intern(e));
			gbr = rel_groupby(sql, rel_dup(srel), NULL);
			rel_groupby_add_aggr(sql, gbr, e);
			append(args, cnt);
		}

		mult = find_func(sql, "sql_mul", args);
		cp = rel_crossproduct(sql->sa, gbl, gbr, op_join);

		nce = exp_op(sql->sa, args, mult);
		exp_setname(sql->sa, nce, rname, name );

		rel_destroy(rel);
		rel = rel_project(sql->sa, cp, append(new_exp_list(sql->sa), nce));

		(*changes)++;
	}
	
	return rel;
}

/*
 * Push TopN (only LIMIT, no ORDER BY) down through projections underneath crossproduct, i.e.,
 *
 *     topn(                          topn(
 *         project(                       project(
 *             crossproduct(                  crossproduct(
 *                 L,           =>                topn( L )[ n ],
 *                 R                              topn( R )[ n ]
 *             )                              )
 *         )[ Cs ]*                       )[ Cs ]*
 *     )[ n ]                         )[ n ]
 *
 *  (TODO: in case of n==1 we can omit the original top-level TopN)
 *
 * also push topn under (non reordering) projections.
 */

static list *
sum_limit_offset(mvc *sql, list *exps )
{
	list *nexps = new_exp_list(sql->sa);
	node *n;
	sql_subtype *wrd = sql_bind_localtype("wrd");
	sql_subfunc *add;

	/* if the expression list only consists of a limit expression, 
	 * we copy it */
	if (list_length(exps) == 1 && exps->h->data)
		return append(nexps, exps->h->data);
	for (n = exps->h; n; n = n->next ) 
		nexps = append(nexps, n->data);
	add = sql_bind_func_result(sql->sa, sql->session->schema, "sql_add", wrd, wrd, wrd);
	return append(nexps, exp_op(sql->sa, exps, add));
}

static int 
topn_save_exps( list *exps )
{
	node *n;

	/* Limit only expression lists are always save */
	if (list_length(exps) == 1)
		return 1;
	for (n = exps->h; n; n = n->next ) {
		sql_exp *e = n->data;

		if (!e || e->type != e_atom) 
			return 0;
	}
	return 1;
}

static void
rel_no_rename_exps( list *exps )
{
	node *n;

	for (n = exps->h; n; n = n->next) {
		sql_exp *e = n->data;

		e->rname = e->l;
		e->name = e->r;
	}
}

static void
rel_rename_exps( mvc *sql, list *exps1, list *exps2)
{
	node *n, *m;

	assert(list_length(exps1) == list_length(exps2)); 
	for (n = exps1->h, m = exps2->h; n && m; n = n->next, m = m->next) {
		sql_exp *e1 = n->data;
		sql_exp *e2 = m->data;
		char *rname = e1->rname;

		if (!rname && e1->type == e_column && e1->l && e2->rname && 
		    strcmp(e1->l, e2->rname) == 0)
			rname = e2->rname;
		exp_setname(sql->sa, e2, rname, e1->name );
	}
}

static sql_rel *
rel_push_topn_down(int *changes, mvc *sql, sql_rel *rel) 
{
	sql_rel *rl, *r = rel->l;

	if (rel->op == op_topn && topn_save_exps(rel->exps)) {
		sql_rel *rp = NULL;

		/* duplicate topn + [ project-order ] under union */
		if (r)
			rp = r->l;
		if (r && r->exps && r->op == op_project && !(rel_is_ref(r)) && r->r && r->l &&
		    rp->op == op_union) {
			sql_rel *u = rp, *ou = u, *x;
			sql_rel *ul = u->l;
			sql_rel *ur = u->r;

			/* only push topn once */
			x = ul;
			while(x->op == op_project && x->l)
				x = x->l;
			if (x && x->op == op_topn)
				return rel;
			x = ur;
			while(x->op == op_project && x->l)
				x = x->l;
			if (x && x->op == op_topn)
				return rel;

			ul = rel_dup(ul);
			ur = rel_dup(ur);
			if (!is_project(ul->op)) 
				ul = rel_project(sql->sa, ul, 
					rel_projections(sql, ul, NULL, 1, 1));
			if (!is_project(ur->op)) 
				ur = rel_project(sql->sa, ur, 
					rel_projections(sql, ur, NULL, 1, 1));
			rel_rename_exps(sql, u->exps, ul->exps);
			rel_rename_exps(sql, u->exps, ur->exps);

			/* introduce projects under the set */
			ul = rel_project(sql->sa, ul, NULL);
			ul->exps = exps_copy(sql->sa, r->exps);
			ul->r = exps_copy(sql->sa, r->r);
			ul = rel_topn(sql->sa, ul, sum_limit_offset(sql, rel->exps));
			ur = rel_project(sql->sa, ur, NULL);
			ur->exps = exps_copy(sql->sa, r->exps);
			ur->r = exps_copy(sql->sa, r->r);
			ur = rel_topn(sql->sa, ur, sum_limit_offset(sql, rel->exps));
			u = rel_setop(sql->sa, ul, ur, op_union);
			u->exps = exps_copy(sql->sa, r->exps); 
			/* zap names */
			rel_no_rename_exps(u->exps);
			rel_destroy(ou);

			r->l = u;
			(*changes)++;
			return rel;
		}

		/* pass through projections */
		while (r && is_project(r->op) && !need_distinct(r) &&
			!(rel_is_ref(r)) &&
			!r->r && (rl = r->l) != NULL && is_project(rl->op)) {
			/* ensure there is no order by */
			if (!r->r) {
				r = r->l;
			} else {
				r = NULL;
			}
		}
		if (r && r != rel && r->op == op_project && !(rel_is_ref(r)) && !r->r && r->l) {
			r = rel_topn(sql->sa, r, sum_limit_offset(sql, rel->exps));
		}

		/* push topn under crossproduct */
		if (r && !r->exps && r->op == op_join && !(rel_is_ref(r)) &&
		    ((sql_rel *)r->l)->op != op_topn && ((sql_rel *)r->r)->op != op_topn) {
			r->l = rel_topn(sql->sa, r->l, sum_limit_offset(sql, rel->exps));
			r->r = rel_topn(sql->sa, r->r, sum_limit_offset(sql, rel->exps));
			(*changes)++;
			return rel;
		}
/* TODO */
#if 0
		/* duplicate topn + [ project-order ] under join on independend always matching joins */
		if (r)
			rp = r->l;
		if (r && r->exps && r->op == op_project && !(rel_is_ref(r)) && r->r && r->l &&
		    rp->op == op_join && rp->exps && rp->exps->h && ((prop*)((sql_exp*)rp->exps->h->data)->p)->kind == PROP_FETCH &&
		    ((sql_rel *)rp->l)->op != op_topn && ((sql_rel *)rp->r)->op != op_topn) {
			/* TODO check if order by columns are independend of join conditions */
			r->l = rel_topn(sql->sa, r->l, sum_limit_offset(sql, rel->exps));
			r->r = rel_topn(sql->sa, r->r, sum_limit_offset(sql, rel->exps));
			(*changes)++;
			return rel;
		}
#endif
	}
	return rel;
}

/* merge projection */

/* push an expression through a projection. 
 * The result should again used in a projection.
 */
static sql_exp *
exp_push_down_prj(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t);

static list *
exps_push_down_prj(mvc *sql, list *exps, sql_rel *f, sql_rel *t)
{
	node *n;
	list *nl = new_exp_list(sql->sa);

	for(n = exps->h; n; n = n->next) {
		sql_exp *arg = n->data, *narg = NULL;

		narg = exp_push_down_prj(sql, arg, f, t);
		if (!narg) 
			return NULL;
		if (arg->p)
			narg->p = prop_copy(sql->sa, arg->p);
		append(nl, narg);
	}
	return nl;
}

static sql_exp *
exp_push_down_prj(mvc *sql, sql_exp *e, sql_rel *f, sql_rel *t) 
{
	sql_exp *ne = NULL, *l, *r, *r2;

	assert(is_project(f->op));

	switch(e->type) {
	case e_column:
		if (e->l) 
			ne = exps_bind_column2(f->exps, e->l, e->r);
		if (!ne && !e->l)
			ne = exps_bind_column(f->exps, e->r, NULL);
		if (!ne || (ne->type != e_column && ne->type != e_atom))
			return NULL;
		while (ne && f->op == op_project && ne->type == e_column) {
			sql_exp *oe = e, *one = ne;

			e = ne;
			ne = NULL;
			if (e->l)
				ne = exps_bind_column2(f->exps, e->l, e->r);
			if (!ne && !e->l)
				ne = exps_bind_column(f->exps, e->r, NULL);
			if (!ne || ne == one) {
				ne = one;
				e = oe;
				break;
			}
			if (ne->type != e_column && ne->type != e_atom)
				return NULL;
		}
		/* possibly a groupby/project column is renamed */
		if (is_groupby(f->op) && f->r) {
			sql_exp *gbe = NULL;
			if (ne->l) 
				gbe = exps_bind_column2(f->r, ne->l, ne->r);
			if (!ne && !e->l)
				gbe = exps_bind_column(f->r, ne->r, NULL);
			ne = gbe;
			if (!ne || (ne->type != e_column && ne->type != e_atom))
				return NULL;
		}
		if (ne->type == e_atom) 
			e = exp_copy(sql->sa, ne);
		else
			e = exp_alias(sql->sa, e->rname, exp_name(e), ne->l, ne->r, exp_subtype(e), e->card, has_nil(e), is_intern(e));
		return e;
	case e_cmp: 
		if (e->flag == cmp_or) {
			list *l = exps_push_down_prj(sql, e->l, f, t);
			list *r = exps_push_down_prj(sql, e->r, f, t);

			if (!l || !r) 
				return NULL;
			return exp_or(sql->sa, l, r);
		} else if (e->flag == cmp_in || e->flag == cmp_notin || get_cmp(e) == cmp_filter) {
			sql_exp *l = exp_push_down_prj(sql, e->l, f, t);
			list *r = exps_push_down_prj(sql, e->r, f, t);

			if (!l || !r) 
				return NULL;
			if (get_cmp(e) == cmp_filter) 
				return exp_filter(sql->sa, l, r, e->f, is_anti(e));
			return exp_in(sql->sa, l, r, e->flag);
		} else {
			l = exp_push_down_prj(sql, e->l, f, t);
			r = exp_push_down_prj(sql, e->r, f, t);
			if (e->f) {
				r2 = exp_push_down_prj(sql, e->f, f, t);
				if (l && r && r2)
					return exp_compare2(sql->sa, l, r, r2, e->flag);
			} else if (l && r) {
				return exp_compare(sql->sa, l, r, e->flag);
			}
		}
		return NULL;
	case e_convert:
		l = exp_push_down_prj(sql, e->l, f, t);
		if (l)
			return exp_convert(sql->sa, l, exp_fromtype(e), exp_totype(e));
		return NULL;
	case e_aggr:
	case e_func: {
		list *l = e->l, *nl = NULL;

		if (!l) {
			return e;
		} else {
			nl = exps_push_down_prj(sql, l, f, t);
			if (!nl)
				return NULL;
		}
		if (e->type == e_func)
			return exp_op(sql->sa, nl, e->f);
		else 
			return exp_aggr(sql->sa, nl, e->f, need_distinct(e), need_no_nil(e), e->card, has_nil(e));
	}	
	case e_atom:
	case e_psm:
		return e;
	}
	return NULL;
}

static sql_rel *
rel_merge_projects(int *changes, mvc *sql, sql_rel *rel) 
{
	list *exps = rel->exps;
	sql_rel *prj = rel->l;
	node *n;

	if (rel->op == op_project && 
	    prj && prj->op == op_project && !(rel_is_ref(prj)) && !prj->r) {
		int all = 1;

		if (project_unsafe(rel) || project_unsafe(prj))
			return rel;

		/* here we need to fix aliases */
		rel->exps = new_exp_list(sql->sa); 

		/* for each exp check if we can rename it */
		for (n = exps->h; n && all; n = n->next) { 
			sql_exp *e = n->data, *ne = NULL;

			ne = exp_push_down_prj(sql, e, prj, prj->l);
			if (ne) {
				exp_setname(sql->sa, ne, exp_relname(e), exp_name(e));
				list_append(rel->exps, ne);
			} else {
				all = 0;
			}
		}
		if (all) {
			/* we can now remove the intermediate project */
			/* push order by expressions */
			if (rel->r) {
				list *nr = new_exp_list(sql->sa), *res = rel->r; 
				for (n = res->h; n; n = n->next) { 
					sql_exp *e = n->data, *ne = NULL;
	
					ne = exp_push_down_prj(sql, e, prj, prj->l);
					if (ne) {
						exp_setname(sql->sa, ne, exp_relname(e), exp_name(e));
						list_append(nr, ne);
					} else {
						all = 0;
					}
				}
				if (all) {
					rel->r = nr;
				} else {
					/* leave as is */
					rel->exps = exps;
					return rel;
				}
			}
			rel->l = prj->l;
			prj->l = NULL;
			rel_destroy(prj);
			(*changes)++;
			return rel_merge_projects(changes, sql, rel);
		} else {
			/* leave as is */
			rel->exps = exps;
		}
		return rel;
	}
	return rel;
}

static sql_subfunc *
find_func( mvc *sql, char *name, list *exps )
{
	list * l = new_func_list(sql->sa); 
	node *n;

	for(n = exps->h; n; n = n->next)
		append(l, exp_subtype(n->data)); 
	return sql_bind_func_(sql->sa, sql->session->schema, name, l, F_FUNC);
}

static sql_exp *
sql_div_fixup( mvc *sql, sql_exp *e, sql_exp *cond, int lr )
{
	list *args = e->l;
	sql_exp *le = args->h->data, *o;
	sql_exp *re = args->h->next->data;
	sql_subfunc *ifthen;

	/* if (cond) then val else const */
	args = new_exp_list(sql->sa);
	append(args, cond);
	if (!lr)
		append(args, re);
	o = exp_atom_wrd(sql->sa, 1);
	append(args, exp_convert(sql->sa, o, exp_subtype(o), exp_subtype(re)));
	if (lr)
		append(args, re);
	ifthen = find_func(sql, "ifthenelse", args);
	assert(ifthen);
	re = exp_op(sql->sa, args, ifthen);

	return exp_binop(sql->sa, le, re, e->f);
}

static list *
exps_case_fixup( mvc *sql, list *exps, sql_exp *cond, int lr )
{
	node *n;

	if (exps) {
		list *nexps = new_exp_list(sql->sa);
		for( n = exps->h; n; n = n->next) {
			sql_exp *e = n->data;
			if (is_func(e->type) && e->l && !is_rank_op(e) ) {
				sql_subfunc *f = e->f;

				if (!f->func->s && !strcmp(f->func->base.name, "sql_div")) {
					e = sql_div_fixup(sql, e, cond, lr);
				} else {
					list *l = exps_case_fixup(sql, e->l, cond, lr);
					sql_exp *ne = exp_op(sql->sa, l, f);
					exp_setname(sql->sa, ne, e->rname, e->name );
					e = ne;
				}
			}
			append(nexps, e);
		}
		return nexps;
	}
	return exps;
}

static sql_exp *
exp_case_fixup( mvc *sql, sql_exp *e )
{
	/* only functions need fix up */
	if (e->type == e_func && e->l && !is_rank_op(e) ) {
		list *l = new_exp_list(sql->sa), *args = e->l;
		node *n;
		sql_exp *ne;
		sql_subfunc *f = e->f;

		/* first fixup arguments */
		for (n=args->h; n; n=n->next) {
			sql_exp *a = exp_case_fixup(sql, n->data);
			list_append(l, a);
		}
		ne = exp_op(sql->sa, l, f);
		exp_setname(sql->sa, ne, e->rname, e->name );

		/* ifthenelse with one of the sides an 'sql_div' */
		args = ne->l;
		if (!f->func->s && !strcmp(f->func->base.name,"ifthenelse")) { 
			sql_exp *cond = args->h->data, *nne; 
			sql_exp *a1 = args->h->next->data; 
			sql_exp *a2 = args->h->next->next->data; 
			sql_subfunc *a1f = a1->f;
			sql_subfunc *a2f = a2->f;

			/* rewrite right hands of div */
			if (a1->type == e_func && !a1f->func->s && 
			     !strcmp(a1f->func->base.name, "sql_div")) {
				a1 = sql_div_fixup(sql, a1, cond, 0);
			} else if (a1->type == e_func && a1->l) { 
				a1->l = exps_case_fixup(sql, a1->l, cond, 0); 
			} else if (a1->type == e_convert) { 
				sql_exp *l = a1->l;
				sql_subfunc *f = l->f;

				if (l->type == e_func && !f->func->s && !strcmp(f->func->base.name, "sql_div")) 
					a1->l = sql_div_fixup(sql, l, cond, 0);
			}
			if  (a2->type == e_func && !a2f->func->s && 
			     !strcmp(a2f->func->base.name, "sql_div")) { 
				a2 = sql_div_fixup(sql, a2, cond, 1);
			} else if (a2->type == e_func && a2->l) { 
				a2->l = exps_case_fixup(sql, a2->l, cond, 1); 
			} else if (a2->type == e_convert) { 
				sql_exp *l = a2->l;
				sql_subfunc *f = l->f;

				if (l->type == e_func && !f->func->s && !strcmp(f->func->base.name, "sql_div")) 
					a2->l = sql_div_fixup(sql, l, cond, 1);
			}
			nne = exp_op3(sql->sa, cond, a1, a2, ne->f);
			exp_setname(sql->sa, nne, ne->rname, ne->name );
			ne = nne;
		}
		return ne;
	}
	if (e->type == e_convert) {
		sql_exp *e1 = exp_case_fixup(sql, e->l);
		sql_exp *ne = exp_convert(sql->sa, e1, exp_fromtype(e), exp_totype(e));
		exp_setname(sql->sa, ne, e->rname, e->name);
		return ne;
	} 
	if (e->type == e_aggr) {
		list *l = NULL, *args = e->l;
		node *n;
		sql_exp *ne;
		sql_subaggr *f = e->f;

		/* first fixup arguments */
		if (args) {
 			l = new_exp_list(sql->sa);
			for (n=args->h; n; n=n->next) {
				sql_exp *a = exp_case_fixup(sql, n->data);
				list_append(l, a);
			}
		}
		ne = exp_aggr(sql->sa, l, f, need_distinct(e), need_no_nil(e), e->card, has_nil(e));
		exp_setname(sql->sa, ne, e->rname, e->name );
		return ne;
	}
	return e;
}

static sql_rel *
rel_case_fixup(int *changes, mvc *sql, sql_rel *rel) 
{
	
	(void)changes; /* only go through it once, ie don't mark for changes */
	if (is_project(rel->op) && rel->exps) {
		list *exps = rel->exps;
		node *n;
		int needed = 0;

		for (n = exps->h; n && !needed; n = n->next) { 
			sql_exp *e = n->data;

			if (e->type == e_func || e->type == e_convert ||
			    e->type == e_aggr) 
				needed = 1;
		}
		if (!needed)
			return rel;

		rel->exps = new_exp_list(sql->sa); 
		for (n = exps->h; n; n = n->next) { 
			sql_exp *e = exp_case_fixup( sql, n->data );
		
			if (!e) 
				return NULL;
			list_append(rel->exps, e);
		}
	} 
	return rel;
}

static sql_rel *
rel_find_ref( sql_rel *r)
{
	while (!rel_is_ref(r) && r->l && 
	      (is_project(r->op) || is_select(r->op) /*|| is_join(r->op)*/))
		r = r->l;
	if (rel_is_ref(r))
		return r;
	return NULL;
}

static sql_rel *
rel_find_select( sql_rel *r)
{
	while (!is_select(r->op) && r->l && is_project(r->op))
		r = r->l;
	if (is_select(r->op))
		return r;
	return NULL;
}

static int
rel_match_projections(sql_rel *l, sql_rel *r)
{
	node *n, *m;
	list *le = l->exps;
	list *re = r->exps;

	if (!le || !re)
		return 0;
	if (list_length(le) != list_length(re))
		return 0;

	for (n = le->h, m = re->h; n && m; n = n->next, m = m->next) 
		if (!exp_match(n->data, m->data))
			return 0;
	return 1;
}

static int
exps_has_predicate( list *l )
{
	node *n;

	for( n = l->h; n; n = n->next){
		sql_exp *e = n->data;

		if (e->card <= CARD_ATOM)
			return 1;
	}
	return 0;
}

static sql_rel *
rel_merge_union(int *changes, mvc *sql, sql_rel *rel) 
{
	sql_rel *l = rel->l;
	sql_rel *r = rel->r;
	sql_rel *ref = NULL;

	if (is_union(rel->op) && 
	    l && is_project(l->op) && !project_unsafe(l) &&
	    r && is_project(r->op) && !project_unsafe(r) &&	
	    (ref = rel_find_ref(l)) != NULL && ref == rel_find_ref(r)) {
		/* Find selects and try to merge */
		sql_rel *ls = rel_find_select(l);
		sql_rel *rs = rel_find_select(r);
		
		/* can we merge ? */
		if (!ls || !rs) 
			return rel;

		/* merge any extra projects */
		if (l->l != ls) 
			rel->l = l = rel_merge_projects(changes, sql, l);
		if (r->l != rs) 
			rel->r = r = rel_merge_projects(changes, sql, r);
		
		if (!rel_match_projections(l,r)) 
			return rel;

		/* for now only union(project*(select(R),project*(select(R))) */
		if (ls != l->l || rs != r->l || 
		    ls->l != rs->l || !rel_is_ref(ls->l))
			return rel;

		if (!ls->exps || !rs->exps || 
		    exps_has_predicate(ls->exps) || 
		    exps_has_predicate(rs->exps))
			return rel;

		/* merge, ie. add 'or exp' */
		(*changes)++;
		ls->exps = append(new_exp_list(sql->sa), exp_or(sql->sa, ls->exps, rs->exps));
		rs->exps = NULL;
		rel = rel_inplace_project(sql->sa, rel, rel_dup(rel->l), rel->exps);
		set_processed(rel);
		return rel;
	}
	return rel;
}

static int
exps_cse( sql_allocator *sa, list *oexps, list *l, list *r )
{
	list *nexps;
	node *n, *m;
	char *lu, *ru;
	int lc = 0, rc = 0, match = 0, res = 0;

	/* first recusive exps_cse */
	nexps = new_exp_list(sa);
	for (n = l->h; n; n = n->next) {
		sql_exp *e = n->data;

		if (e->type == e_cmp && e->flag == cmp_or) {
			res = exps_cse(sa, nexps, e->l, e->r); 
		} else {
			append(nexps, e);
		}
	}
	l = nexps;

	nexps = new_exp_list(sa);
	for (n = r->h; n; n = n->next) {
		sql_exp *e = n->data;

		if (e->type == e_cmp && e->flag == cmp_or) {
			res = exps_cse(sa, nexps, e->l, e->r); 
		} else {
			append(nexps, e);
		}
	}
	r = nexps;

	lu = calloc(list_length(l), sizeof(char));
	ru = calloc(list_length(r), sizeof(char));
	for (n = l->h, lc = 0; n; n = n->next, lc++) {
		sql_exp *le = n->data;

		for ( m = r->h, rc = 0; m; m = m->next, rc++) {
			sql_exp *re = m->data;

			if (!ru[rc] && exp_match_exp(le,re)) {
				lu[lc] = 1;
				ru[rc] = 1;
				match = 1;
			}
		}
	}
	if (match) {
		list *nl = new_exp_list(sa);
		list *nr = new_exp_list(sa);

		for (n = l->h, lc = 0; n; n = n->next, lc++) 
			if (!lu[lc])
				append(nl, n->data);
		for (n = r->h, rc = 0; n; n = n->next, rc++) 
			if (!ru[rc])
				append(nr, n->data);

		if (list_length(nl) && list_length(nr)) 
			append(oexps, exp_or(sa, nl, nr)); 

		for (n = l->h, lc = 0; n; n = n->next, lc++) {
			if (lu[lc])
				append(oexps, n->data);
		}
		res = 1;
	} else {
		append(oexps, exp_or(sa, list_dup(l, (fdup)NULL), 
				     list_dup(r, (fdup)NULL)));
	}
	free(lu);
	free(ru);
	return res;
}

static int
are_equality_exps( list *exps, sql_exp **L) 
{
	sql_exp *l = *L;

	if (list_length(exps) == 1) {
		sql_exp *e = exps->h->data, *le = e->l, *re = e->r;

		if (e->type == e_cmp && e->flag == cmp_equal && le->card != CARD_ATOM && re->card == CARD_ATOM) {
			if (!l) {
				*L = l = le;
				if (!is_column(le->type))
					return 0;
			}
			return (exp_match(l, le));
		}
		if (e->type == e_cmp && e->flag == cmp_or)
			return (are_equality_exps(e->l, L) && 
				are_equality_exps(e->r, L));
	}
	return 0;
}

static void 
get_exps( list *n, list *l )
{
	sql_exp *e = l->h->data, *re = e->r;

	if (e->type == e_cmp && e->flag == cmp_equal && re->card == CARD_ATOM)
		list_append(n, re);
	if (e->type == e_cmp && e->flag == cmp_or) {
		get_exps(n, e->l);
		get_exps(n, e->r);
	}
}

static sql_exp *
equality_exps_2_in( mvc *sql, sql_exp *ce, list *l, list *r)
{
	list *nl = new_exp_list(sql->sa);

	get_exps(nl, l);
	get_exps(nl, r);

	return exp_in( sql->sa, ce, nl, cmp_in);
}

static sql_rel *
rel_select_cse(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)sql;
	if (is_select(rel->op) && rel->exps) { 
		node *n;
		list *nexps;
		int needed = 0;

		for (n=rel->exps->h; n && !needed; n = n->next) {
			sql_exp *e = n->data;

			if (e->type == e_cmp && e->flag == cmp_or) 
				needed = 1;
		}
		if (!needed)
			return rel;

 		nexps = new_exp_list(sql->sa);
		for (n=rel->exps->h; n; n = n->next) {
			sql_exp *e = n->data, *l = NULL;

			if (e->type == e_cmp && e->flag == cmp_or && are_equality_exps(e->l, &l) && are_equality_exps(e->r, &l) && l) {
				(*changes)++;
				append(nexps, equality_exps_2_in(sql, l, e->l, e->r));
			} else {
				append(nexps, e);
			}
		}
		rel->exps = nexps;
	}
	if (is_select(rel->op) && rel->exps) { 
		node *n;
		list *nexps;
		int needed = 0;

		for (n=rel->exps->h; n && !needed; n = n->next) {
			sql_exp *e = n->data;

			if (e->type == e_cmp && e->flag == cmp_or) 
				needed = 1;
		}
		if (!needed)
			return rel;
 		nexps = new_exp_list(sql->sa);
		for (n=rel->exps->h; n; n = n->next) {
			sql_exp *e = n->data;

			if (e->type == e_cmp && e->flag == cmp_or) {
				/* split the common expressions */
				*changes += exps_cse(sql->sa, nexps, e->l, e->r);
			} else {
				append(nexps, e);
			}
		}
		rel->exps = nexps;
	}
	return rel;
}

static sql_rel *
rel_project_cse(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)changes;
	if (is_project(rel->op) && rel->exps) { 
		node *n, *m;
		list *nexps;
		int needed = 0;

		for (n=rel->exps->h; n && !needed; n = n->next) {
			sql_exp *e1 = n->data;

			if (e1->type != e_column && e1->type != e_atom && e1->name) {
				for (m=n->next; m; m = m->next){
					sql_exp *e2 = m->data;
				
					if (exp_name(e2) && exp_match_exp(e1, e2)) 
						needed = 1;
				}
			}
		}

		if (!needed)
			return rel;

		nexps = new_exp_list(sql->sa);
		for (n=rel->exps->h; n; n = n->next) {
			sql_exp *e1 = n->data;

			if (e1->type != e_column && e1->type != e_atom && e1->name) {
				for (m=nexps->h; m; m = m->next){
					sql_exp *e2 = m->data;
				
					if (exp_name(e2) && exp_match_exp(e1, e2)) {
						sql_exp *ne = exp_alias(sql->sa, e1->rname, exp_name(e1), e2->rname, exp_name(e2), exp_subtype(e2), e2->card, has_nil(e2), is_intern(e1));
						e1 = ne;
						break;
					}
				}
			}
			append(nexps, e1);
		}
		rel->exps = nexps;
	}
	return rel;
}

static list *
exps_merge_rse( mvc *sql, list *l, list *r )
{
	node *n, *m, *o;
	list *nexps = NULL, *lexps, *rexps;

 	lexps = new_exp_list(sql->sa);
	for (n = l->h; n; n = n->next) {
		sql_exp *e = n->data;
	
		if (e->type == e_cmp && e->flag == cmp_or) {
			list *nexps = exps_merge_rse(sql, e->l, e->r);
			for (o = nexps->h; o; o = o->next) 
				append(lexps, o->data);
		} else {
			append(lexps, e);
		}
	}
 	rexps = new_exp_list(sql->sa);
	for (n = r->h; n; n = n->next) {
		sql_exp *e = n->data;
	
		if (e->type == e_cmp && e->flag == cmp_or) {
			list *nexps = exps_merge_rse(sql, e->l, e->r);
			for (o = nexps->h; o; o = o->next) 
				append(rexps, o->data);
		} else {
			append(rexps, e);
		}
	}

 	nexps = new_exp_list(sql->sa);

	/* merge merged lists first ? */
	for (n = lexps->h; n; n = n->next) {
		sql_exp *le = n->data, *re, *fnd = NULL;

		if (le->type != e_cmp || le->flag == cmp_or)
			continue;
		for (m = rexps->h; !fnd && m; m = m->next) {
			re = m->data;
			if (exps_match_col_exps(le, re))
				fnd = re;
		}
		/* cases
		 * 1) 2 values (cmp_equal)
		 * 2) 1 value (cmp_equal), and cmp_in 
		 * 	(also cmp_in, cmp_equal)
		 * 3) 2 cmp_in
		 * 4) ranges 
		 */
		if (fnd) {
			re = fnd;
			fnd = NULL;
			if (le->flag == cmp_equal && re->flag == cmp_equal) {
				list *exps = new_exp_list(sql->sa);

				append(exps, le->r);
				append(exps, re->r);
				fnd = exp_in(sql->sa, le->l, exps, cmp_in);
			} else if (le->flag == cmp_equal && re->flag == cmp_in){
				list *exps = new_exp_list(sql->sa);
				
				append(exps, le->r);
				list_merge(exps, re->r, NULL);
				fnd = exp_in(sql->sa, le->l, exps, cmp_in);
			} else if (le->flag == cmp_in && re->flag == cmp_equal){
				list *exps = new_exp_list(sql->sa);
				
				append(exps, re->r);
				list_merge(exps, le->r, NULL);
				fnd = exp_in(sql->sa, le->l, exps, cmp_in);
			} else if (le->flag == cmp_in && re->flag == cmp_in){
				list *exps = new_exp_list(sql->sa);

				list_merge(exps, le->r, NULL);
				list_merge(exps, re->r, NULL);
				fnd = exp_in(sql->sa, le->l, exps, cmp_in);
			} else if (le->f && re->f && /* merge ranges */
				   le->flag == re->flag && le->flag <= cmp_lt) {
				sql_subfunc *min = sql_bind_func(sql->sa, sql->session->schema, "sql_min", exp_subtype(le->r), exp_subtype(re->r), F_FUNC);
				sql_subfunc *max = sql_bind_func(sql->sa, sql->session->schema, "sql_max", exp_subtype(le->f), exp_subtype(re->f), F_FUNC);
				sql_exp *mine, *maxe;

				if (!min || !max)
					continue;
				mine = exp_binop(sql->sa, le->r, re->r, min);
				maxe = exp_binop(sql->sa, le->f, re->f, max);
				fnd = exp_compare2(sql->sa, le->l, mine, maxe, le->flag);
			}
			if (fnd)
				append(nexps, fnd);
		}
	}
	return nexps;
}


/* merge related sub expressions 
 * 
 * ie   (x = a and y > 1 and y < 5) or 
 *      (x = c and y > 1 and y < 10) or 
 *      (x = e and y > 1 and y < 20) 
 * ->
 *     ((x = a and y > 1 and y < 5) or 
 *      (x = c and y > 1 and y < 10) or 
 *      (x = e and y > 1 and y < 20)) and
 *     	 x in (a,c,e) and
 *     	 y > 1 and y < 20
 * */
static sql_rel *
rel_merge_rse(int *changes, mvc *sql, sql_rel *rel) 
{
	/* only execute once per select */
	(void)*changes;

	if (is_select(rel->op) && rel->exps) { 
		node *n, *o;
		list *nexps = new_exp_list(sql->sa);

		for (n=rel->exps->h; n; n = n->next) {
			sql_exp *e = n->data;

			if (e->type == e_cmp && e->flag == cmp_or) {
				/* possibly merge related expressions */
				list *ps = exps_merge_rse(sql, e->l, e->r);
				for (o = ps->h; o; o = o->next) 
					append(nexps, o->data);
			}
		}
		if (list_length(nexps))
		       for (o = nexps->h; o; o = o->next)
				append(rel->exps, o->data);
	}
	return rel;
}

/*
 * Rewrite aggregations over union all.
 *	groupby ([ union all (a, b) ], [gbe], [ count, sum ] )
 *
 * into
 * 	groupby ( [ union all( groupby( a, [gbe], [ count, sum] ), [ groupby( b, [gbe], [ count, sum] )) , [gbe], [sum, sum] ) 
 */
static sql_rel *
rel_push_aggr_down(int *changes, mvc *sql, sql_rel *rel) 
{
	if (rel->op == op_groupby && rel->l) {
		sql_rel *u = rel->l, *ou = u;
		sql_rel *g = rel;
		sql_rel *ul = u->l;
		sql_rel *ur = u->r;
		node *n, *m;
		list *lgbe = NULL, *rgbe = NULL, *gbe = NULL, *exps = NULL;

		if (u->op == op_project)
			u = u->l;

		if (!u || !is_union(u->op) || need_distinct(u) || !u->exps || rel_is_ref(u)) 
			return rel;

		ul = u->l;
		ur = u->r;

		/* make sure we don't create group by on group by's */
		if (ul->op == op_groupby || ur->op == op_groupby) 
			return rel;

		rel->subquery = 0;
		/* distinct should be done over the full result */
		for (n = g->exps->h; n; n = n->next) {
			sql_exp *e = n->data;
			sql_subaggr *af = e->f;

			if (e->type == e_atom || 
			    e->type == e_func || 
			   (e->type == e_aggr && 
			   ((strcmp(af->aggr->base.name, "sum") && 
			     strcmp(af->aggr->base.name, "count") &&
			     strcmp(af->aggr->base.name, "min") &&
			     strcmp(af->aggr->base.name, "max")) ||
			   need_distinct(e))))
				return rel; 
		}

		ul = rel_dup(ul);
		ur = rel_dup(ur);
		if (!is_project(ul->op)) 
			ul = rel_project(sql->sa, ul, 
				rel_projections(sql, ul, NULL, 1, 1));
		if (!is_project(ur->op)) 
			ur = rel_project(sql->sa, ur, 
				rel_projections(sql, ur, NULL, 1, 1));
		rel_rename_exps(sql, u->exps, ul->exps);
		rel_rename_exps(sql, u->exps, ur->exps);
		if (u != ou) {
			ul = rel_project(sql->sa, ul, NULL);
			ul->exps = exps_copy(sql->sa, ou->exps);
			rel_rename_exps(sql, ou->exps, ul->exps);
			ur = rel_project(sql->sa, ur, NULL);
			ur->exps = exps_copy(sql->sa, ou->exps);
			rel_rename_exps(sql, ou->exps, ur->exps);
		}	

		if (g->r && list_length(g->r) > 0) {
			list *gbe = g->r;

			lgbe = exps_copy(sql->sa, gbe);
			rgbe = exps_copy(sql->sa, gbe);
		}
		ul = rel_groupby(sql, ul, NULL);
		ul->r = lgbe;
		ul->nrcols = g->nrcols;
		ul->card = g->card;
		ul->exps = exps_copy(sql->sa, g->exps);

		ur = rel_groupby(sql, ur, NULL);
		ur->r = rgbe;
		ur->nrcols = g->nrcols;
		ur->card = g->card;
		ur->exps = exps_copy(sql->sa, g->exps);

		/* group by on primary keys which define the partioning scheme 
		 * don't need a finalizing group by */
		/* how to check if a partion is based on some primary key ? 
		 * */
		if (rel->r && list_length(rel->r)) {
			node *n;

			for (n = ((list*)rel->r)->h; n; n = n->next) {
				sql_exp *gbe = n->data;

				if (find_prop(gbe->p, PROP_HASHCOL)) {
					fcmp cmp = (fcmp)&kc_column_cmp;
					sql_column *c = exp_find_column(rel->l, gbe, -2);

					/* check if key is partition key */
					if (c && c->t->p && list_find(c->t->pkey->k.columns, c, cmp) != NULL) {
						(*changes)++;
						return rel_inplace_setop(rel, ul, ur, op_union,
					       	       rel_projections(sql, rel, NULL, 1, 1));
					}
				}
			}
		}

		u = rel_setop(sql->sa, ul, ur, op_union);
		u->exps = rel_projections(sql, rel, NULL, 1, 1);

		if (rel->r) {
			list *ogbe = rel->r;

			gbe = new_exp_list(sql->sa);
			for (n = ogbe->h; n; n = n->next) { 
				sql_exp *e = n->data, *ne;

				ne = list_find_exp( u->exps, e);
				assert(ne);
				ne = exp_column(sql->sa, exp_find_rel_name(ne), exp_name(ne), exp_subtype(ne), ne->card, has_nil(ne), is_intern(ne));
				append(gbe, ne);
			}
		}
		exps = new_exp_list(sql->sa); 
		for (n = u->exps->h, m = rel->exps->h; n && m; n = n->next, m = m->next) {
			sql_exp *ne, *e = n->data, *oa = m->data;

			if (oa->type == e_aggr) {
				sql_subaggr *f = oa->f;
				int cnt = strcmp(f->aggr->base.name,"count")==0;
				sql_subaggr *a = sql_bind_aggr(sql->sa, sql->session->schema, (cnt)?"sum":f->aggr->base.name, exp_subtype(e));

				assert(a);
				/* union of aggr result may have nils 
			   	 * because sum/count of empty set */
				set_has_nil(e);
				e = exp_column(sql->sa, exp_find_rel_name(e), exp_name(e), exp_subtype(e), e->card, has_nil(e), is_intern(e));
				ne = exp_aggr1(sql->sa, e, a, need_distinct(e), 1, e->card, 1);
			} else {
				ne = exp_column(sql->sa, exp_find_rel_name(e), exp_name(e), exp_subtype(e), e->card, has_nil(e), is_intern(e));
			}
			exp_setname(sql->sa, ne, exp_find_rel_name(oa), exp_name(oa));
			append(exps, ne);
		}
		(*changes)++;
		return rel_inplace_groupby( rel, u, gbe, exps);
	}
	return rel;
}

/*
 * Push select down, pushes the selects through (simple) projections. Also
 * it cleans up the projections which become useless.
 */

/* TODO push select expressions in outer joins down */
static sql_rel *
rel_push_select_down(int *changes, mvc *sql, sql_rel *rel) 
{
	list *exps = NULL;
	sql_rel *r = NULL;
	node *n;

	if (rel_is_ref(rel))
		return rel;

	/* don't make changes for empty selects */
	if (is_select(rel->op) && (!rel->exps || list_length(rel->exps) == 0)) 
		return rel;

	/* merge 2 selects */
	r = rel->l;
	if (is_select(rel->op) && r && is_select(r->op) && !(rel_is_ref(r))) {
		(void)list_merge(r->exps, rel->exps, (fdup)NULL);
		rel->l = NULL;
		rel_destroy(rel);
		(*changes)++;
		return rel_push_select_down(changes, sql, r);
	}
	/* 
	 * Push select through semi/anti join 
	 * 	select (semi(A,B)) == semi(select(A), B) 
	 */
	if (is_select(rel->op) && r && is_semi(r->op) && !(rel_is_ref(r))) {
		rel->l = r->l;
		r->l = rel;
		(*changes)++;
		/* 
		 * if A has 2 references (ie used on both sides of 
		 * the semi join), we also push the select into A. 
		 */
		if (rel_is_ref(rel->l) && rel->l == rel_find_ref(r->r)){
			sql_rel *lx = rel->l;
			sql_rel *rx = r->r;
			if (lx->ref.refcnt == 2 && !rel_is_ref(rx)) {
				while (rx->l && !rel_is_ref(rx->l) &&
	      			       (is_project(rx->op) || 
					is_select(rx->op) ||
					is_join(rx->op)))
						rx = rx->l;
				/* probably we need to introduce a project */
				rel_destroy(rel->l);
				lx = rel_project(sql->sa, rel, rel_projections(sql, rel, NULL, 1, 1));
				r->l = lx;
				rx->l = rel_dup(lx);
			}
		}
		return r;
	}
	exps = rel->exps;

	if (rel->op == op_project && 
	    r && r->op == op_project && !(rel_is_ref(r))) 
		return rel_merge_projects(changes, sql, rel);

	/* push select through join */
	if (is_select(rel->op) && r && is_join(r->op) && !(rel_is_ref(r))) {
		sql_rel *jl = r->l;
		sql_rel *jr = r->r;
		int left = r->op == op_join || r->op == op_left;
		int right = r->op == op_join || r->op == op_right;

		if (r->op == op_full)
			return rel;

		/* introduce selects under the join (if needed) */
		set_processed(jl);
		set_processed(jr);
		if (!is_select(jl->op)) 
			r->l = jl = rel_select(sql->sa, jl, NULL);
		if (!is_select(jr->op))
			r->r = jr = rel_select(sql->sa, jr, NULL);
		
		rel->exps = new_exp_list(sql->sa); 
		for (n = exps->h; n; n = n->next) { 
			sql_exp *e = n->data, *ne = NULL;
			int done = 0;
	
			if (left)
				ne = exp_push_down(sql, e, jl, jl);
			if (ne && ne != e) {
				done = 1; 
				rel_select_add_exp(jl, ne);
			} else if (right) {
				ne = exp_push_down(sql, e, jr, jr);
				if (ne && ne != e) {
					done = 1; 
					rel_select_add_exp(jr, ne);
				}
			}
			if (!done)
				append(rel->exps, e);
			*changes += done;
		}
		return rel;
	}

	/* merge select and cross product ? */
	if (is_select(rel->op) && r && r->op == op_join && !(rel_is_ref(r))) {
		list *exps = rel->exps;

		if (!r->exps)
			r->exps = new_exp_list(sql->sa); 
		rel->exps = new_exp_list(sql->sa); 
		for (n = exps->h; n; n = n->next) { 
			sql_exp *e = n->data;

			if (exp_is_join_exp(e) == 0) {
				append(r->exps, e);
				(*changes)++;
			} else {
				append(rel->exps, e);
			}
		}
		return rel;
	}

	if (is_select(rel->op) && r && r->op == op_project && !(rel_is_ref(r))){
		list *exps = rel->exps;
		sql_rel *pl;
		/* we cannot push through rank (row_number etc) functions or
		   projects with distinct */
		if (!r->l || project_unsafe(r))
			return rel;

		/* here we need to fix aliases */
		rel->exps = new_exp_list(sql->sa); 
		pl = r->l;
		/* introduce selects under the project (if needed) */
		set_processed(pl);
		if (!is_select(pl->op))
			r->l = pl = rel_select(sql->sa, pl, NULL);

		/* for each exp check if we can rename it */
		for (n = exps->h; n; n = n->next) { 
			sql_exp *e = n->data, *ne = NULL;

			/* sometimes we also have functions in the expression list (TODO change them to e_cmp (predicates like (1=0))) */
			if (e->type == e_cmp) {
				ne = exp_push_down_prj(sql, e, r, pl);

				/* can we move it down */
				if (ne && ne != e) {
					rel_select_add_exp(pl, ne);
					(*changes)++;
				} else {
					append(rel->exps, (ne)?ne:e);
				}
			} else {
				list_append(rel->exps, e);
			}
		}
		return rel;
	}
	return rel;
}

static sql_rel *
rel_push_select_down_join(int *changes, mvc *sql, sql_rel *rel) 
{
	list *exps = NULL;
	sql_rel *r = NULL;
	node *n;

	exps = rel->exps;
	r = rel->l;

	/* push select through join */
	if (is_select(rel->op) && r && r->op == op_join && !(rel_is_ref(r))) {
		rel->exps = new_exp_list(sql->sa); 
		for (n = exps->h; n; n = n->next) { 
			sql_exp *e = n->data;
			if (e->type == e_cmp && !e->f && !is_complex_exp(e->flag)) {
				sql_exp *re = e->r;

				if (re->card >= CARD_AGGR) {
					rel->l = rel_push_join(sql->sa, r, e->l, re, NULL, e);
				} else {
					rel->l = rel_push_select(sql->sa, r, e->l, e);
				}
				/* only pushed down selects are counted */
				if (r == rel->l) {
					(*changes)++;
				} else { /* Do not introduce an extra select */
					sql_rel *r = rel->l;

					rel->l = r->l;
					r->l = NULL;
					list_append(rel->exps, e);
					rel_destroy(r);
				}
				assert(r == rel->l);
			} else {
				list_append(rel->exps, e);
			} 
		}
		return rel;
	}
	return rel;
}

static sql_rel *
rel_remove_empty_select(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)sql;

	if ((is_join(rel->op) || is_semi(rel->op) || is_select(rel->op) || is_project(rel->op) || is_topn(rel->op) || is_sample(rel->op)) && rel->l) {
		sql_rel *l = rel->l;
		if (is_select(l->op) && !(rel_is_ref(l)) &&
		   (!l->exps || list_length(l->exps) == 0)) {
			rel->l = l->l;
			l->l = NULL;
			rel_destroy(l);
			(*changes)++;
		} 
	}
	if ((is_join(rel->op) || is_semi(rel->op) || is_set(rel->op)) && rel->r) {
		sql_rel *r = rel->r;
		if (is_select(r->op) && !(rel_is_ref(r)) &&
	   	   (!r->exps || list_length(r->exps) == 0)) {
			rel->r = r->l;
			r->l = NULL;
			rel_destroy(r);
			(*changes)++;
		}
	} 
	if (is_join(rel->op) && rel->exps && list_length(rel->exps) == 0) 
		rel->exps = NULL;
	return rel;
}

/*
 * Push {semi}joins down, pushes the joins through group by expressions. 
 * When the join is on the group by columns, we can push the joins left
 * under the group by. This should only be done, iff the new semijoin would
 * reduce the input table to the groupby. So there should be a reduction 
 * (selection) on the table A and this should be propagated to the groupby via
 * for example a primary key.
 *
 * {semi}join( A, groupby( B ) [gbe][aggrs] ) [ gbe == A.x ]
 * ->
 * {semi}join( A, groupby( semijoin(B,A) [gbe == A.x] ) [gbe][aggrs] ) [ gbe == A.x ]
 */

/* find in the list of expression an expression which uses e */ 
static sql_exp *
exp_uses_exp( list *exps, sql_exp *e)
{
	node *n;
	char *rname = exp_find_rel_name(e);
	char *name = exp_name(e);

	if (!exps)
		return NULL;

	for ( n = exps->h; n; n = n->next) {
		sql_exp *u = n->data;

		if (u->l && rname && strcmp(u->l, rname) == 0 &&
		    u->r && name && strcmp(u->r, name) == 0) 
			return u;
		if (!u->l && !rname &&
		    u->r && name && strcmp(u->r, name) == 0) 
			return u;
	}
	return NULL;
}

static sql_rel *
rel_push_join_down(int *changes, mvc *sql, sql_rel *rel) 
{
	list *exps = NULL;

	(void)*changes;
	if (!rel_is_ref(rel) && (((is_join(rel->op) && rel->exps) || is_semi(rel->op)) && rel->l)) {
		sql_rel *gb = rel->r, *ogb = gb, *l = NULL, *rell = rel->l;

		if (gb->op == op_project)
			gb = gb->l;

		if (is_basetable(rell->op) || rel_is_ref(rell))
			return rel;

		exps = rel->exps;
		if (gb && gb->op == op_groupby && gb->r && list_length(gb->r)) {
			list *jes = new_exp_list(sql->sa);
			node *n, *m;
			list *gbes = gb->r;
			/* find out if all group by expressions are used in the join */
			for(n = gbes->h; n; n = n->next) {
				sql_exp *gbe = n->data;
				int fnd = 0;
				char *rname = NULL, *name = NULL;

				/* project in between, ie find alias */
				/* first find expression in expression list */
				gbe = exp_uses_exp( gb->exps, gbe);
				if (!gbe)
					continue;
				if (ogb != gb) 
					gbe = exp_uses_exp( ogb->exps, gbe);
				if (gbe) {
					rname = exp_find_rel_name(gbe);
					name = exp_name(gbe);
				}

				for (m = exps->h; m && !fnd; m = m->next) {
					sql_exp *je = m->data;

					if (je->card >= CARD_ATOM && je->type == e_cmp && 
					    !is_complex_exp(je->flag)) {
						/* expect right expression to match */
						sql_exp *r = je->r;

						if (r == 0 || r->type != e_column)
							continue;
						if (r->l && rname && strcmp(r->l, rname) == 0 && strcmp(r->r, name)==0) {
							fnd = 1;
						} else if (!r->l && !rname  && strcmp(r->r, name)==0) {
							fnd = 1;
						}
						if (fnd) {
							sql_exp *le = je->l;
							sql_exp *re = exp_push_down_prj(sql, r, gb, gb->l);
							if (!re || (list_length(jes) == 0 && !find_prop(le->p, PROP_HASHCOL))) {
								fnd = 0;
							} else {
								je = exp_compare(sql->sa, le, re, je->flag);
								list_append(jes, je);
							}
						}
					}
				}
				if (!fnd) 
					return rel;
			}
			l = rel_dup(rel->l);

			/* push join's left side (as semijoin) down group by */
			l = gb->l = rel_crossproduct(sql->sa, gb->l, l, op_semi);
			l->exps = jes;
			return rel;
		} 
	}
	return rel;
}

/*
 * Push semijoins down, pushes the semijoin through a join. 
 *
 * semijoin( join(A, B) [ A.x == B.y ], C ) [ A.z == C.c ]
 * ->
 * join( semijoin(A, C) [ A.z == C.c ], B ) [ A.x == B.y ]
 */
#if 0
static sql_rel *
rel_push_semijoin_down(int *changes, mvc *sql, sql_rel *rel) 
{
	list *exps = NULL;

	(void)*changes;
	if (is_semi(rel->op) && rel->exps && rel->l) {
		sql_rel *l = rel->l, *ol = l, *ll = NULL, *lr = NULL;
		sql_rel *r = rel->r;
		list *exps = rel->exps, *nsexps, *njexps;

		/* handle project 
		if (l->op == op_project && !need_distinct(l))
			l = l->l;
		*/

		if (!is_join(l->op)  
			return rel;

		ll = l->l;
		lr = l->r;
		/* semijoin shouldn't be based on right relation of join */)
		for(n = exps->h; n; n = n->next) {
			sql_exp *sje = n->data;

			if (is_complex_exp(e->flag) || 
			    rel_has_exp(lr, sje->l) ||
			    rel_has_exp(lr, sje->r))
				return rel;
			}
		} 
		nsexps = sa_list(sql->sa);
		for(n = exps->h; n; n = n->next) {
			sql_exp *sje = n->data;
		} 
		njexps = sa_list(sql->sa);
		if (l->exps) {
			for(n = l->exps->h; n; n = n->next) {
				sql_exp *sje = n->data;
			} 
		}
		l = rel_crossproduce(sql->sa, ll, r, op_semi);
		l->exps = nsexps;
		rel = rel_inplace_join(rel, l, lr, op_join, njexps);
	}
	return rel;
}
#endif

static int
rel_is_join_on_pkey( sql_rel *rel ) 
{
	node *n;

	if (!rel || !rel->exps)
		return 0;
	for (n = rel->exps->h; n; n = n->next){
		sql_exp *je = n->data;

		if (je->type == e_cmp && je->flag == cmp_equal &&
		    find_prop(((sql_exp*)je->l)->p, PROP_HASHCOL)) { /* aligned PKEY JOIN */
			fcmp cmp = (fcmp)&kc_column_cmp;
			sql_exp *e = je->l;
			sql_column *c = exp_find_column(rel, e, -2);

			if (c && c->t->p && list_find(c->t->pkey->k.columns, c, cmp) != NULL) 
				return 1;
		}
	}
	return 0;
}

static int
rel_part_nr( sql_rel *rel, sql_exp *e )
{
	sql_column *c;
	sql_table *pp;
	assert(e->type == e_cmp);

	c = exp_find_column(rel, e->l, -1); 
	if (!c)
		c = exp_find_column(rel, e->r, -1); 
	if (!c)
		return -1;
	pp = c->t;
	if (pp->p)
		return list_position(pp->p->tables.set, pp);
	return -1;
}

static int
rel_uses_part_nr( sql_rel *rel, sql_exp *e, int pnr )
{
	sql_column *c;
	assert(e->type == e_cmp);

	/* 
	 * following case fails. 
	 *
	 * semijoin( A1, union [A1, A2] )
	 * The union will never return proper column (from A2).
	 * ie need different solution (probaly pass pnr).
	 */
	c = exp_find_column(rel, e->l, pnr); 
	if (!c)
		c = exp_find_column(rel, e->r, pnr); 
	if (c) {
		sql_table *pp = c->t;
		if (pp->p && list_position(pp->p->tables.set, pp) == pnr)
			return 1;
	}
	/* for projects we may need to do a rename! */
	if (is_project(rel->op) || is_topn(rel->op) || is_sample(rel->op))
		return rel_uses_part_nr( rel->l, e, pnr);

	if (is_union(rel->op) || is_join(rel->op) || is_semi(rel->op)) {
		if (rel_uses_part_nr( rel->l, e, pnr))
			return 1;
		if (!is_semi(rel->op) && rel_uses_part_nr( rel->r, e, pnr))
			return 1;
	}
	return 0;
}

/*
 * Push (semi)joins down unions, this is basically for merge tables, where
 * we know that the fk-indices are split over two clustered merge tables.
 */
static sql_rel *
rel_push_join_down_union(int *changes, mvc *sql, sql_rel *rel) 
{
	if (((is_join(rel->op) && !is_outerjoin(rel->op)) || is_semi(rel->op)) && rel->exps) {
		sql_rel *l = rel->l, *r = rel->r, *ol = l, *or = r;
		list *exps = rel->exps;
		sql_exp *je = exps->h->data;

		if (!l || !r || need_distinct(l) || need_distinct(r))
			return rel;
		if (l->op == op_project)
			l = l->l;
		if (r->op == op_project)
			r = r->l;

		/* both sides only if we have a join index */
		if (!l || !r ||(is_union(l->op) && is_union(r->op) && 
			!find_prop(je->p, PROP_JOINIDX) && /* FKEY JOIN */
			!rel_is_join_on_pkey(rel))) /* aligned PKEY JOIN */
			return rel;

		ol->subquery = or->subquery = 0;
		if ((is_union(l->op) && !need_distinct(l)) && !is_union(r->op)){
			sql_rel *nl, *nr;
			sql_rel *ll = rel_dup(l->l), *lr = rel_dup(l->r);

			/* join(union(a,b), c) -> union(join(a,c), join(b,c)) */
			if (!is_project(ll->op))
				ll = rel_project(sql->sa, ll, 
					rel_projections(sql, ll, NULL, 1, 1));
			if (!is_project(lr->op))
				lr = rel_project(sql->sa, lr, 
					rel_projections(sql, lr, NULL, 1, 1));
			rel_rename_exps(sql, l->exps, ll->exps);
			rel_rename_exps(sql, l->exps, lr->exps);
			if (l != ol) {
				ll = rel_project(sql->sa, ll, NULL);
				ll->exps = exps_copy(sql->sa, ol->exps);
				lr = rel_project(sql->sa, lr, NULL);
				lr->exps = exps_copy(sql->sa, ol->exps);
			}	
			nl = rel_crossproduct(sql->sa, ll, rel_dup(or), rel->op);
			nr = rel_crossproduct(sql->sa, lr, rel_dup(or), rel->op);
			nl->exps = exps_copy(sql->sa, exps);
			nr->exps = exps_copy(sql->sa, exps);
			(*changes)++;
			return rel_inplace_setop(rel, nl, nr, op_union, rel_projections(sql, rel, NULL, 1, 1));
		} else if (is_union(l->op) && !need_distinct(l) &&
			   is_union(r->op) && !need_distinct(r)) {
			sql_rel *nl, *nr;
			sql_rel *ll = rel_dup(l->l), *lr = rel_dup(l->r);
			sql_rel *rl = rel_dup(r->l), *rr = rel_dup(r->r);

			/* join(union(a,b), union(c,d)) -> union(join(a,c), join(b,d)) */
			if (!is_project(ll->op))
				ll = rel_project(sql->sa, ll, 
					rel_projections(sql, ll, NULL, 1, 1));
			if (!is_project(lr->op))
				lr = rel_project(sql->sa, lr, 
					rel_projections(sql, lr, NULL, 1, 1));
			rel_rename_exps(sql, l->exps, ll->exps);
			rel_rename_exps(sql, l->exps, lr->exps);
			if (l != ol) {
				ll = rel_project(sql->sa, ll, NULL);
				ll->exps = exps_copy(sql->sa, ol->exps);
				lr = rel_project(sql->sa, lr, NULL);
				lr->exps = exps_copy(sql->sa, ol->exps);
			}	
			if (!is_project(rl->op))
				rl = rel_project(sql->sa, rl, 
					rel_projections(sql, rl, NULL, 1, 1));
			if (!is_project(rr->op))
				rr = rel_project(sql->sa, rr, 
					rel_projections(sql, rr, NULL, 1, 1));
			rel_rename_exps(sql, r->exps, rl->exps);
			rel_rename_exps(sql, r->exps, rr->exps);
			if (r != or) {
				rl = rel_project(sql->sa, rl, NULL);
				rl->exps = exps_copy(sql->sa, or->exps);
				rr = rel_project(sql->sa, rr, NULL);
				rr->exps = exps_copy(sql->sa, or->exps);
			}	
			nl = rel_crossproduct(sql->sa, ll, rl, rel->op);
			nr = rel_crossproduct(sql->sa, lr, rr, rel->op);
			nl->exps = exps_copy(sql->sa, exps);
			nr->exps = exps_copy(sql->sa, exps);

			(*changes)++;
			return rel_inplace_setop(rel, nl, nr, op_union, rel_projections(sql, rel, NULL, 1, 1));
		} else if (!is_union(l->op) && 
			   is_union(r->op) && !need_distinct(r) &&
			   !is_semi(rel->op)) {
			sql_rel *nl, *nr;
			sql_rel *rl = rel_dup(r->l), *rr = rel_dup(r->r);

			/* join(a, union(b,c)) -> union(join(a,b), join(a,c)) */
			if (!is_project(rl->op))
				rl = rel_project(sql->sa, rl, 
					rel_projections(sql, rl, NULL, 1, 1));
			if (!is_project(rr->op))
				rr = rel_project(sql->sa, rr, 
					rel_projections(sql, rr, NULL, 1, 1));
			rel_rename_exps(sql, r->exps, rl->exps);
			rel_rename_exps(sql, r->exps, rr->exps);
			if (r != or) {
				rl = rel_project(sql->sa, rl, NULL);
				rl->exps = exps_copy(sql->sa, or->exps);
				rr = rel_project(sql->sa, rr, NULL);
				rr->exps = exps_copy(sql->sa, or->exps);
			}	
			nl = rel_crossproduct(sql->sa, rel_dup(ol), rl, rel->op);
			nr = rel_crossproduct(sql->sa, rel_dup(ol), rr, rel->op);
			nl->exps = exps_copy(sql->sa, exps);
			nr->exps = exps_copy(sql->sa, exps);

			(*changes)++;
			return rel_inplace_setop(rel, nl, nr, op_union, rel_projections(sql, rel, NULL, 1, 1));
		/* {semi}join ( A1, union (A2, B)) [A1.partkey = A2.partkey] ->
		 * {semi}join ( A1, A2 ) 
		 * and
		 * {semi}join ( A1, union (B, A2)) [A1.partkey = A2.partkey] ->
		 * {semi}join ( A1, A2 ) 
		 * (ie a single part on the left)
		 *
		 * Howto detect that a relation isn't matching.
		 *
		 * partitioning is currently done only on pkey/fkey's
		 * ie only matching per part if join is on pkey/fkey (parts)
		 *
		 * and part numbers should match.
		 *
		 * */
		} else if (!is_union(l->op) && 
			   is_union(r->op) && !need_distinct(r) &&
			   is_semi(rel->op) && rel_is_join_on_pkey(rel)) {
			/* use first join expression, to find part nr */
			sql_exp *je = rel->exps->h->data;
			int lpnr = rel_part_nr(l, je);
			sql_rel *rl = r->l;
			sql_rel *rr = r->r;

			if (lpnr < 0)
				return rel;
			/* case 1: uses left not right */
			if (rel_uses_part_nr(rl, je, lpnr) && 
			   !rel_uses_part_nr(rr, je, lpnr)) {
				sql_rel *nl;

				rl = rel_dup(rl);
				if (!is_project(rl->op))
					rl = rel_project(sql->sa, rl, 
					rel_projections(sql, rl, NULL, 1, 1));
				rel_rename_exps(sql, r->exps, rl->exps);
				if (r != or) {
					rl = rel_project(sql->sa, rl, NULL);
					rl->exps = exps_copy(sql->sa, or->exps);
				}	
				nl = rel_crossproduct(sql->sa, rel_dup(ol), rl, rel->op);
				nl->exps = exps_copy(sql->sa, exps);
				(*changes)++;
				return rel_inplace_project(sql->sa, rel, nl, rel_projections(sql, rel, NULL, 1, 1));
			/* case 2: uses right not left */
			} else if (!rel_uses_part_nr(rl, je, lpnr) && 
				    rel_uses_part_nr(rr, je, lpnr)) {
				sql_rel *nl;

				rr = rel_dup(rr);
				if (!is_project(rr->op))
					rr = rel_project(sql->sa, rr, 
						rel_projections(sql, rr, NULL, 1, 1));
				rel_rename_exps(sql, r->exps, rr->exps);
				if (r != or) {
					rr = rel_project(sql->sa, rr, NULL);
					rr->exps = exps_copy(sql->sa, or->exps);
				}	
				nl = rel_crossproduct(sql->sa, rel_dup(ol), rr, rel->op);
				nl->exps = exps_copy(sql->sa, exps);
				(*changes)++;
				return rel_inplace_project(sql->sa, rel, nl, rel_projections(sql, rel, NULL, 1, 1));
			}
		}
	}
	return rel;
}

static int 
rel_is_empty( sql_rel *rel )
{
	(void)rel;
	if ((is_join(rel->op) || is_semi(rel->op)) && rel->exps) {
		sql_rel *l = rel->l, *r = rel->r;

		if (rel_is_empty(l) || (is_join(rel->op) && rel_is_empty(r)))
			return 1;
		/* check */
		if (rel_is_join_on_pkey(rel)) {
			sql_exp *je = rel->exps->h->data;
			int lpnr = rel_part_nr(l, je);

			if (lpnr >= 0 && !rel_uses_part_nr(r, je, lpnr))
				return 1;
		}
	}
	if (!is_union(rel->op) && 
			(is_project(rel->op) || is_topn(rel->op)) && rel->l)
		return rel_is_empty(rel->l);
	return 0;
}

/* non overlapping partitions should be removed */
static sql_rel *
rel_remove_empty_join(mvc *sql, sql_rel *rel, int *changes) 
{
	/* recurse check rel_is_empty 
	 * For half empty unions replace by projects
	 * */
	if (is_union(rel->op)) {
		sql_rel *l = rel->l, *r = rel->r;

		rel->l = l = rel_remove_empty_join(sql, l, changes);
		rel->r = r = rel_remove_empty_join(sql, r, changes);
		if (rel_is_empty(l)) {
			(*changes)++;
			return rel_inplace_project(sql->sa, rel, rel_dup(r), rel->exps);
		} else if (rel_is_empty(r)) {
			(*changes)++;
			return rel_inplace_project(sql->sa, rel, rel_dup(l), rel->exps);
		}
	} else if ((is_project(rel->op) || is_topn(rel->op) || is_select(rel->op)
				|| is_sample(rel->op)) && rel->l) {
		rel->l = rel_remove_empty_join(sql, rel->l, changes);
	} else if (is_join(rel->op)) {
		rel->l = rel_remove_empty_join(sql, rel->l, changes);
		rel->r = rel_remove_empty_join(sql, rel->r, changes);
	}
	return rel;
}

static sql_rel *
rel_push_select_down_union(int *changes, mvc *sql, sql_rel *rel) 
{
	if (is_select(rel->op) && rel->l && rel->exps) {
		sql_rel *u = rel->l, *ou = u;
		sql_rel *s = rel;
		sql_rel *ul = u->l;
		sql_rel *ur = u->r;

		if (u->op == op_project)
			u = u->l;

		if (!u || !is_union(u->op) || need_distinct(u) || !u->exps || rel_is_ref(u))
			return rel;

		ul = u->l;
		ur = u->r;

		rel->subquery = 0;
		u->subquery = 0;
		ul->subquery = 0;
		ur->subquery = 0;
		ul = rel_dup(ul);
		ur = rel_dup(ur);
		if (!is_project(ul->op)) 
			ul = rel_project(sql->sa, ul, 
				rel_projections(sql, ul, NULL, 1, 1));
		if (!is_project(ur->op)) 
			ur = rel_project(sql->sa, ur, 
				rel_projections(sql, ur, NULL, 1, 1));
		rel_rename_exps(sql, u->exps, ul->exps);
		rel_rename_exps(sql, u->exps, ur->exps);

		if (u != ou) {
			ul = rel_project(sql->sa, ul, NULL);
			ul->exps = exps_copy(sql->sa, ou->exps);
			rel_rename_exps(sql, ou->exps, ul->exps);
			ur = rel_project(sql->sa, ur, NULL);
			ur->exps = exps_copy(sql->sa, ou->exps);
			rel_rename_exps(sql, ou->exps, ur->exps);
		}	

		/* introduce selects under the set (if needed) */
		set_processed(ul);
		set_processed(ur);
		ul = rel_select(sql->sa, ul, NULL);
		ur = rel_select(sql->sa, ur, NULL);
		
		ul->exps = exps_copy(sql->sa, s->exps);
		ur->exps = exps_copy(sql->sa, s->exps);

		rel = rel_inplace_setop(rel, ul, ur, op_union, rel_projections(sql, rel, NULL, 1, 1));
		(*changes)++;
		return rel;
	}
	return rel;
}

static int
exps_unique( list *exps )
{
	node *n;

	if ((n = exps->h) != NULL) {
		sql_exp *e = n->data;
		prop *p;

		/* TODO, check if ukey is complete */
		if (e && (p = find_prop(e->p, PROP_HASHCOL)) != NULL) {
			sql_ukey *k = p->value;
			if (list_length(k->k.columns) <= 1)
				return 1;
		}
	}
	return 0;
}

static sql_rel *
rel_push_project_down_union(int *changes, mvc *sql, sql_rel *rel) 
{
	/* first remove distinct if allready unique */
	if (rel->op == op_project && need_distinct(rel) && rel->exps && exps_unique(rel->exps))
		set_nodistinct(rel);

	if (rel->op == op_project && rel->l && rel->exps && !rel->r && !project_unsafe(rel)) {
		int need_distinct = need_distinct(rel);
		sql_rel *u = rel->l;
		sql_rel *p = rel;
		sql_rel *ul = u->l;
		sql_rel *ur = u->r;

		if (!u || !is_union(u->op) || need_distinct(u) || !u->exps || rel_is_ref(u))
			return rel;
		/* don't push project down union of single values */
		if ((is_project(ul->op) && !ul->l) || (is_project(ur->op) && !ur->l))
			return rel;

		rel->subquery = 0;
		u->subquery = 0;
		ul = rel_dup(ul);
		ur = rel_dup(ur);

		if (!is_project(ul->op)) 
			ul = rel_project(sql->sa, ul, 
				rel_projections(sql, ul, NULL, 1, 1));
		if (!is_project(ur->op)) 
			ur = rel_project(sql->sa, ur, 
				rel_projections(sql, ur, NULL, 1, 1));
		need_distinct = (need_distinct && 
				(!exps_unique(ul->exps) ||
				 !exps_unique(ur->exps)));
		rel_rename_exps(sql, u->exps, ul->exps);
		rel_rename_exps(sql, u->exps, ur->exps);

		/* introduce projects under the set */
		ul = rel_project(sql->sa, ul, NULL);
		if (need_distinct)
			set_distinct(ul);
		ur = rel_project(sql->sa, ur, NULL);
		if (need_distinct)
			set_distinct(ur);
		
		ul->exps = exps_copy(sql->sa, p->exps);
		ur->exps = exps_copy(sql->sa, p->exps);

		rel = rel_inplace_setop(rel, ul, ur, op_union,
			rel_projections(sql, rel, NULL, 1, 1));
		if (need_distinct)
			set_distinct(rel);
		(*changes)++;
		return rel;
	}
	return rel;
}

/* Compute the efficiency of using this expression early in a group by list */
static int
score_gbe( mvc *sql, sql_rel *rel, sql_exp *e)
{
	int res = 10;
	sql_subtype *t = exp_subtype(e);
	sql_column *c = NULL;

	/* can we find out if the underlying table is sorted */
	if ( (c = exp_find_column(rel, e, -2)) != NULL) {
		if (mvc_is_sorted (sql, c)) 
			res += 500;
	}

	/* is the column selective */

	/* prefer the shorter var types over the longer onces */
	if (!EC_FIXED(t->type->eclass) && t->digits)
		res -= t->digits;
	/* smallest type first */
	if (EC_FIXED(t->type->eclass))
		res -= t->type->eclass; 
	return res;
}

/* reorder group by expressions */
static sql_rel *
rel_groupby_order(int *changes, mvc *sql, sql_rel *rel) 
{
	list *gbe = rel->r;

	(void)*changes;
	if (is_groupby(rel->op) && list_length(gbe) > 1 && list_length(gbe)<9) {
		node *n;
		int i, *scores = calloc(list_length(gbe), sizeof(int));

		for (i = 0, n = gbe->h; n; i++, n = n->next) 
			scores[i] = score_gbe(sql, rel, n->data);
		rel->r = list_keysort(gbe, scores, (fdup)NULL);
		free(scores);
	}
	return rel;
}


/* reduce group by expressions based on pkey info 
 *
 * The reduced group by and aggr expressions are restored via
 * a join with the base table (ie which is similar to late projection).
 */

static sql_rel *
rel_reduce_groupby_exps(int *changes, mvc *sql, sql_rel *rel) 
{
	list *gbe = rel->r;

	if (is_groupby(rel->op) && rel->r && !rel_is_ref(rel)) {
		node *n, *m;
		signed char *scores = malloc(list_length(gbe));
		int k, j, i;
		sql_column *c;
		sql_table **tbls;
		sql_rel **bts, *bt = NULL;

		gbe = rel->r;
		tbls = (sql_table**)malloc(sizeof(sql_table*)*list_length(gbe));
		bts = (sql_rel**)malloc(sizeof(sql_rel*)*list_length(gbe));
		for (k = 0, i = 0, n = gbe->h; n; n = n->next, k++) {
			sql_exp *e = n->data;

			c = exp_find_column_(rel, e, -2, &bt);
			if (c) {
				for(j = 0; j < i; j++)
					if (c->t == tbls[j] && bts[j] == bt)
						break;
				tbls[j] = c->t;
				bts[j] = bt;
				i += (j == i);
			}
		}
		if (i) { /* forall tables find pkey and 
				remove useless other columns */
			for(j = 0; j < i; j++) {
				int l, nr = 0, cnr = 0;

				k = list_length(gbe);
				memset(scores, 0, list_length(gbe));
				if (tbls[j]->pkey) {
					for (l = 0, n = gbe->h; l < k && n; l++, n = n->next) {
						fcmp cmp = (fcmp)&kc_column_cmp;
						sql_exp *e = n->data;

						c = exp_find_column_(rel, e, -2, &bt);
						if (c && c->t == tbls[j] && bts[j] == bt &&  
						    list_find(tbls[j]->pkey->k.columns, c, cmp) != NULL) {
							scores[l] = 1;
							nr ++;
						} else if (c && c->t == tbls[j] && bts[j] == bt) { 
							/* Okay we can cleanup a group by column */
							scores[l] = -1;
							cnr ++;
						}
					}
				}
				if (nr) { 
					int all = (list_length(tbls[j]->pkey->k.columns) == nr); 
					sql_kc *kc = tbls[j]->pkey->k.columns->h->data; 

					c = kc->c;
					for (l = 0, n = gbe->h; l < k && n; l++, n = n->next) {
						sql_exp *e = n->data;

						/* pkey based group by */
						if (scores[l] == 1 && ((all || 
						   /* first of key */
						   (c == exp_find_column(rel, e, -2))) && !find_prop(e->p, PROP_HASHCOL)))
							e->p = prop_create(sql->sa, PROP_HASHCOL, e->p);
					}
					for (m = rel->exps->h; m; m = m->next ){
						sql_exp *e = m->data;

						for (l = 0, n = gbe->h; l < k && n; l++, n = n->next) {
							sql_exp *gb = n->data;

							/* pkey based group by */
							if (scores[l] == 1 && exp_match_exp(e,gb) && find_prop(gb->p, PROP_HASHCOL) && !find_prop(e->p, PROP_HASHCOL)) {
								e->p = prop_create(sql->sa, PROP_HASHCOL, e->p);
								break;
							}

						}
					}
				}
				if (cnr && nr && list_length(tbls[j]->pkey->k.columns) == nr) {
					char rname[16], *rnme = number2name(rname, 16, ++sql->label);
					sql_rel *r = rel_basetable(sql, tbls[j], rnme);
					list *ngbe = new_exp_list(sql->sa);
					list *exps = rel->exps, *nexps = new_exp_list(sql->sa);
					list *lpje = new_exp_list(sql->sa);

					r = rel_project(sql->sa, r, new_exp_list(sql->sa));
					for (l = 0, n = gbe->h; l < k && n; l++, n = n->next) {
						sql_exp *e = n->data;

						/* keep the group by columns which form a primary key
						 * of this table. And those unrelated to this table. */
						if (scores[l] != -1) 
							append(ngbe, e); 
						/* primary key's are used in the late projection */
						if (scores[l] == 1) {
							sql_column *c = exp_find_column_(rel, e, -2, &bt);
							
							sql_exp *rs = exp_column(sql->sa, rnme, c->base.name, exp_subtype(e), rel->card, has_nil(e), is_intern(e));
							append(r->exps, rs);

							e = exp_compare(sql->sa, e, rs, cmp_equal);
							append(lpje, e);

							e->p = prop_create(sql->sa, PROP_FETCH, e->p);
						}
					}
					rel->r = ngbe;
					/* remove gbe also from aggr list */
					for (m = exps->h; m; m = m->next ){
						sql_exp *e = m->data;
						int fnd = 0;

						for (l = 0, n = gbe->h; l < k && n && !fnd; l++, n = n->next) {
							sql_exp *gb = n->data;

							if (scores[l] == -1 && exp_match_exp(e,gb)) {
								sql_column *c = exp_find_column_(rel, e, -2, &bt);
								sql_exp *rs = exp_column(sql->sa, rnme, c->base.name, exp_subtype(e), rel->card, has_nil(e), is_intern(e));
								exp_setname(sql->sa, rs, exp_find_rel_name(e), exp_name(e));
								append(r->exps, rs);
								fnd = 1;
							}
						}
						if (!fnd)
							append(nexps, e);
					}
					/* new reduce aggr expression list */
					rel->exps = nexps;
					rel = rel_crossproduct(sql->sa, rel, r, op_join);
					rel->exps = lpje;
					/* only one reduction at a time */
					*changes = 1;
					free(bts);
					free(tbls);
					free(scores);
					return rel;
				} 
				gbe = rel->r;
			}
		}
		free(bts);
		free(tbls);
		free(scores);
	}
	return rel;
}

static sql_exp *split_aggr_and_project(mvc *sql, list *aexps, sql_exp *e);

static void
list_split_aggr_and_project(mvc *sql, list *aexps, list *exps)
{
	node *n;

	if (!exps)
		return ;
	for(n = exps->h; n; n = n->next) 
		n->data = split_aggr_and_project(sql, aexps, n->data);
}

static sql_exp *
split_aggr_and_project(mvc *sql, list *aexps, sql_exp *e)
{
	switch(e->type) {
	case e_aggr:
		/* add to the aggrs */
		if (!exp_name(e)) {
			exp_label(sql->sa, e, ++sql->label);
			e->rname = e->name;
		}
		list_append(aexps, e);
		return exp_column(sql->sa, exp_find_rel_name(e), exp_name(e), exp_subtype(e), e->card, has_nil(e), is_intern(e));
	case e_cmp:
		/* e_cmp's shouldn't exist in an aggr expression list */
		assert(0);
	case e_convert:
		e->l = split_aggr_and_project(sql, aexps, e->l);
		return e;
	case e_func: 
		list_split_aggr_and_project(sql, aexps, e->l);
	case e_column: /* constants and columns shouldn't be rewriten */
	case e_atom:
	case e_psm:
		return e;
	}
	return NULL;
}

/* Pushing projects up the tree. Done very early in the optimizer.
 * Makes later steps easier. 
 */
static sql_rel *
rel_push_project_up(int *changes, mvc *sql, sql_rel *rel) 
{
	/* project/project cleanup is done later */
	if (is_join(rel->op) || is_select(rel->op)) {
		node *n;
		list *exps = NULL, *l_exps, *r_exps;
		sql_rel *l = rel->l;
		sql_rel *r = rel->r;
		sql_rel *t;

		/* Don't rewrite refs, non projections or constant or 
		   order by projections  */
		if (!l || rel_is_ref(l) || 
		   (is_join(rel->op) && (!r || rel_is_ref(r))) ||
		   (is_select(rel->op) && l->op != op_project) ||
		   (is_join(rel->op) && l->op != op_project && r->op != op_project) ||
		  ((l->op == op_project && (!l->l || l->r || project_unsafe(l))) ||
		   (is_join(rel->op) && (is_subquery(r) ||
		    (r->op == op_project && (!r->l || r->r || project_unsafe(r))))))) 
			return rel;

		if (l->op == op_project && l->l) {
			/* Go through the list of project expressions.
			   Check if they can be pushed up, ie are they not
			   changing or introducing any columns used
			   by the upper operator. */

			exps = new_exp_list(sql->sa);
			for (n = l->exps->h; n; n = n->next) { 
				sql_exp *e = n->data;

				if (is_column(e->type) && exp_is_atom(e)) {
					list_append(exps, e);
				} else if (e->type == e_column /*||
					   e->type == e_func ||
					   e->type == e_convert*/) {
					if (e->name && e->name[0] == 'L')
						return rel;
					list_append(exps, e);
				} else {
					return rel;
				}
			}
		} else {
			exps = rel_projections(sql, l, NULL, 1, 1);
		}
		/* also handle right hand of join */
		if (is_join(rel->op) && r->op == op_project && r->l) {
			/* Here we also check all expressions of r like above
			   but also we need to check for ambigious names. */ 

			for (n = r->exps->h; n; n = n->next) { 
				sql_exp *e = n->data;

				if (is_column(e->type) && exp_is_atom(e)) {
					list_append(exps, e);
				} else if (e->type == e_column /*||
					   e->type == e_func ||
					   e->type == e_convert*/) {
					if (e->name && e->name[0] == 'L')
						return rel;
					list_append(exps, e);
				} else {
					return rel;
				}
			}
		} else if (is_join(rel->op)) {
			list *r_exps = rel_projections(sql, r, NULL, 1, 1);

			list_merge(exps, r_exps, (fdup)NULL);
		}
		/* Here we should check for ambigious names ? */
		if (is_join(rel->op) && r) {
			t = (l->op == op_project && l->l)?l->l:l;
			l_exps = rel_projections(sql, t, NULL, 1, 1);
			t = (r->op == op_project && r->l)?r->l:r;
			r_exps = rel_projections(sql, t, NULL, 1, 1);
			for(n = l_exps->h; n; n = n->next) {
				sql_exp *e = n->data;
	
				if (exp_is_atom(e))
					continue;
				if ((e->l && exps_bind_column2(r_exps, e->l, e->r) != NULL) || 
				   (exps_bind_column(r_exps, e->r, NULL) != NULL && (!e->l || !e->r)))
					return rel;
			}
			for(n = r_exps->h; n; n = n->next) {
				sql_exp *e = n->data;
	
				if (exp_is_atom(e))
					continue;
				if ((e->l && exps_bind_column2(l_exps, e->l, e->r) != NULL) || 
				   (exps_bind_column(l_exps, e->r, NULL) != NULL && (!e->l || !e->r)))
					return rel;
			}
		}

		/* rename operator expressions */
		if (l->op == op_project) {
			/* rewrite rel from rel->l into rel->l->l */
			if (rel->exps) {
				list *nexps = new_exp_list(sql->sa);

				for (n = rel->exps->h; n; n = n->next) {
					sql_exp *e = n->data;
	
					e = exp_rename(sql, e, l, l->l);
					assert(e);
					list_append(nexps, e);
				}
				rel->exps = nexps;
			}
			rel->l = l->l;
			l->l = NULL;
			rel_destroy(l);
		}
		if (is_join(rel->op) && r->op == op_project) {
			/* rewrite rel from rel->r into rel->r->l */
			if (rel->exps) {
				list *nexps = new_exp_list(sql->sa);

				for (n = rel->exps->h; n; n = n->next) {
					sql_exp *e = n->data;

					e = exp_rename(sql, e, r, r->l);
					assert(e);
					list_append(nexps, e);
				}
				rel->exps = nexps;
			}
			rel->r = r->l;
			r->l = NULL;
			rel_destroy(r);
		} 
		/* Done, ie introduce new project */
		exps_fix_card(exps, rel->card);
		(*changes)++;
		return rel_inplace_project(sql->sa, rel, NULL, exps);
	}
	if (is_groupby(rel->op) && !rel_is_ref(rel) && rel->exps) {
		node *n;
		int fnd = 0;
		list *aexps, *pexps;

		/* check if some are expressions aren't e_aggr */
		for (n = rel->exps->h; n && !fnd; n = n->next) {
			sql_exp *e = n->data;

			if (e->type != e_aggr && e->type != e_column) {
				fnd = 1;
			}
		}
		/* only aggr, no rewrite needed */
		if (!fnd) 
			return rel;

		aexps = sa_list(sql->sa);
		pexps = sa_list(sql->sa);
		for ( n = rel->exps->h; n; n = n->next) {
			sql_exp *e = n->data, *ne = NULL;

			switch (e->type) {	
			case e_atom: /* move over to the projection */
				list_append(pexps, e);
				break;
			case e_func: 
			case e_convert: 
				list_append(pexps, e);
				list_split_aggr_and_project(sql, aexps, e->l);
				break;
			default: /* simple alias */
				list_append(aexps, e);
				ne = exp_column(sql->sa, exp_find_rel_name(e), exp_name(e), exp_subtype(e), e->card, has_nil(e), is_intern(e));
				list_append(pexps, ne);
				break;
			}
		}
		(*changes)++;
		rel->exps = aexps;
		return rel_inplace_project( sql->sa, rel, NULL, pexps);
	}
	return rel;
}

static int
exp_mark_used(sql_rel *subrel, sql_exp *e)
{
	int nr = 0;
	sql_exp *ne = NULL;

	switch(e->type) {
	case e_column:
		ne = rel_find_exp(subrel, e);
		break;
	case e_convert:
		return exp_mark_used(subrel, e->l);
	case e_aggr:
	case e_func: {
		if (e->l) {
			list *l = e->l;
			node *n = l->h;
	
			for (;n != NULL; n = n->next) 
				nr += exp_mark_used(subrel, n->data);
		}
		/* rank operators have a second list of arguments */
		if (e->r) {
			list *l = e->r;
			node *n = l->h;
	
			for (;n != NULL; n = n->next) 
				nr += exp_mark_used(subrel, n->data);
		}
		break;
	}
	case e_cmp:
		if (e->flag == cmp_or) {
			list *l = e->l;
			node *n;
	
			for (n = l->h; n != NULL; n = n->next) 
				nr += exp_mark_used(subrel, n->data);
			l = e->r;
			for (n = l->h; n != NULL; n = n->next) 
				nr += exp_mark_used(subrel, n->data);
		} else if (e->flag == cmp_in || e->flag == cmp_notin || get_cmp(e) == cmp_filter) {
			list *r = e->r;
			node *n;

			nr += exp_mark_used(subrel, e->l);
			for (n = r->h; n != NULL; n = n->next)
				nr += exp_mark_used(subrel, n->data);
		} else {
			nr += exp_mark_used(subrel, e->l);
			nr += exp_mark_used(subrel, e->r);
			if (e->f)
				nr += exp_mark_used(subrel, e->f);
		}
		break;
	case e_atom:
		/* atoms are used in e_cmp */
		e->used = 1;
		/* return 0 as constants may require a full column ! */
		return 0;
	case e_psm:
		e->used = 1;
		break;
	}
	if (ne) {
		ne->used = 1;
		return ne->used;
	}
	return nr;
}

static void
positional_exps_mark_used( sql_rel *rel, sql_rel *subrel )
{
	if (!rel->exps) 
		assert(0);

	if ((is_topn(subrel->op) || is_sample(subrel->op)) && subrel->l)
		subrel = subrel->l;
	/* everything is used within the set operation */
	if (rel->exps && subrel->exps) {
		node *m;
		for (m=subrel->exps->h; m; m = m->next) {
			sql_exp *se = m->data;

			se->used = 1;
		}
	}
}

static void
exps_mark_used(sql_allocator *sa, sql_rel *rel, sql_rel *subrel)
{
	int nr = 0;
	if (rel->exps) {
		node *n;
		int len = list_length(rel->exps), i;
		sql_exp **exps = SA_NEW_ARRAY(sa, sql_exp*, len);

		for (n=rel->exps->h, i = 0; n; n = n->next, i++) 
			exps[i] = n->data;

		for (i = len-1; i >= 0; i--) {
			sql_exp *e = exps[i];

			if (!is_project(rel->op) || e->used) {
				if (is_project(rel->op))
					nr += exp_mark_used(rel, e);
				nr += exp_mark_used(subrel, e);
			}
		}
	}
	/* for count/rank we need atleast one column */
	if (!nr && (is_project(subrel->op) || is_base(subrel->op)) && subrel->exps->h) {
		sql_exp *e = subrel->exps->h->data;
		e->used = 1;
	}
	if (rel->r && (rel->op == op_project || rel->op  == op_groupby)) {
		list *l = rel->r;
		node *n;

		for (n=l->h; n; n = n->next) {
			sql_exp *e = n->data;

			exp_mark_used(rel, e);
			/* possibly project/groupby uses columns from the inner */ 
			exp_mark_used(subrel, e);
		}
	}
}

static void
exps_used(list *l)
{
	node *n;

	if (l) {
		for (n = l->h; n; n = n->next) {
			sql_exp *e = n->data;
	
			if (e)
				e->used = 1;
		}
	}
}

static void
rel_used(sql_rel *rel)
{
	if (is_join(rel->op) || is_set(rel->op) || is_semi(rel->op)) {
		if (rel->l) 
			rel_used(rel->l);
		if (rel->r) 
			rel_used(rel->r);
	} else if (is_topn(rel->op) || is_select(rel->op) || is_sample(rel->op)) {
		rel_used(rel->l);
		rel = rel->l;
	}
	if (rel->exps) {
		exps_used(rel->exps);
		if (rel->r && (rel->op == op_project || rel->op  == op_groupby))
			exps_used(rel->r);
	}
}

static void
rel_mark_used(mvc *sql, sql_rel *rel, int proj)
{
	(void)sql;

	if (proj && (need_distinct(rel))) 
		rel_used(rel);

	switch(rel->op) {
	case op_basetable:
	case op_table:
		break;

	case op_topn:
	case op_sample:
		if (proj) {
			rel = rel ->l;
			rel_mark_used(sql, rel, proj);
			break;
		}
	case op_project:
	case op_groupby: 
		if (proj && rel->l) {
			exps_mark_used(sql->sa, rel, rel->l);
			rel_mark_used(sql, rel->l, 0);
		}
		break;
	case op_insert:
	case op_update:
	case op_delete:
	case op_ddl:
		break;

	case op_select:
		if (rel->l) {
			exps_mark_used(sql->sa, rel, rel->l);
			rel_mark_used(sql, rel->l, 0);
		}
		break;

	case op_union: 
	case op_inter: 
	case op_except: 
		/* For now we mark all union expression as used */

		/* Later we should (in case of union all) remove unused
		 * columns from the projection.
		 * 
 		 * Project part of union is based on column position.
		 */
		if (proj && (need_distinct(rel) || !rel->exps)) {
			rel_used(rel);
			if (!rel->exps) {
				rel_used(rel->l);
				rel_used(rel->r);
			}
			rel_mark_used(sql, rel->l, 0);
			rel_mark_used(sql, rel->r, 0);
		} else if (proj && !need_distinct(rel)) {
			sql_rel *l = rel->l;

			positional_exps_mark_used(rel, l);
			rel_mark_used(sql, rel->l, 1);
			/* based on child check set expression list */
			if (is_project(l->op) && need_distinct(l))
				positional_exps_mark_used(l, rel);
			positional_exps_mark_used(rel, rel->r);
			rel_mark_used(sql, rel->r, 1);
		}
		break;

	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 
	case op_semi: 
	case op_anti: 
		exps_mark_used(sql->sa, rel, rel->l);
		exps_mark_used(sql->sa, rel, rel->r);
		rel_mark_used(sql, rel->l, 0);
		rel_mark_used(sql, rel->r, 0);
		break;
	}
}

static sql_rel * rel_dce_sub(mvc *sql, sql_rel *rel);
static sql_rel * rel_dce(mvc *sql, sql_rel *rel);

static sql_rel *
rel_remove_unused(mvc *sql, sql_rel *rel) 
{
	int needed = 0;

	if (!rel || rel_is_ref(rel))
		return rel;

	switch(rel->op) {
	case op_basetable: {
		sql_table *t = rel->l;

		if (isMergeTable(t) || isReplicaTable(t)) 
			return rel;
	}
	case op_table:
		if (rel->exps) {
			node *n;
			list *exps;

			for(n=rel->exps->h; n && !needed; n = n->next) {
				sql_exp *e = n->data;

				if (!e->used)
					needed = 1;
			}

			if (!needed)
				return rel;

			exps = new_exp_list(sql->sa);
			for(n=rel->exps->h; n; n = n->next) {
				sql_exp *e = n->data;

				if (e->used)
					append(exps, e);
			}
			/* atleast one (needed for crossproducts, count(*), rank() and single value projections) !, handled by exps_mark_used */
			if (list_length(exps) == 0)
				append(exps, rel->exps->h->data);
			rel->exps = exps;
		}
		return rel;

	case op_topn:
	case op_sample:

		if (rel->l)
			rel->l = rel_remove_unused(sql, rel->l);
		return rel;

	case op_project:
	case op_groupby: 


		if (rel->l && rel->exps) {
			node *n;
			list *exps;

			for(n=rel->exps->h; n && !needed; n = n->next) {
				sql_exp *e = n->data;

				if (!e->used)
					needed = 1;
			}
			if (!needed)
				return rel;

 			exps = new_exp_list(sql->sa);
			for(n=rel->exps->h; n; n = n->next) {
				sql_exp *e = n->data;

				if (e->used)
					append(exps, e);
			}
			/* atleast one (needed for crossproducts, count(*), rank() and single value projections) !, handled by exps_mark_used */
			assert(list_length(exps) > 0);
			rel->exps = exps;
		}
		return rel;

	case op_union: 
	case op_inter: 
	case op_except: 

	case op_insert:
	case op_update:
	case op_delete:

	case op_select: 

	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 
	case op_semi: 
	case op_anti: 
	case op_ddl:
	default:
		return rel;
	}
}

static sql_rel *
rel_dce_down(mvc *sql, sql_rel *rel, int skip_proj) 
{
	if (!rel)
		return rel;

	if (!skip_proj && rel_is_ref(rel))
		return rel_dce(sql, rel);

	switch(rel->op) {
	case op_basetable:
	case op_table:

		if (!skip_proj)
			rel_dce_sub(sql, rel);

	case op_insert:
	case op_update:
	case op_delete:
	case op_ddl:

		return rel;

	case op_topn: 
	case op_sample: 
	case op_project:
	case op_groupby: 

		if (skip_proj && rel->l)
			rel->l = rel_dce_down(sql, rel->l, is_topn(rel->op) || is_sample(rel->op));
		if (!skip_proj)
			rel_dce_sub(sql, rel);
		return rel;

	case op_union: 
	case op_inter: 
	case op_except: 
		if (skip_proj) {
			if (rel->l)
				rel->l = rel_dce_down(sql, rel->l, 0);
			if (rel->r)
				rel->r = rel_dce_down(sql, rel->r, 0);
		}
		if (!skip_proj)
			rel_dce_sub(sql, rel);
		return rel;

	case op_select: 
		if (rel->l)
			rel->l = rel_dce_down(sql, rel->l, 0);
		return rel;

	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 
	case op_semi: 
	case op_anti: 
		if (rel->l)
			rel->l = rel_dce_down(sql, rel->l, 0);
		if (rel->r)
			rel->r = rel_dce_down(sql, rel->r, 0);
		return rel;
	}
	return rel;
}

/* DCE
 *
 * Based on top relation expressions mark sub expressions as used.
 * Then recurse down until the projections. Clean them up and repeat.
 */

static sql_rel *
rel_dce_sub(mvc *sql, sql_rel *rel)
{
	if (!rel)
		return rel;
	
	/* 
  	 * Mark used up until the next project 
	 * For setops we need to first mark, then remove 
         * because of positional dependency 
 	 */
	rel_mark_used(sql, rel, 1);
	rel = rel_remove_unused(sql, rel);
	rel_dce_down(sql, rel, 1);
	return rel;
}

/* add projects under set ops */
static sql_rel *
rel_add_projects(mvc *sql, sql_rel *rel) 
{
	if (!rel)
		return rel;

	switch(rel->op) {
	case op_basetable:
	case op_table:

	case op_insert:
	case op_update:
	case op_delete:
	case op_ddl:

		return rel;

	case op_union: 
	case op_inter: 
	case op_except: 

		/* We can only reduce the list of expressions of an set op
		 * if the projection under it can also be reduced.
		 */
		if (rel->l) {
			sql_rel *l = rel->l;

			l->subquery = 0;
			if (!is_project(l->op) && !need_distinct(rel))
				l = rel_project(sql->sa, l, rel_projections(sql, l, NULL, 1, 1));
			rel->l = rel_add_projects(sql, l);
		}
		if (rel->r) {
			sql_rel *r = rel->r;

			r->subquery = 0;
			if (!is_project(r->op) && !need_distinct(rel))
				r = rel_project(sql->sa, r, rel_projections(sql, r, NULL, 1, 1));
			rel->r = rel_add_projects(sql, r);
		}
		return rel;

	case op_topn: 
	case op_sample: 
	case op_project:
	case op_groupby: 
	case op_select: 
		if (rel->l)
			rel->l = rel_add_projects(sql, rel->l);
		return rel;

	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 
	case op_semi: 
	case op_anti: 
		if (rel->l)
			rel->l = rel_add_projects(sql, rel->l);
		if (rel->r)
			rel->r = rel_add_projects(sql, rel->r);
		return rel;
	}
	return rel;
}

static sql_rel *
rel_dce(mvc *sql, sql_rel *rel)
{
	rel = rel_add_projects(sql, rel);
	rel_used(rel);
	rel_dce_sub(sql, rel);
	return rel;
}

static int
index_exp(sql_exp *e, sql_idx *i) 
{
	if (e->type == e_cmp && !is_complex_exp(e->flag)) {
		switch(i->type) {
		case hash_idx:
		case oph_idx:
			if (e->flag == cmp_equal)
				return 0;
		case join_idx:
		default:
			return -1;
		}
	}
	return -1;
}

static sql_idx *
find_index(sql_allocator *sa, sql_rel *rel, sql_rel *sub, list **EXPS)
{
	node *n;

	/* any (partial) match of the expressions with the index columns */
	/* Depending on the index type we may need full matches and only
	   limited number of cmp types (hash only equality etc) */
	/* Depending on the index type we should (in the rel_bin) generate
	   more code, ie for spatial index add post filter etc, for hash
	   compute hash value and use index */

	if (sub->exps && rel->exps) 
	for(n = sub->exps->h; n; n = n->next) {
		prop *p;
		sql_exp *e = n->data;

		if ((p = find_prop(e->p, PROP_HASHIDX)) != NULL) {
			list *exps, *cols;
	    		sql_idx *i = p->value;
			fcmp cmp = (fcmp)&sql_column_kc_cmp;

			/* join indices are only interesting for joins */
			if (i->type == join_idx || list_length(i->columns) <= 1)
				continue;
			/* based on the index type, find qualifying exps */
			exps = list_select(rel->exps, i, (fcmp) &index_exp, (fdup)NULL);
			if (!exps || !list_length(exps))
				continue;
			/* now we obtain the columns, move into sql_column_kc_cmp! */
			cols = list_map(exps, sub, (fmap) &sjexp_col);

			/* TODO check that at most 2 relations are involved */

			/* Match the index columns with the expression columns. 
			   TODO, Allow partial matches ! */
			if (list_match(cols, i->columns, cmp) == 0) {
				/* re-order exps in index order */
				node *n, *m;
				list *es = sa_list(sa);

				for(n = i->columns->h; n; n = n->next) {
					int i = 0;
					for(m = cols->h; m; m = m->next, i++) {
						if (cmp(m->data, n->data) == 0){
							sql_exp *e = list_fetch(exps, i);
							list_append(es, e);
							break;
						}
					}
				}
				/* fix the destroy function */
				cols->destroy = NULL;
				*EXPS = es;
				e->used = 1;
				return i;
			}
			cols->destroy = NULL;
		}
	}
	return NULL;
}

static sql_rel *
rel_use_index(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)changes;
	(void)sql;
	if (rel->l && (is_select(rel->op) || is_join(rel->op))) {
		list *exps = NULL;
		sql_idx *i = find_index(sql->sa, rel, rel->l, &exps);
		int left = 1;

		if (!i && is_join(rel->op))
			i = find_index(sql->sa, rel, rel->l, &exps);
		if (!i && is_join(rel->op)) {
			left = 0;
			i = find_index(sql->sa, rel, rel->r, &exps);
		}
			
		if (i) {
			prop *p;
			node *n;
			int single_table = 1;
			sql_exp *re = NULL;
	
			for( n = exps->h; n && single_table; n = n->next) { 
				sql_exp *e = n->data;
				sql_exp *nre = e->r;

				if (is_join(rel->op) && 
				 	((left && !rel_find_exp(rel->l, e->l)) ||
				 	(!left && !rel_find_exp(rel->r, e->l)))) 
					nre = e->l;
				single_table = (!re || !exps_match_col_exps(nre, re));
				re = nre;
			}
			if (single_table) { /* add PROP_HASHCOL to all column exps */
				for( n = exps->h; n; n = n->next) { 
					sql_exp *e = n->data;

					/* swapped ? */
					if (is_join(rel->op) && 
					 	((left && !rel_find_exp(rel->l, e->l)) ||
					 	(!left && !rel_find_exp(rel->r, e->l)))) 
						n->data = e = exp_compare(sql->sa, e->r, e->l, cmp_equal);
					p = find_prop(e->p, PROP_HASHCOL);
					if (!p)
						e->p = p = prop_create(sql->sa, PROP_HASHCOL, e->p);
					p->value = i;
				}
			}
			/* add the remaining exps to the new exp list */
			if (list_length(rel->exps) > list_length(exps)) {
				for( n = rel->exps->h; n; n = n->next) {
					sql_exp *e = n->data;
					if (!list_find(exps, e, (fcmp)&exp_cmp))
						list_append(exps, e);
				}
			}
			rel->exps = exps;
		}
	}
	return rel;
}

/* TODO CSE */
#if 0
static list *
exp_merge(list *exps)
{
	node *n, *m;
	for (n=exps->h; n && n->next; n = n->next) {
		sql_exp *e = n->data;
		/*sql_exp *le = e->l;*/
		sql_exp *re = e->r;

		if (e->type == e_cmp && e->flag == cmp_or)
			continue;

		/* only look for gt, gte, lte, lt */
		if (re->card == CARD_ATOM && e->flag < cmp_equal) {
			for (m=n->next; m; m = m->next) {
				sql_exp *f = m->data;
				/*sql_exp *lf = f->l;*/
				sql_exp *rf = f->r;

				if (rf->card == CARD_ATOM && f->flag < cmp_equal) {
					printf("possible candidate\n");
				}
			}
		}
	}
	return exps;
}
#endif

static int
score_se( mvc *sql, sql_rel *rel, sql_exp *e)
{
	int score = 0;
	if (e->type == e_cmp && !is_complex_exp(e->flag)) {
		score += score_gbe(sql, rel, e->l);
	}
	score += exp_keyvalue(e);
	return score;
}

static sql_rel *
rel_select_order(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)changes;
	(void)sql;
	if (is_select(rel->op) && rel->exps && list_length(rel->exps)>1) {
		int i, *scores = calloc(list_length(rel->exps), sizeof(int));
		node *n;

		for (i = 0, n = rel->exps->h; n; i++, n = n->next) 
			scores[i] = score_se(sql, rel, n->data);
		rel->exps = list_keysort(rel->exps, scores, (fdup)NULL);
		free(scores);
	}
	return rel;
}

static sql_rel *
rel_simplify_like_select(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)sql;
	if (is_select(rel->op) && rel->exps) {
		node *n;
		list *exps = sa_list(sql->sa);
			
		for (n = rel->exps->h; n; n = n->next) {
			sql_exp *e = n->data;

			if (e->type == e_cmp && get_cmp(e) == cmp_filter && strcmp(((sql_subfunc*)e->f)->func->base.name, "like") == 0) {
				list *r = e->r;
				sql_exp *fmt = r->h->data;
				sql_exp *esc = (r->h->next)?r->h->next->data:NULL;
				int rewrite = 0;

				if (fmt->type == e_convert)
					fmt = fmt->l;
				/* check for simple like expression */
				if (is_atom(fmt->type)) {
					atom *fa = NULL;

					if (fmt->l) {
						fa = fmt->l;
					/* simple numbered argument */
					} else if (!fmt->r && !fmt->f) {
						fa = sql->args[fmt->flag];

					}
					if (fa && fa->data.vtype == TYPE_str && 
					    !strchr(fa->data.val.sval, '%') &&
					    !strchr(fa->data.val.sval, '_'))
						rewrite = 1;
				}
				if (rewrite && esc && is_atom(esc->type)) {
			 		atom *ea = NULL;

					if (esc->l) {
						ea = esc->l;
					/* simple numbered argument */
					} else if (!esc->r && !esc->f) {
						ea = sql->args[esc->flag];

					}
					if (ea && (ea->data.vtype != TYPE_str ||
					    strlen(ea->data.val.sval) != 0))
						rewrite = 0;
				}
				if (rewrite) { 	/* rewrite to cmp_equal ! */
					list *r = e->r;
					sql_exp *ne = exp_compare(sql->sa, e->l, r->h->data, cmp_equal);
					/* if rewriten don't cache this query */
					list_append(exps, ne);
					sql->caching = 0;
					(*changes)++;
				} else {
					list_append(exps, e);
				}
			} else {
				list_append(exps, e);
			}
		}
		rel->exps = exps;
	}
	return rel;
}

static list *
exp_merge_range(sql_allocator *sa, list *exps)
{
	node *n, *m;
	for (n=exps->h; n; n = n->next) {
		sql_exp *e = n->data;
		sql_exp *le = e->l;
		sql_exp *re = e->r;

		/* handle the and's in the or lists */
		if (e->type == e_cmp && e->flag == cmp_or) {
			e->l = exp_merge_range(sa, e->l);
			e->r = exp_merge_range(sa, e->r);
		/* only look for gt, gte, lte, lt */
		} else if (n->next &&
		    e->type == e_cmp && e->flag < cmp_equal && !e->f && 
		    re->card == CARD_ATOM) {
			for (m=n->next; m; m = m->next) {
				sql_exp *f = m->data;
				sql_exp *lf = f->l;
				sql_exp *rf = f->r;

				if (f->type == e_cmp && f->flag < cmp_equal && !f->f &&
				    rf->card == CARD_ATOM && 
				    exp_match_exp(le, lf)) {
					sql_exp *ne;
					int swap = 0, lt = 0, gt = 0;
					/* for now only   c1 <[=] x <[=] c2 */ 

					swap = lt = (e->flag == cmp_lt || e->flag == cmp_lte);
					gt = !lt;

					if (gt && 
					   (f->flag == cmp_gt ||
					    f->flag == cmp_gte)) 
						continue;
					if (lt && 
					   (f->flag == cmp_lt ||
					    f->flag == cmp_lte)) 
						continue;
					if (!swap) 
						ne = exp_compare2(sa, le, re, rf, compare2range(e->flag, f->flag));
					else
						ne = exp_compare2(sa, le, rf, re, compare2range(f->flag, e->flag));

					list_remove_data(exps, e);
					list_remove_data(exps, f);
					list_append(exps, ne);
					return exp_merge_range(sa, exps);
				}
			}
		} else if (n->next &&
			   e->type == e_cmp && e->flag < cmp_equal && !e->f && 
		    	   re->card > CARD_ATOM) {
			for (m=n->next; m; m = m->next) {
				sql_exp *f = m->data;
				sql_exp *lf = f->l;
				sql_exp *rf = f->r;

				if (f->type == e_cmp && f->flag < cmp_equal && !f->f  &&
				    rf->card > CARD_ATOM) {
					sql_exp *ne;
					int swap = 0, lt = 0, gt = 0;
					comp_type ef = (comp_type) e->flag, ff = (comp_type) f->flag;
				
					/* is left swapped ? */
				     	if (exp_match_exp(re, lf)) {
						sql_exp *t = re; 

						re = le;
						le = t;
						ef = swap_compare(ef);
					}
					/* is right swapped ? */
				     	if (exp_match_exp(le, rf)) {
						sql_exp *t = rf; 

						rf = lf;
						lf = t;
						ff = swap_compare(ff);
					}

				    	if (!exp_match_exp(le, lf))
						continue;

					/* for now only   c1 <[=] x <[=] c2 */ 
					swap = lt = (ef == cmp_lt || ef == cmp_lte);
					gt = !lt;

					if (gt && (ff == cmp_gt || ff == cmp_gte)) 
						continue;
					if (lt && (ff == cmp_lt || ff == cmp_lte)) 
						continue;
					if (!swap) 
						ne = exp_compare2(sa, le, re, rf, compare2range(ef, ff));
					else
						ne = exp_compare2(sa, le, rf, re, compare2range(ff, ef));

					list_remove_data(exps, e);
					list_remove_data(exps, f);
					list_append(exps, ne);
					return exp_merge_range(sa, exps);
				}
			}
		}
	}
	return exps;
}

static sql_rel *
rel_find_range(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)changes;
	if ((is_join(rel->op) || is_select(rel->op)) && rel->exps && list_length(rel->exps)>1) 
		rel->exps = exp_merge_range(sql->sa, rel->exps);
	return rel;
}

/* 
 * Casting decimal values on both sides of a compare expression is expensive,
 * both in preformance (cpu cost) and memory requirements (need for large 
 * types). 
 */

static int
reduce_scale(atom *a)
{
	if (a->data.vtype == TYPE_lng) {
		lng v = a->data.val.lval;
		int i = 0;

		if (v != 0) 
                        while( (v/10)*10 == v ) {
                                i++;
                                v /= 10;
                        }
		a->data.val.lval = v;
		return i;
	}
	if (a->data.vtype == TYPE_int) {
		int v = a->data.val.ival;
		int i = 0;

		if (v != 0) 
                        while( (v/10)*10 == v ) {
                                i++;
                                v /= 10;
                        }
		a->data.val.ival = v;
		return i;
	}
	if (a->data.vtype == TYPE_sht) {
		sht v = a->data.val.shval;
		int i = 0;

		if (v != 0) 
                        while( (v/10)*10 == v ) {
                                i++;
                                v /= 10;
                        }
		a->data.val.shval = v;
		return i;
	}
	return 0;
}

static sql_rel *
rel_project_reduce_casts(int *changes, mvc *sql, sql_rel *rel) 
{
	if (is_project(rel->op) && list_length(rel->exps)) {
		list *exps = rel->exps;
		node *n;

		for (n=exps->h; n; n = n->next) {
			sql_exp *e = n->data;

			if (e && e->type == e_func) {
				sql_subfunc *f = e->f;

				if (!f->func->s && !strcmp(f->func->base.name, "sql_mul") && f->res.scale > 0) {
					list *args = e->l;
					sql_exp *h = args->h->data;
					sql_exp *t = args->t->data;
					atom *a;

					if ((is_atom(h->type) && (a = exp_value(h, sql->args, sql->argc)) != NULL) ||
					    (is_atom(t->type) && (a = exp_value(t, sql->args, sql->argc)) != NULL)) {
						int rs = reduce_scale(a);

						f->res.scale -= rs; 
						if (rs)
							(*changes)+= rs;
					}
				}
			}
		}
	}
	return rel;
}

static sql_rel *
rel_reduce_casts(int *changes, mvc *sql, sql_rel *rel) 
{
	(void)sql;
	(void)changes;
	if ((is_join(rel->op) || is_semi(rel->op) || is_select(rel->op)) && 
			rel->exps && list_length(rel->exps)) {
		list *exps = rel->exps;
		node *n;

		for (n=exps->h; n; n = n->next) {
			sql_exp *e = n->data;
			sql_exp *le = e->l;
			sql_exp *re = e->r;
	
			/* handle the and's in the or lists */
			if (e->type != e_cmp || 
			   (e->flag != cmp_lt && e->flag != cmp_gt)) 
				continue;
			/* rewrite e if left or right is cast */
			if (le->type == e_convert || re->type == e_convert) {
				sql_rel *r = rel->r;

				/* if convert on left then find
				 * mul or div on right which increased
				 * scale!
				 */
				if (le->type == e_convert && re->type == e_column && r && is_project(r->op)) {
					sql_exp *nre = rel_find_exp(r, re);
					sql_subtype *tt = exp_totype(le);
					sql_subtype *ft = exp_fromtype(le);

					if (nre && nre->type == e_func) {
						sql_subfunc *f = nre->f;

						if (!f->func->s && !strcmp(f->func->base.name, "sql_mul")) {
							list *args = nre->l;
							sql_exp *ce = args->t->data;
							sql_subtype *fst = exp_subtype(args->h->data);
							atom *a;

							if (fst->scale == ft->scale &&
							   (a = exp_value(ce, sql->args, sql->argc)) != NULL) {
								lng v = 1;
								/* multiply with smallest value, then scale and (round) */
								int scale = tt->scale - ft->scale;
								int rs = reduce_scale(a);

								scale -= rs;

								args = new_exp_list(sql->sa);
								while(scale > 0) {
									scale--;
									v *= 10;
								}
								append(args, re);
								append(args, exp_atom_lng(sql->sa, v));
								f = find_func(sql, "scale_down", args);
								nre = exp_op(sql->sa, args, f);
								e = exp_compare(sql->sa, le->l, nre, e->flag);
							}
						}
					}
				}
			}
			n->data = e;	
		}
	}
	return rel;
}

static int
is_identity_of(sql_exp *e, sql_rel *l) 
{
	if (e->type != e_cmp)
		return 0;
	if (!is_identity(e->l, l) || !is_identity(e->r, l))
		return 0;
	return 1;
}

/* rewrite {semi,anti}join (A, join(A,B)) into {semi,anti}join (A,B) */
/* TODO: handle A, join(B,A) as well */

/* More general case is (join reduction)
 *
   	{semi,anti}join (A, join(A,B) [A.c1 == B.c1]) [ A.c1 == B.c1 ]
	into {semi,anti}join (A,B) [ A.c1 == B.c1 ] 
*/
	
static sql_rel *
rel_rewrite_semijoin(int *changes, mvc *sql, sql_rel *rel)
{
	(void)sql;
	if (is_semi(rel->op)) {
		sql_rel *l = rel->l;
		sql_rel *r = rel->r;
		sql_rel *rl = (r->l)?r->l:NULL;

		if (l->ref.refcnt == 2 && 
		   ((is_join(r->op) && l == r->l) || 
		    (is_project(r->op) && rl && is_join(rl->op) && l == rl->l))){
			sql_rel *or = r;

			if (!rel->exps || list_length(rel->exps) != 1 ||
			    !is_identity_of(rel->exps->h->data, l)) 
				return rel;
			
			if (is_project(r->op)) 
				r = rl;

			rel->r = rel_dup(r->r);
			/* maybe rename exps ? */
			rel->exps = r->exps;
			r->exps = NULL;
			rel_destroy(or);
			(*changes)++;
		}
	}
	if (is_semi(rel->op)) {
		sql_rel *l = rel->l, *rl = NULL;
		sql_rel *r = rel->r, *or = r;

		if (r)
			rl = r->l;
		if (r && is_project(r->op)) {
			r = rl;
			if (r)
				rl = r->l;
		}

		if (l && r && rl && 
		    is_basetable(l->op) && is_basetable(rl->op) &&
		    is_join(r->op) && l->l == rl->l)
		{
			node *n, *m;
			list *exps;

			if (!rel->exps || !r->exps ||
		       	    list_length(rel->exps) != list_length(r->exps)) 
				return rel;
			exps = new_exp_list(sql->sa);

			/* are the join conditions equal */
			for (n = rel->exps->h, m = r->exps->h;
			     n && m; n = n->next, m = m->next)
			{
				sql_exp *le = NULL, *oe = n->data;
				sql_exp *re = NULL, *ne = m->data;
				sql_column *cl;  
				
				if (oe->type != e_cmp || ne->type != e_cmp ||
				    oe->flag != cmp_equal || 
				    ne->flag != cmp_equal)
					return rel;

				if ((cl = exp_find_column(rel->l, oe->l, -2)) != NULL)
					le = oe->l;
				else if ((cl = exp_find_column(rel->l, oe->r, -2)) != NULL)
					le = oe->r;

				if (exp_find_column(rl, ne->l, -2) == cl)
					re = oe->r;
				else if (exp_find_column(rl, ne->r, -2) == cl)
					re = oe->l;
				if (!re)
					return rel;
				ne = exp_compare(sql->sa, le, re, cmp_equal);
				append(exps, ne);
			}

			rel->r = rel_dup(r->r);
			/* maybe rename exps ? */
			rel->exps = exps;
			rel_destroy(or);
			(*changes)++;
		}
	}
	return rel;
}

/* antijoin(a, union(b,c)) -> antijoin(antijoin(a,b), c) */
static sql_rel *
rel_rewrite_antijoin(int *changes, mvc *sql, sql_rel *rel)
{
	if (rel->op == op_anti) {
		sql_rel *l = rel->l;
		sql_rel *r = rel->r;

		if (l && !rel_is_ref(l) && 
		    r && !rel_is_ref(r) && is_union(r->op)) {
			sql_rel *rl = rel_dup(r->l), *nl;
			sql_rel *rr = rel_dup(r->r);

			if (!is_project(rl->op)) 
				rl = rel_project(sql->sa, rl, 
					rel_projections(sql, rl, NULL, 1, 1));
			if (!is_project(rr->op)) 
				rr = rel_project(sql->sa, rr,
					rel_projections(sql, rr, NULL, 1, 1));
			rel_rename_exps(sql, r->exps, rl->exps);
			rel_rename_exps(sql, r->exps, rr->exps);

			nl = rel_crossproduct(sql->sa, rel->l, rl, op_anti);
			nl->exps = exps_copy(sql->sa, rel->exps);
			rel->l = nl;
			rel->r = rr;
			rel_destroy(r);
			(*changes)++;
			return rel;
		}
	}
	return rel;
}

static sql_rel *
rel_semijoin_use_fk(int *changes, mvc *sql, sql_rel *rel)
{
	(void)changes;
	if (is_semi(rel->op) && rel->exps) {
		list *exps = rel->exps;
		list *rels = new_rel_list(sql->sa);

		rel->exps = NULL;
		append(rels, rel->l);
		append(rels, rel->r);

		(void) find_fk( sql, rels, exps);
		rel->exps = exps;
	}
	return rel;
}

/* rewrite sqltype into backend types */
static sql_rel *
rel_rewrite_types(int *changes, mvc *sql, sql_rel *rel)
{
	(void)sql;
	(void)changes;
	return rel;
}

/* rewrite merge tables into union of base tables and call optimizer again */
static sql_rel *
rel_merge_table_rewrite(int *changes, mvc *sql, sql_rel *rel)
{
	if (is_basetable(rel->op)) {
		sql_table *t = rel->l;

		if (isMergeTable(t)) {
			/* instantiate merge tabel */
			sql_rel *nrel = NULL;
			char *tname = exp_find_rel_name(rel->exps->h->data);

			node *n;

			if (list_empty(t->tables.set)) 
				return rel;
			assert(!rel_is_ref(rel));
			(*changes)++;
			if (t->tables.set) {
				for (n = t->tables.set->h; n; n = n->next) {
					sql_table *pt = n->data;
					sql_rel *prel = rel_basetable(sql, pt, tname);
					node *n, *m;

					/* rename (mostly the idxs) */
					for (n = rel->exps->h, m = prel->exps->h; n && m; n = n->next, m = m->next ) {
						sql_exp *e = n->data;
						sql_exp *ne = m->data;

						exp_setname(sql->sa, ne, e->rname, e->name);
					}
					if (nrel) { 
						nrel = rel_setop(sql->sa, nrel, prel, op_union);
						nrel->exps = rel_projections(sql, rel, NULL, 1, 1);
					} else {
						nrel = prel;
					}
				}
			}
			if (nrel && list_length(t->tables.set) == 1) {
				nrel = rel_project(sql->sa, nrel, rel->exps);
			} else if (nrel)
				nrel->exps = rel->exps;
			rel_destroy(rel);
			return nrel;
		}
	}
	return rel;
}

static sql_rel *
rewrite(mvc *sql, sql_rel *rel, rewrite_fptr rewriter, int *has_changes) 
{
	int changes = 0;

	if (!rel)
		return rel;

	switch (rel->op) {
	case op_basetable:
	case op_table:
		break;
	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 

	case op_semi: 
	case op_anti: 

	case op_union: 
	case op_inter: 
	case op_except: 
		rel->l = rewrite(sql, rel->l, rewriter, has_changes);
		rel->r = rewrite(sql, rel->r, rewriter, has_changes);
		break;
	case op_project:
	case op_select: 
	case op_groupby: 
	case op_topn: 
	case op_sample: 
		rel->l = rewrite(sql, rel->l, rewriter, has_changes);
		break;
	case op_ddl: 
		rel->l = rewrite(sql, rel->l, rewriter, has_changes);
		if (rel->r)
			rel->r = rewrite(sql, rel->r, rewriter, has_changes);
		break;
	case op_insert:
	case op_update:
	case op_delete:
		rel->r = rewrite(sql, rel->r, rewriter, has_changes);
		break;
	}
	rel = rewriter(&changes, sql, rel);
	if (changes) {
		(*has_changes)++;
		return rewrite(sql, rel, rewriter, has_changes);
	}
	return rel;
}

static sql_rel *
rewrite_topdown(mvc *sql, sql_rel *rel, rewrite_fptr rewriter, int *has_changes) 
{
	if (!rel)
		return rel;

	rel = rewriter(has_changes, sql, rel);
	switch (rel->op) {
	case op_basetable:
	case op_table:
		break;
	case op_join: 
	case op_left: 
	case op_right: 
	case op_full: 

	case op_semi: 
	case op_anti: 

	case op_union: 
	case op_inter: 
	case op_except: 
		rel->l = rewrite_topdown(sql, rel->l, rewriter, has_changes);
		rel->r = rewrite_topdown(sql, rel->r, rewriter, has_changes);
		break;
	case op_project:
	case op_select: 
	case op_groupby: 
	case op_topn: 
	case op_sample: 
		rel->l = rewrite_topdown(sql, rel->l, rewriter, has_changes);
		break;
	case op_ddl: 
		rel->l = rewrite_topdown(sql, rel->l, rewriter, has_changes);
		if (rel->r)
			rel->r = rewrite_topdown(sql, rel->r, rewriter, has_changes);
		break;
	case op_insert:
	case op_update:
	case op_delete:
		rel->r = rewrite_topdown(sql, rel->r, rewriter, has_changes);
		break;
	}
	return rel;
}

static sql_rel *
_rel_optimizer(mvc *sql, sql_rel *rel, int level) 
{
	int changes = 0, e_changes = 0;
	global_props gp; 

	memset(&gp, 0, sizeof(global_props));
	rel_properties(sql, &gp, rel);

#ifdef DEBUG
{
	int i;
	for (i = 0; i < MAXOPS; i++) {
		if (gp.cnt[i]> 0)
			printf("%s %d\n", op2string((operator_type)i), gp.cnt[i]);
	}
}
#endif
	/* simple merging of projects */
	if (gp.cnt[op_project]) {
		rel = rewrite(sql, rel, &rel_merge_projects, &changes);
		if (level <= 0)
			rel = rewrite(sql, rel, &rel_case_fixup, &changes);
	}

	if (gp.cnt[op_join] || 
	    gp.cnt[op_left] || gp.cnt[op_right] || gp.cnt[op_full] || 
	    gp.cnt[op_semi] || gp.cnt[op_anti] ||
	    gp.cnt[op_select]) {
		rel = rewrite(sql, rel, &rel_find_range, &changes);
		rel = rel_project_reduce_casts(&changes, sql, rel);
		rel = rewrite(sql, rel, &rel_reduce_casts, &changes);
	}

	if (gp.cnt[op_union])
		rel = rewrite(sql, rel, &rel_merge_union, &changes); 

	if (gp.cnt[op_select]) 
		rel = rewrite(sql, rel, &rel_select_cse, &changes); 

	if (gp.cnt[op_project]) 
		rel = rewrite(sql, rel, &rel_project_cse, &changes);

	/* push (simple renaming) projections up */
	if (gp.cnt[op_project])
		rel = rewrite(sql, rel, &rel_push_project_up, &changes); 

	rel = rewrite(sql, rel, &rel_rewrite_types, &changes); 

	if (gp.cnt[op_anti] || gp.cnt[op_semi]) {
		/* rewrite semijoin (A, join(A,B)) into semijoin (A,B) */
		rel = rewrite(sql, rel, &rel_rewrite_semijoin, &changes);
		/* push semijoin through join */
		//rel = rewrite(sql, rel, &rel_push_semijoin_down, &changes);
		/* antijoin(a, union(b,c)) -> antijoin(antijoin(a,b), c) */
		rel = rewrite(sql, rel, &rel_rewrite_antijoin, &changes);
		if (level <= 0)
			rel = rewrite_topdown(sql, rel, &rel_semijoin_use_fk, &changes);
	}

	if (gp.cnt[op_select]) {
		/* only once */
		if (level <= 0)
			rel = rewrite(sql, rel, &rel_merge_rse, &changes); 

		rel = rewrite_topdown(sql, rel, &rel_push_select_down, &changes); 
		rel = rewrite(sql, rel, &rel_remove_empty_select, &e_changes); 
	}

	if (gp.cnt[op_select] && gp.cnt[op_join]) {
		rel = rewrite_topdown(sql, rel, &rel_push_select_down_join, &changes); 
		rel = rewrite(sql, rel, &rel_remove_empty_select, &e_changes); 
	}

	if (gp.cnt[op_join] && gp.cnt[op_groupby]) {
		rel = rewrite_topdown(sql, rel, &rel_push_count_down, &changes);
		if (level <= 0)
			rel = rewrite_topdown(sql, rel, &rel_push_join_down, &changes); 

		/* push_join_down introduces semijoins */
		/* rewrite semijoin (A, join(A,B)) into semijoin (A,B) */
		rel = rewrite(sql, rel, &rel_rewrite_semijoin, &changes);
	}

	if (gp.cnt[op_select])
		rel = rewrite(sql, rel, &rel_push_select_down_union, &changes); 

	if (gp.cnt[op_groupby]) {
		rel = rewrite_topdown(sql, rel, &rel_push_aggr_down, &changes);
		rel = rewrite(sql, rel, &rel_groupby_order, &changes); 
		rel = rewrite(sql, rel, &rel_reduce_groupby_exps, &changes); 
	}

	if (gp.cnt[op_join] || gp.cnt[op_left] || 
	    gp.cnt[op_semi] || gp.cnt[op_anti]) {
		rel = rel_remove_empty_join(sql, rel, &changes);
		if (!gp.cnt[op_update])
			rel = rewrite(sql, rel, &rel_join_order, &changes); 
		rel = rewrite(sql, rel, &rel_push_join_down_union, &changes); 
		/* rel_join_order may introduce empty selects */
		rel = rewrite(sql, rel, &rel_remove_empty_select, &e_changes); 
	}

	if (gp.cnt[op_select] && sql->emode != m_prepare) 
		rel = rewrite(sql, rel, &rel_simplify_like_select, &changes); 

	if (gp.cnt[op_select]) 
		rel = rewrite(sql, rel, &rel_select_order, &changes); 

	if (gp.cnt[op_select] || gp.cnt[op_join])
		rel = rewrite(sql, rel, &rel_use_index, &changes); 

	if (gp.cnt[op_project])
		rel = rewrite(sql, rel, &rel_push_project_down_union, &changes);

	if (gp.cnt[op_join] || 
	    gp.cnt[op_left] || gp.cnt[op_right] || gp.cnt[op_full] || 
	    gp.cnt[op_semi] || gp.cnt[op_anti] ||
	    gp.cnt[op_select]) 
		rel = rewrite(sql, rel, &rel_push_func_down, &changes);

	if (!changes && gp.cnt[op_topn]) {
		rel = rewrite_topdown(sql, rel, &rel_push_topn_down, &changes); 
		changes = 0;
	}

	rel = rewrite(sql, rel, &rel_merge_table_rewrite, &changes);

	/* Remove unused expressions */
	rel = rel_dce(sql, rel);

	if (changes && level > 10) {
		assert(0);
		return rel;
	}

	if (changes)
		return _rel_optimizer(sql, rel, ++level);

	/* optimize */
	return rel;
}


sql_rel *
rel_optimizer(mvc *sql, sql_rel *rel) 
{
	sql_rel *ret = _rel_optimizer(sql, rel, 0);
	
	if(!sql->q_in_q)
	{
		node* n = NULL;
		list* list_PERPAD = NULL;
		sel_predicate** sps = NULL;
		int num_PERPAD = 0, i;
		int num_pkeys_to_be_enumerated = 0;
		int* is_pkey_to_be_enumerated;
		discovered_table_pkeys = list_create(NULL);
		
		sql->q_in_q = 1;
		
		list_PERPAD = collect_PERPAD(sql, ret);
		
		printf("num_discovered_tables: %d\n", list_length(discovered_table_pkeys));
		for (n = discovered_table_pkeys->h; n; n = n->next) 
		{
			table_pkeys *tp = n->data;
			printf("num_pkey_columns: %d\n", list_length(tp->pkey_column_names));
		}
		printf("num_PERPAD: %d\n", num_PERPAD=list_length(list_PERPAD));
		
		sps = convert_all_into_in_clause_except_cmp_equal(list_PERPAD);
		
		/* enumerate the pkey space into a temp table */
		is_pkey_to_be_enumerated = enumerate_and_insert_into_temp_table(sql, sps, num_PERPAD);
		
		for(i = 0; i < num_PERPAD; i++)
		{
			if(is_pkey_to_be_enumerated[i])
				num_pkeys_to_be_enumerated++;
		}
		
		find_out_pkey_space_for_unavailable_required_derived_metadata(sql, list_PERPAD, is_pkey_to_be_enumerated, num_pkeys_to_be_enumerated);
		
		compute_and_insert_unavailable_required_derived_metadata(sql, sps, num_PERPAD, is_pkey_to_be_enumerated, num_pkeys_to_be_enumerated);
		
		sql->q_in_q = 0;
	}
	
	return ret;
}
