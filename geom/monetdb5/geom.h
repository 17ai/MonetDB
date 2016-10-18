/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/*
 * @a Wouter Scherphof, Niels Nes, Foteini Alvanaki
 */

#include <monetdb_config.h>
#include "libgeom.h"

#include <mal.h>
#include <mal_interpreter.h>
#include <mal_atom.h>
#include <mal_exception.h>
#include <mal_client.h>
#include <stream.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <gdk_logger.h>

#ifdef WIN32
#ifndef LIBGEOM
#define geom_export extern __declspec(dllimport)
#else
#define geom_export extern __declspec(dllexport)
#endif
#else
#define geom_export extern
#endif

#define OPENCL_DYNAMIC 1
#define OPENCL_THREADS 8
#define OPENMP  1

#ifdef OPENMP
#include <omp.h>
#endif

#define BATrmprops(b)                                           \
    b->tsorted = b->trevsorted = 0;     \
    b->tnosorted = b->tnorevsorted = 0;                         \
    b->tkey |= 0;                                               \
    b->tnokey[0] = b->tnokey[1] = 0;                            \
    b->tnonil = 0;                                             \
    b->tnodense = 1;                                            

/*Necessary for external functionality*/
geom_export str numPointsLineString(unsigned int *out, const GEOSGeometry *geosGeometry);

/* general functions */
geom_export str geoHasZ(int* res, int* info);
geom_export str geoHasM(int* res, int* info);
geom_export str geoGetType(char** res, int* info, int* flag);
geom_export str GEOSGeomGetZ(const GEOSGeometry *geom, double *z);

geom_export str geom_prelude(void *ret);
geom_export str geom_epilogue(void *ret);

/* the len argument is needed for correct storage and retrieval */
geom_export int wkbTOSTR(char **geomWKT, int *len, wkb *geomWKB);
geom_export int mbrTOSTR(char **dst, int *len, mbr *atom);
geom_export int wkbaTOSTR(char **toStr, int* len, wkba *fromArray);

geom_export int wkbFROMSTR(char* geomWKT, int *len, wkb** geomWKB);
geom_export int mbrFROMSTR(char *src, int *len, mbr **atom);
geom_export int wkbaFROMSTR(char *fromStr, int *len, wkba **toArray);

geom_export wkb *wkbNULL(void);
geom_export mbr *mbrNULL(void);
geom_export wkba *wkbaNULL(void);

geom_export BUN wkbHASH(wkb *w);
geom_export BUN mbrHASH(mbr *atom);
geom_export BUN wkbaHASH(wkba *w);

geom_export int wkbCOMP(wkb *l, wkb *r);
geom_export int mbrCOMP(mbr *l, mbr *r);
geom_export int wkbaCOMP(wkba *l, wkba *r);

/* read/write to/from log */
geom_export wkb *wkbREAD(wkb *a, stream *s, size_t cnt);
geom_export mbr *mbrREAD(mbr *a, stream *s, size_t cnt);
geom_export wkba* wkbaREAD(wkba *a, stream *s, size_t cnt);

geom_export gdk_return wkbWRITE(wkb *a, stream *s, size_t cnt);
geom_export gdk_return mbrWRITE(mbr *c, stream *s, size_t cnt);
geom_export gdk_return wkbaWRITE(wkba *c, stream *s, size_t cnt);

geom_export var_t wkbPUT(Heap *h, var_t *bun, wkb *val);
geom_export var_t wkbaPUT(Heap *h, var_t *bun, wkba *val);

geom_export void wkbDEL(Heap *h, var_t *index);
geom_export void wkbaDEL(Heap *h, var_t *index);

geom_export int wkbLENGTH(wkb *p);
geom_export int wkbaLENGTH(wkba *p);

geom_export void wkbHEAP(Heap *heap, size_t capacity);
geom_export void wkbaHEAP(Heap *heap, size_t capacity);

geom_export str mbrFromString(mbr **w, str *src);
geom_export str wkbIsnil(bit *r, wkb **v);

/* functions that are used when a column is added to an existing table */
geom_export str mbrFromMBR(mbr **w, mbr **src);
geom_export str wkbFromWKB(wkb **w, wkb **src);
//Is it needed?? geom_export str wkbFromWKB_bat(bat* outBAT_id, bat* inBAT_id);

/* The WKB we use is the EWKB used also in PostGIS 
 * because we decided that it is easire to carry around
 * the SRID */
 
/* gets a GEOSGeometry and returns the mbr of it 
 * works only for 2D geometries */
geom_export mbr* mbrFromGeos(const GEOSGeom geosGeometry);
geom_export wkb* geos2wkb(const GEOSGeometry *geosGeometry);


geom_export str wkbFromText(wkb **geomWKB, str *geomWKT, int* srid, int *tpe);
geom_export str wkbFromText_bat(bat *outBAT_id, bat *inBAT_id, int *srid, int *tpe);

geom_export str wkbMLineStringToPolygon(wkb** geomWKB, str* geomWKT, int* srid, int* flag);


/* Basic Methods on Geometric objects (OGC) */
geom_export str wkbDimension(int*, wkb**);
geom_export str wkbDimension_bat(bat *inBAT_id, bat *outBAT_id);

geom_export str wkbGeometryType(char**, wkb**, int*);
geom_export str wkbGeometryType_bat(bat *inBAT_id, bat *outBAT_id, int *flag);

geom_export str wkbGetSRID(int*, wkb**);
//Envelope
geom_export str wkbAsText(char **outTXT, wkb **inWKB, int *withSRID);
geom_export str wkbAsText_bat(bat *inBAT_id, bat *outBAT_id, int *withSRID);

geom_export str wkbAsBinary(char**, wkb**);
geom_export str wkbFromBinary(wkb**, char**);

geom_export str wkbIsEmpty(bit*, wkb**);
geom_export str wkbIsEmpty_bat(bat *inBAT_id, bat *outBAT_id);

geom_export str wkbIsSimple(bit*, wkb**);
geom_export str wkbIsSimple_bat(bat *inBAT_id, bat *outBAT_id);
//Is3D
//IsMeasured
geom_export str wkbBoundary(wkb **outWKB, wkb **inWKB);
geom_export str wkbBoundary_bat(bat *inBAT_id, bat *outBAT_id);


/* Methods for testing spatial relatioships between geometris (OGC) */
geom_export str wkbEquals(bit*, wkb**, wkb**);
geom_export str wkbDisjoint(bit*, wkb**, wkb**);
geom_export str wkbIntersects(bit*, wkb**, wkb**);
geom_export str wkbIntersectsXYZ(bit*, wkb**, dbl*, dbl*, dbl*, int*);
geom_export str wkbIntersects_bat(bat *outBAT_id, bat *aBAT_id, bat *bBAT_id, wkb **b);
geom_export str wkbIntersectsXYZ_bat(bat *outBAT_id, bat *inBAT_id, bat *inXBAT_id, double *dx, bat *inYBAT_id, double *dy, bat *inZBAT_id, double *dz, int* srid);
geom_export str wkbTouches(bit*, wkb**, wkb**);
geom_export str wkbCrosses(bit*, wkb**, wkb**);
geom_export str wkbIsType_bat(bat *outBAT_id, bat *aBAT_id, bat *bBAT_id, str *b);
geom_export str wkbWithin(bit*, wkb**, wkb**);
geom_export str wkbWithin_bat(bat *outBAT_id, bat *aBAT_id, bat *bBAT_id, wkb **b);
geom_export str wkbContains(bit*, wkb**, wkb**);
geom_export str wkbContainsXYZ(bit *out, wkb **a, dbl *px, dbl *py, dbl *pz, int *srid);
geom_export str wkbOverlaps(bit*, wkb**, wkb**);
geom_export str wkbRelate(bit*, wkb**, wkb**, str*);
geom_export str wkbCovers(bit *out, wkb **geomWKB_a, wkb **geomWKB_b);
geom_export str wkbCoveredBy(bit *out, wkb **geomWKB_a, wkb **geomWKB_b);
geom_export str wkbDWithin(bit*, wkb**, wkb**, dbl*);
geom_export str wkbDWithinXYZ(bit*, wkb**, dbl*, dbl*, dbl*, int*, dbl*);
geom_export str wkbDWithin_bat(bat *outBAT_id, bat *aBAT_id, bat *bBAT_id, dbl*);
geom_export str wkbDWithinXYZ_bat(bat *outBAT_id, bat *inBAT_id, bat *inXBAT_id, double *dx, bat *inYBAT_id, double *dy, bat *inZBAT_id, double *dz, int* srid, dbl* dist);

typedef struct {
	int nvert;
    int nholes;
	double *vert_x;
    double *vert_y;
    double **holes_x;
    double **holes_y;
    int *holes_n;
} vertexWKB;

geom_export str getVerts(wkb *geom, vertexWKB **res);
geom_export void freeVerts(vertexWKB *verts);
//LocateAlong
//LocateBetween

//geom_export str wkbFromString(wkb**, str*); 

geom_export str wkbMakePoint(wkb**, dbl*, dbl*, dbl*, dbl*, int*);
geom_export str wkbMakePoint_bat(bat*, bat*, bat*, bat*, bat*, int*);

geom_export str wkbCoordDim(int* , wkb**);
geom_export str wkbSetSRID(wkb**, wkb**, int*);

geom_export str wkbGetCoordinate(dbl *out, wkb **geom, int *dimNum);
geom_export str wkbGetCoordinate_bat(bat *outBAT_id, bat *inBAT_id, int* flag);

geom_export str wkbStartPoint(wkb **out, wkb **geom);
geom_export str wkbEndPoint(wkb **out, wkb **geom);

geom_export str wkbNumPoints(int *out, wkb **geom, int *check);
geom_export str wkbNumPoints_bat(bat *outBAT_id, bat *inBAT_id, int* flag);
geom_export str numPointsGeometry(unsigned int *out, const GEOSGeometry* geosGeometry);

geom_export str wkbPointN(wkb **out, wkb **geom, int *n);
geom_export str wkbEnvelope(wkb **out, wkb **geom);
geom_export str wkbEnvelopeFromCoordinates(wkb** out, dbl* xmin, dbl* ymin, dbl* xmax, dbl* ymax, int* srid);
geom_export str wkbMakePolygon(wkb** out, wkb** external, const int* srid); /*Only Polygons without holes*/
geom_export str wkbMakePolygon_bat(bat *outBAT_id, bat *inBAT_id, const int* srid); /*Only Polygons without holes*/
geom_export str wkbMakeLine(wkb**, wkb**, wkb**);
geom_export str wkbMakeLineAggr(wkb** outWKB, bat* inBAT_id);
geom_export str wkbsubMakeLine(bat *outBAT_id, bat* bBAT_id, bat *gBAT_id, bat *eBAT_id, bit* fla);
geom_export str wkbExteriorRing(wkb**, wkb**);
geom_export str wkbInteriorRingN(wkb**, wkb**, int*);

geom_export str wkbNumRings(int*, wkb**, int*);
geom_export str wkbNumRings_bat(bat *outBAT_id, bat *inBAT_id, int* flag);

geom_export str wkbInteriorRings(wkba**, wkb**);

geom_export str wkbIsClosed(bit *out, wkb **geom);
geom_export str wkbIsClosed_bat(bat *inBAT_id, bat *outBAT_id);

geom_export str wkbIsRing(bit *out, wkb **geom);
geom_export str wkbIsRing_bat(bat *inBAT_id, bat *outBAT_id);

geom_export str wkbIsValid(bit *out, wkb **geom);
geom_export str wkbIsValid_bat(bat *inBAT_id, bat *outBAT_id);

geom_export str wkbIsValidReason(char** out, wkb **geom);
geom_export str wkbIsValidDetail(char** out, wkb **geom);

geom_export str wkbArea(dbl *out, wkb **a);
geom_export str wkbArea_bat(bat *inBAT_id, bat *outBAT_id);
geom_export str wkbCentroid(wkb **out, wkb **geom);
geom_export str wkbCentroid_bat(bat *outBAT_id, bat *inBAT_id);
geom_export str wkbDistance(dbl *out, wkb **a, wkb **b);
geom_export str wkbDistanceXYZ(dbl *out, wkb **a, dbl *x, dbl *y, dbl *z, int *srid);
geom_export str wkbDistanceXYZ_bat(bat *outBAT_id, bat *inBAT_id, bat *inXBAT_id, double *dx, bat *inYBAT_id, double *dy, bat *inZBAT_id, double *dz, int* srid);
geom_export str wkbLength(dbl *out, wkb **a);
geom_export str wkbConvexHull(wkb **out, wkb **geom);
geom_export str wkbIntersection(wkb **out, wkb **a, wkb **b);
geom_export str wkbIntersection_bat(bat *outBAT_id, bat *aBAT_id, bat *bBAT_id);
geom_export str wkbIntersection_bat_s(bat *outBAT_id, bat *aBAT_id, bat *bBAT_id, bat *saBAT_id, bat *sbBAT_id);
geom_export str wkbUnion(wkb **out, wkb **a, wkb **b);
geom_export str wkbUnionAggr(wkb** outWKB, bat* inBAT_id);
geom_export str wkbUnionCascade(wkb** outWKB, bat* inBAT_id);
geom_export str wkbsubUnion(bat *outBAT_id, bat* bBAT_id, bat *gBAT_id, bat *eBAT_id, bit* flag);
geom_export str wkbCollect(wkb **out, wkb **a, wkb **b);
geom_export str wkbCollectCascade(wkb** outWKB, bat* inBAT_id);
geom_export str wkbsubCollect(bat *outBAT_id, bat* bBAT_id, bat *gBAT_id, bat *eBAT_id, bit* flag);
geom_export str wkbDifference(wkb **out, wkb **a, wkb **b);
geom_export str wkbSymDifference(wkb **out, wkb **a, wkb **b);
geom_export str wkbBuffer(wkb **out, wkb **geom, dbl *distance);

geom_export str wkbGeometryN(wkb** out, wkb** geom, const int* geometryNum); 
geom_export str wkbGeometryN_bat(bat *outBAT_id, bat *inBAT_id, const int* flag);

geom_export str wkbNumGeometries(int* out, wkb** geom);
geom_export str wkbNumGeometries_bat(bat *outBAT_id, bat *inBAT_id);

geom_export str wkbAddPoint(wkb**, wkb**, wkb**);
geom_export str wkbAddPointIdx(wkb**, wkb**, wkb**, int* idx);

geom_export str wkbTransform(wkb**, wkb**, int*, int*, char**, char**);
geom_export str wkbTranslate(wkb**, wkb**, dbl*, dbl*, dbl*);
geom_export str wkbTranslate_bat(bat *outBAT_id, bat *inBAT_id, bat *inXBAT_id, double *dx, bat *inYBAT_id, double *dy, bat *inZBAT_id, double *dz);
geom_export str wkbDelaunayTriangles(wkb**, wkb**, dbl*, int*);
geom_export str wkbPointOnSurface(wkb**, wkb**);
geom_export str wkbForceDim(wkb**, wkb**, const int*);
geom_export str wkbForceDim_bat(bat *outBAT_id, bat *inBAT_id, const int*);
geom_export str wkbSegmentize(wkb**, wkb**, dbl*);
geom_export str wkbSegmentize_bat(bat *outBAT_id, bat *inBAT_id, double *flag);

geom_export str wkbDump(bat* idBAT_id, bat* geomBAT_id, wkb**);
geom_export str wkbDump_bat(bat* idBAT_id, bat* geomBAT_id, bat* wkbBAT_id);
geom_export str wkbDumpP(bat* partentBAT_id, bat* idBAT_id, bat* geomBAT_id, wkb**, int* parent);
geom_export str wkbDumpP_bat(bat* partentBAT_id, bat* idBAT_id, bat* geomBAT_id, bat* wkbBAT_id, bat *parent);
geom_export str wkbDumpPoints(bat* idBAT_id, bat* geomBAT_id, wkb**);
geom_export str wkbDumpPoints_bat(bat* idBAT_id, bat* geomBAT_id, bat* wkbBAT_id);
geom_export str wkbDumpPointsP(bat* partentBAT_id, bat* idBAT_id, bat* geomBAT_id, wkb**, int* parent);
geom_export str wkbDumpPointsP_bat(bat* partentBAT_id, bat* idBAT_id, bat* geomBAT_id, bat* wkbBAT_id, bat *parent);
geom_export str wkbDumpRings(bat *idBAT_id, bat* geomBAT_id, wkb**);
geom_export str wkbDumpRings_bat(bat* idBAT_id, bat* geomBAT_id, bat* wkbBAT_id);
geom_export str wkbDumpRingsP(bat* partentBAT_id, bat *idBAT_id, bat* geomBAT_id, wkb**, int* parent);
geom_export str wkbDumpRingsP_bat(bat* partentBAT_id, bat* idBAT_id, bat* geomBAT_id, bat* wkbBAT_id, bat *parent);
geom_export str dumpGeometriesGeometry(BAT *idBAT, BAT *geomBAT, const GEOSGeometry *geosGeometry, uint32_t *num_geoms, const char *path);
geom_export str dumpPointsGeometry(BAT *idBAT, BAT *geomBAT, const GEOSGeometry *geosGeometry, uint32_t *num_points, const char *path);
geom_export str dumpRingsGeometry(BAT *idBAT, BAT *geomBAT, const GEOSGeometry *geosGeometry, uint32_t *num_rings, const char *path);
geom_export str wkbPolygonize(wkb **res, wkb **geom);
geom_export str wkbsubPolygonize(bat *outBAT_id, bat* bBAT_id, bat *gBAT_id, bat *eBAT_id, bit* flag);
geom_export str wkbSimplify(wkb **res, wkb **geom, float *tolerance);
geom_export str wkbSimplifyPreserveTopology(wkb **res, wkb **geom, float *tolerance);

geom_export str geom_2_geom(wkb** resWKB, wkb **valueWKB, int* columnType, int* columnSRID); 

geom_export str wkbMBR(mbr **res, wkb **geom);

geom_export str wkbBox2D(mbr** box, wkb** point1, wkb** point2);
geom_export str wkbBox2D_bat(bat* outBAT_id, bat *aBAT_id, bat *bBAT_id);

geom_export str mbrOverlaps(bit *out, mbr **b1, mbr **b2);
geom_export str mbrOverlaps_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrAbove(bit *out, mbr **b1, mbr **b2);
geom_export str mbrAbove_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrBelow(bit *out, mbr **b1, mbr **b2);
geom_export str mbrBelow_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrLeft(bit *out, mbr **b1, mbr **b2);
geom_export str mbrLeft_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrRight(bit *out, mbr **b1, mbr **b2);
geom_export str mbrRight_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrOverlapOrAbove(bit *out, mbr **b1, mbr **b2);
geom_export str mbrOverlapOrAbove_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrOverlapOrBelow(bit *out, mbr **b1, mbr **b2);
geom_export str mbrOverlapOrBelow_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrOverlapOrLeft(bit *out, mbr **b1, mbr **b2);
geom_export str mbrOverlapOrLeft_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrOverlapOrRight(bit *out, mbr **b1, mbr **b2);
geom_export str mbrOverlapOrRight_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrContains(bit *out, mbr **b1, mbr **b2);
geom_export str mbrContains_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrContained(bit *out, mbr **b1, mbr **b2);
geom_export str mbrContained_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrEqual(bit *out, mbr **b1, mbr **b2);
geom_export str mbrEqual_wkb(bit *out, wkb **geom1WKB, wkb **geom2WKB);
geom_export str mbrDistance(dbl *out, mbr **b1, mbr **b2);
geom_export str mbrDistance_wkb(dbl *out, wkb **geom1WKB, wkb **geom2WKB);

geom_export str wkbCoordinateFromWKB(dbl*, wkb**, int*);
geom_export str wkbCoordinateFromMBR(dbl*, mbr**, int*);

geom_export str ordinatesMBR(mbr **res, flt *minX, flt *minY, flt *maxX, flt *maxY);

/* BULK */

geom_export str wkbDistance_bat(bat* outBAT_id, bat*, bat*);
geom_export str wkbDistance_geom_bat(bat* outBAT_id, wkb** geomWKB, bat* inBAT_id);
geom_export str wkbDistance_bat_geom(bat* outBAT_id, bat* inBAT_id, wkb** geomWKB);

geom_export str wkbContains_bat(bat* outBAT_id, bat*, bat*);
geom_export str wkbContains_geom_bat(bat* outBAT_id, wkb** geomWKB, bat* inBAT_id);
geom_export str wkbContains_bat_geom(bat* outBAT_id, bat* inBAT_id, wkb** geomWKB);
geom_export str wkbContains_point_bat(bat *out, wkb **a, bat *point_x, bat *point_y);
geom_export str wkbContainsXYZ_bat(bat *outBAT_id, bat *inBAT_id, bat *inXBAT_id, double *dx, bat *inYBAT_id, double *dy, bat *inZBAT_id, double *dz, int* srid);

//geom_export str wkbFilter_bat(bat* aBATfiltered_id, bat* bBATfiltered_id, bat* aBAT_id, bat* bBAT_id);
geom_export str wkbFilter_geom_bat(bat* BATfiltered_id, wkb** geomWKB, bat* BAToriginal_id);
geom_export str wkbFilter_bat_geom(bat* BATfiltered_id, bat* BAToriginal_id, wkb** geomWKB);

geom_export str wkbMakeLine_bat(bat* outBAT_id, bat* aBAT_id, bat* bBAT_id);
geom_export str wkbUnion_bat(bat* outBAT_id, bat* aBAT_id, bat* bBAT_id);
geom_export str wkbCollect_bat(bat* outBAT_id, bat* aBAT_id, bat* bBAT_id);

geom_export str wkbSetSRID_bat(bat* outBAT_id, bat* inBAT_id, int* srid);

geom_export str geom_2_geom_bat(bat* outBAT_id, bat* inBAT_id, int* columnType, int* columnSRID);

geom_export str wkbMBR_bat(bat* outBAT_id, bat* inBAT_id);

geom_export str wkbCoordinateFromWKB_bat(bat *outBAT_id, bat *inBAT_id, int* coordinateIdx);
geom_export str wkbCoordinateFromMBR_bat(bat *outBAT_id, bat *inBAT_id, int* coordinateIdx);

geom_export int geom_catalog_upgrade(void *, int);
geom_export str geom_sql_upgrade(int);

/* To Export X3D and GeoJSON */
#define OUT_MAX_DOUBLE_PRECISION 15
#define OUT_MAX_DOUBLE 1E15
#define OUT_SHOW_DIGS_DOUBLE 20
#define OUT_MAX_DOUBLE_PRECISION 15
#define OUT_MAX_DIGS_DOUBLE (OUT_SHOW_DIGS_DOUBLE + 2)
#define GEOM_X3D_FLIP_XY         (1<<0)
#define GEOM_X3D_USE_GEOCOORDS   (1<<1)
#define X3D_USE_GEOCOORDS(x) ((x) & GEOM_X3D_USE_GEOCOORDS)

#define POLY_NUM_VERT 120
#define POLY_NUM_HOLE 10

typedef struct {
    double xmin; 
    double ymin; 
    double zmin; 
    double xmax; 
    double ymax; 
    double zmax; 
} bbox3D;

geom_export str bbox3DFromGeos(bbox3D **bbox, const GEOSGeom geosGeometry);
geom_export char *geom_to_geojson(GEOSGeom geom, char *srs, int precision, int has_bbox);
geom_export char *geom_to_x3d_3(GEOSGeom geom, int precision, int opts, const char *defid);
geom_export str wkbAsX3D(str *res, wkb **geom, int *maxDecDigits, int *options);
geom_export str wkbAsX3D_bat(bat *outBAT_id, bat *inBAT_id, int *maxDecDigits, int *options);
geom_export str wkbAsGeoJson(str *res, wkb **geom, int *maxDecDigits, int *options);
geom_export str wkbPatchToGeom(wkb **res, wkb **geom, dbl* px, dbl*py, dbl*pz);
geom_export str wkbPatchToGeom_bat(wkb **res, wkb **geom, bat* px, bat* py, bat* pz);

/*Filter functions - subselect*/
geom_export str IsValidsubselect(bat *lresBAT_id, bat *lBAT_id, bat *slBAT_id, bit *nil_matches);
geom_export str IsTypesubselect(bat *lresBAT_id, bat *lBAT_id, bat *slBAT_id, str *b, bit *nil_matches);
geom_export str Withinsubselect(bat *lresBAT_id, bat *lBAT_id, bat *slBAT_id, wkb **b, bit *nil_matches);
geom_export str Intersectssubselect(bat *lresBAT_id, bat *lBAT_id, bat *slBAT_id, wkb **b, bit *nil_matches);
geom_export str IntersectsXYZsubselect(bat *lresBAT_id, bat *lBAT_id, bat *slBAT_id, dbl *x, dbl *y, dbl *z, int *srid, bit *nil_matches);
geom_export str ContainsXYZsubselect(bat *lresBAT_id, bat *lBAT_id, bat *slBAT_id, dbl *x, dbl *y, dbl *z, int *srid, bit *nil_matches);

/*Filter joins - subjoin*/
geom_export str Intersectssubjoin(bat *lres, bat *rres, bat *lid, bat *rid, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str IntersectsXYZsubjoin(bat *lres, bat *rres, bat *lid, bat *xid, bat *yid, bat *zid, int *srid, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str DWithinsubjoin(bat *lres, bat *rres, bat *lid, bat *rid, double *dist, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str DWithinXYZsubjoin(bat *lres, bat *rres, bat *lid, bat *xid, bat *yid, bat *zid, int *srid, double *dist, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str DNNXYZsubjoin(bat *lres, bat *rres, bat *lid, bat *xid, bat *yid, bat *zid, int *srid, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str Containssubjoin(bat *lres, bat *rres, bat *lid, bat *rid, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str ContainsXYZsubjoin(bat *lres, bat *rres, bat *lid, bat *xid, bat *yid, bat *zid, int *srid, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str IsValidsubjoin(bat *lres, bat *rres, bat *lid, bat *rid, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
geom_export str IsTypesubjoin(bat *lres, bat *rres, bat *lid, bat *rid, bat *sl, bat *sr, bit *nil_matches, lng *estimate);
