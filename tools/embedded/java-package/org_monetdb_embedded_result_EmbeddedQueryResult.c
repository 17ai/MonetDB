/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 2008-2015 MonetDB B.V.
 */

#include "org_monetdb_embedded_result_EmbeddedQueryResult.h"
#include "embedded.h"

JNIEXPORT jobject JNICALL Java_org_monetdb_embedded_result_EmbeddedQueryResult_getColumnWrapper
(JNIEnv *env, jobject object, jlong resultTablePointer, jint columnIndex) {
	// The result table
	res_table* result = (res_table *)resultTablePointer;
	// Get the column we need
	res_col col = output->cols[columnIndex];
	BAT* b = BATdescriptor(col.b);
	int size = BATcount(b);
	// The values and nulls arrays
	jintArray values = (*env)->NewIntArray(env, size);
	jbooleanArray nulls =  (*env)->NewBooleanArray(env, size);

	jobect *column;
	jclass columnClass = (*env)->FindClass(env, "org/monetdb/embedded/result/column/IntegerColumn");
	// from Java IntegerColumn(int[] values, int columnSize, boolean[] nullIndex)
	jmethodID columnConstructor = (*env)->GetMethodID(env, columnClass, "<init>", "([II[Z)V");

	int i = 0;
	int val_tmp[size];
	jboolean nul_tmp[size];
	if (b->T->nonil && !b->T->nil) {
		for (i = 0; i < size; i++) {
			val_tmp[i] = (int) ((int*) Tloc(b, BUNfirst(b)))[i];
		}
	}
	else {
		for (i = 0; i < size; i++) {
			int v = ((int*) Tloc(b, BUNfirst(b)))[i];
			if (v == int##_nil) {
				val_tmp[i] = 0;
				nul_tmp = JNI_TRUE;

			} else {
				val_tmp[i] = (int)v;
				nul_tmp = JNI_FALSE;
			}
		}
	}
	// Move from the tmp C arrays to a Java arrays
	(*env)->SetIntArrayRegion(env, values, 0, size, val_tmp);
	(*env)->SetBooleanArrayRegion(env, nulls, 0, size, nul_tmp);

	// Create the column object
	// from Java IntegerColumn(int[] values, int columnSize, boolean[] nullIndex)
	result = (*env)->NewObject(env, columnClass, columnConstructor, values, size, nulls);

	return column;
}

JNIEXPORT void JNICALL Java_org_monetdb_embedded_result_EmbeddedQueryResult_cleanupResult
(JNIEnv *env, jobject object, jlong resultTablePointer) {
	res_table* result = (res_table *)resultTablePointer;

	monetdb_cleanup_result(result);
}
