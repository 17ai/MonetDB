/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://monetdb.cwi.nl/Legal/MonetDBLicense-1.1.html
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
 * Copyright August 2008-2010 MonetDB B.V.
 * All Rights Reserved.
 */

#include "sql_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <gdk.h>
#include <mal_sabaoth.h>
#include <Mapi.h>

#include "merovingian.h"
#include "discoveryrunner.h"
#include "multiplex-funnel.h"

multiplex *
multiplexInit(char *database)
{
	multiplex *m = malloc(sizeof(multiplex));
	char *p, *q;
	int i;
	char buf[1024];

	/* the multiplex targets are given separated by commas, split them
	 * out in multiplex_database entries */
	/* user+pass@pattern,user+pass@pattern,... */

	Mfprintf(stdout, "building multiplexer for %s\n", database);

	m->tid = 0;
	m->pool = strdup(database);
	m->dbcc = 1;
	p = m->pool;
	while ((p = strchr(p, ',')) != NULL) {
		m->dbcc++;
		p++;
	}
	m->dbcv = malloc(sizeof(multiplex_database *) * m->dbcc);
	p = m->pool;
	i = 0;
	while ((q = strchr(p, ',')) != NULL) {
		m->dbcv[i] = malloc(sizeof(multiplex_database));
		m->dbcv[i]->user = malloc(sizeof(char) * (q - p + 1));
		memcpy(m->dbcv[i]->user, p, q - p);
		m->dbcv[i]->user[q - p] = '\0';
		if ((p = strchr(m->dbcv[i]->user, '+')) == NULL) {
			Mfprintf(stderr, "skipping target %s, missing '+'\n",
					m->dbcv[i]->user);
			free(m->dbcv[i]->user);
			free(m->dbcv[i]);
			continue;
		}
		*p = '\0';
		m->dbcv[i]->pass = p + 1;
		if ((p = strchr(m->dbcv[i]->pass, '@')) == NULL) {
			Mfprintf(stderr, "skipping target %s+%s, missing '@'\n",
					m->dbcv[i]->user, m->dbcv[i]->pass);
			free(m->dbcv[i]->user);
			free(m->dbcv[i]);
			continue;
		}
		*p = '\0';
		m->dbcv[i]->database = p + 1;

		i++;
		p = q + 1;
	}
	m->dbcv[i] = malloc(sizeof(multiplex_database));
	m->dbcv[i]->user = strdup(p);
	if ((p = strchr(m->dbcv[i]->user, '+')) == NULL) {
		Mfprintf(stderr, "skipping target %s, missing '+'\n",
				m->dbcv[i]->user);
		free(m->dbcv[i]->user);
		free(m->dbcv[i]);
		i--;
	} else {
		*p = '\0';
		m->dbcv[i]->pass = p + 1;
		if ((p = strchr(m->dbcv[i]->pass, '@')) == NULL) {
			Mfprintf(stderr, "skipping target %s:%s, missing '@'\n",
					m->dbcv[i]->user, m->dbcv[i]->pass);
			free(m->dbcv[i]->user);
			free(m->dbcv[i]);
			i--;
		} else {
			*p = '\0';
			m->dbcv[i]->database = p + 1;
		}
	}
	/* correct size in case we dropped some entries */
	m->dbcc = i + 1;

	for (i = 0; i < m->dbcc; i++) {
		sabdb *stats = getRemoteDB(m->dbcv[i]->database);
		if (stats == NULL) {
			Mfprintf(stderr, "target %s cannot be resolved\n",
					m->dbcv[i]->database);
			continue;
		}
		snprintf(buf, sizeof(buf), "%s%s", stats->conns->val, stats->dbname);
		Mfprintf(stdout, "setting up multiplexer target %s->%s\n",
				m->dbcv[i]->database, buf);
		m->dbcv[i]->conn = mapi_mapiuri(buf,
				m->dbcv[i]->user, m->dbcv[i]->pass, "sql");
		SABAOTHfreeStatus(&stats);
	}

	m->clients = NULL; /* initially noone is connected */

	return(m);
}

void
multiplexQuery(multiplex *m, char *buf, stream *fout)
{
	int i;
	MapiHdl hdl[m->dbcc];
	char *t;
	mapi_int64 rlen;
	int fcnt;

	/* first send the query to all, such that we don't waste time
	 * waiting for each server to produce an answer, but wait for all of
	 * them concurrently */
	for (i = 0; i < m->dbcc; i++) {
		if (!mapi_is_connected(m->dbcv[i]->conn) &&
				mapi_reconnect(m->dbcv[i]->conn) != MOK)
		{
			mnstr_printf(fout, "!failed to establish connection for %s: %s\n",
					m->dbcv[i]->database, mapi_error_str(m->dbcv[i]->conn));
			mnstr_flush(fout);
			Mfprintf(stderr, "mapi_reconnect for %s failed: %s\n",
					m->dbcv[i]->database, mapi_error_str(m->dbcv[i]->conn));
			return;
		}

		hdl[i] = mapi_query(m->dbcv[i]->conn, buf);
	}
	/* fail as soon as one of the servers fails */
	t = NULL;
	rlen = 0;
	fcnt = -1;
	/* only support Q_TABLE, because appending is easy */
	for (i = 0; i < m->dbcc; i++) {
		if ((t = mapi_result_error(hdl[i])) != NULL) {
			mnstr_printf(fout, "!node %s failed: %s\n",
					m->dbcv[i]->database, t);
			Mfprintf(stderr, "mapi_result_error for %s: %s\n",
					m->dbcv[i]->database, t);
			break;
		}
		if (mapi_get_querytype(hdl[i]) != Q_TABLE) {
			t = "err"; /* for cleanup code below */
			mnstr_printf(fout, "!node %s returned a non-table result\n",
					m->dbcv[i]->database);
			Mfprintf(stderr, "querytype != Q_TABLE for %s: %d\n",
					m->dbcv[i]->database, mapi_get_querytype(hdl[i]));
			break;
		}
		rlen += mapi_get_row_count(hdl[i]);
		if (fcnt == -1) {
			fcnt = mapi_get_field_count(hdl[i]);
		} else {
			if (mapi_get_field_count(hdl[i]) != fcnt) {
				t = "err"; /* for cleanup code below */
				mnstr_printf(fout, "!node %s has mismatch in result fields\n",
						m->dbcv[i]->database);
				Mfprintf(stderr, "mapi_get_field_count inconsistent for %s: "
						"got %d, expected %d\n",
						m->dbcv[i]->database,
						mapi_get_field_count(hdl[i]), fcnt);
				break;
			}
		}
	}
	if (t != NULL) {
		mnstr_flush(fout);
		for (i = 0; i < m->dbcc; i++)
			mapi_close_handle(hdl[i]);
		return;
	}
	/* Compose the header, the tricky part in here is that mapi doesn't
	 * give us access to everything.  Because it takes too much, we just
	 * don't give any size estimation (length).  Same for the table id,
	 * we just send 0, such that we never get a close request. */
	mnstr_printf(fout, "&%d 0 %d %d %d\n", Q_TABLE, rlen, fcnt, rlen);
	mnstr_write(fout, "% ", 2, 1);
	for (i = 0; i < fcnt; i++) {
		if (i > 0)
			mnstr_write(fout, ",\t", 2, 1);
		mnstr_printf(fout, "%s", mapi_get_table(hdl[0], i));
	}
	mnstr_write(fout, " # table_name\n", 14, 1);
	mnstr_write(fout, "% ", 2, 1);
	for (i = 0; i < fcnt; i++) {
		if (i > 0)
			mnstr_write(fout, ",\t", 2, 1);
		mnstr_printf(fout, "%s", mapi_get_name(hdl[0], i));
	}
	mnstr_write(fout, " # name\n", 8, 1);
	mnstr_write(fout, "% ", 2, 1);
	for (i = 0; i < fcnt; i++) {
		if (i > 0)
			mnstr_write(fout, ",\t", 2, 1);
		mnstr_printf(fout, "%s", mapi_get_type(hdl[0], i));
	}
	mnstr_write(fout, " # type\n", 8, 1);
	mnstr_write(fout, "% ", 2, 1);
	for (i = 0; i < fcnt; i++) {
		if (i > 0)
			mnstr_write(fout, ",\t", 2, 1);
		mnstr_write(fout, "0", 1, 1);
	}
	mnstr_write(fout, " # length\n", 10, 1);
	/* now read the answers, and write them directly to the client */
	for (i = 0; i < m->dbcc; i++) {
		while ((t = mapi_fetch_line(hdl[i])) != NULL)
			if (*t != '%') /* skip server's headers */
				mnstr_write(fout, t, strlen(t), 1);
	}
	mnstr_flush(fout);
	/* finish up */
	for (i = 0; i < m->dbcc; i++)
		mapi_close_handle(hdl[i]);
}

void
multiplexThread(void *d)
{
	multiplex *m = (multiplex *)d;
	struct timeval tv;
	fd_set fds;
	multiplex_client *c;
	int msock = -1;
	char buf[8096];
	int r;

	/* select on upstream clients, on new data, read query, forward,
	 * union all results, send back, and restart cycle. */
	
	buf[8095] = '\0';
	while (_mero_keep_listening == 1) {
		FD_ZERO(&fds);
		for (c = m->clients; c != NULL; c = c->next) {
			FD_SET(c->sock, &fds);
			if (c->sock > msock)
				msock = c->sock;
		}
		/* wait up to 5 seconds. */
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		r = select(msock + 1, &fds, NULL, NULL, &tv);
		/* nothing interesting has happened */
		if (r == 0)
			continue;
		for (c = m->clients; c != NULL; c = c->next) {
			if (!FD_ISSET(c->sock, &fds))
				continue;
			if (mnstr_read_block(c->fdin, buf, 8095, 1) < 0) {
				/* error, or some garbage */
				multiplexRemoveClient(m, c);
				continue;
			}
			/* we assume (and require) the query to fit in one block,
			 * that is, we only forward the first block, without having
			 * any idea what it is */
			multiplexQuery(m, buf, c->fout);
		}
	}
}

void
multiplexAddClient(multiplex *m, int sock, stream *fout, stream *fdin, char *name)
{
	multiplex_client *w;
	multiplex_client *n = malloc(sizeof(multiplex_client));

	n->sock = sock;
	n->fdin = fdin;
	n->fout = fout;
	n->name = strdup(name);
	n->next = NULL;

	if (m->clients == NULL) {
		m->clients = n;
	} else {
		for (w = m->clients; w->next != NULL; w = w->next);
		w->next = n;
	}

	Mfprintf(stdout, "added new client %s for multiplexer %s\n",
			n->name, m->pool);

	/* send client a prompt */
	mnstr_flush(fout);
}

void
multiplexRemoveClient(multiplex *m, multiplex_client *c)
{
	multiplex_client *w;
	multiplex_client *p = NULL;

	Mfprintf(stdout, "removing client %s for multiplexer %s\n",
			c->name, m->pool);

	for (w = m->clients; w != NULL; w = w->next) {
		if (w == c) {
			if (w == m->clients) {
				m->clients = w->next;
			} else {
				p->next = w->next;
			}
			c->next = NULL;
			close_stream(c->fdin);
			close_stream(c->fout);
			close(c->sock);
			free(c->name);
			free(c);
			break;
		}
	}
}

/* vim:set ts=4 sw=4 noexpandtab: */
