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

#ifndef _GTIFF_
#define _GTIFF_
#include "mal.h"
#include "mal_client.h"

#ifdef WIN32
#ifndef LIBGTIFF
#define geotiff_export extern __declspec(dllimport)
#else
#define geotiff_export extern __declspec(dllexport)
#endif
#else
#define geotiff_export extern
#endif

geotiff_export str GTIFFtest(int *wid, int *len, str *fname);
geotiff_export str GTIFFattach(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
geotiff_export str GTIFFloadGreyscaleImage(bat *x, bat *y, bat *intensity, str *fname);
geotiff_export str GTIFFimportImage(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci);
#endif

