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

#ifndef _CRACKERS_HOLISTIC_H_
#define _CRACKERS_HOLISTIC_H_

#include "mal.h"
#include "mal_client.h"

#define LOCKS 64

typedef struct frequency{

	int 		bid;   		/* The base BAT */
	int 		c;		/*number of pieces in the index*/
	int 		f1;		/*number of queries that triggered cracking*/
	int 		f2;		/*number of queries that did not trigger cracking(because the value already existed in the index)*/
	double 		weight;
	//MT_Lock		nodeLock;
	struct frequency *next;

}FrequencyNode;

typedef  str (*IdleFuncPtr)(int *);
crackers_export MT_Lock frequencylock;
crackers_export MT_Lock CRKIndexLock[LOCKS];
//crackers_export MT_Lock CRKIndexLock;
crackers_export pthread_rwlock_t CRKFirstPieceRWLock;

crackers_export FrequencyNode *getFrequencyStruct(char which);
crackers_export void push(int bat_id,FrequencyNode* head);
crackers_export void printFrequencyStruct(FrequencyNode* head);
crackers_export FrequencyNode* searchBAT(FrequencyNode* head,int bat_id);
crackers_export double changeWeight_1(FrequencyNode* node,int N,int L1);
crackers_export double changeWeight_2(FrequencyNode* node,int N,int L1);
crackers_export double changeWeight_3(FrequencyNode* node,int N,int L1);
crackers_export double changeWeight(FrequencyNode* node,int N,int L1);
crackers_export FrequencyNode* findMax(FrequencyNode* head);
crackers_export FrequencyNode* findOtherMax(FrequencyNode* head,int bat_id);
crackers_export FrequencyNode* pickRandom(FrequencyNode* head);
crackers_export IdleFuncPtr IdleFunc;

crackers_export str CRKinitHolistic(int *ret);
crackers_export str CRKinitHolisticUpdates(int *ret);
crackers_export str CRKinitFrequencyStruct(int *vid,int bid);
crackers_export str CRKrandomCrack(int *ret);
crackers_export str CRKrandomCrackST(int *ret);
crackers_export str CRKrandomCrackUpdates(int *ret);
#endif /*crackers_holistic*/ 
