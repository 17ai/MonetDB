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
 *                * Copyright August 2008-2014 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * (c)2014 author Martin Kersten
 */

#ifndef _MOSAIC_HDR_
#define _MOSAIC_HDR_

#include <mal.h>
#include "mal_interpreter.h"
#include "mal_client.h"

mosaic_export void MOSdumpHeader(Client cntxt, MOStask task);
mosaic_export void MOSupdateHeader(Client cntxt, MOStask task);
mosaic_export void MOSinitHeader(MOStask task);
mosaic_export BUN MOSfindChunk(Client cntxt, MOStask task, oid o);
#endif /* _MOSAIC_HDR_ */
