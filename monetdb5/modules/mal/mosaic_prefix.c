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
 * Blocked bit_ prefix compression
 */

#include "monetdb_config.h"
#include "mosaic.h"
#include "mosaic_prefix.h"

/* Beware, the dump routines use the compressed part of the task */
void
MOSdump_prefix(Client cntxt, MOStask task)
{
	MosaicBlk blk= task->blk;
	void *val = (void*)(((char*) blk) + MosaicBlkSize);

	mnstr_printf(cntxt->fdout,"#rle "BUNFMT" ", MOSgetCnt(blk));
	switch(task->type){
	case TYPE_bte:
		mnstr_printf(cntxt->fdout,"bte %hhd", *(bte*) val); break;
	case TYPE_sht:
		mnstr_printf(cntxt->fdout,"sht %hd", *(sht*) val); break;
	case TYPE_int:
		mnstr_printf(cntxt->fdout,"int %d", *(int*) val); break;
	case  TYPE_oid:
		mnstr_printf(cntxt->fdout,"oid "OIDFMT, *(oid*) val); break;
	case  TYPE_lng:
		mnstr_printf(cntxt->fdout,"lng "LLFMT, *(lng*) val); break;
#ifdef HAVE_HGE
	case  TYPE_hge:
		mnstr_printf(cntxt->fdout,"hge %.40g", (dbl) *(hge*) val); break;
#endif
	case  TYPE_wrd:
		mnstr_printf(cntxt->fdout,"wrd "SZFMT, *(wrd*) val); break;
	case TYPE_flt:
		mnstr_printf(cntxt->fdout,"flt  %f", *(flt*) val); break;
	case TYPE_dbl:
		mnstr_printf(cntxt->fdout,"flt  %f", *(dbl*) val); break;
	case TYPE_str:
		mnstr_printf(cntxt->fdout,"str TBD"); break;
	default:
		if( task->type == TYPE_date)
			mnstr_printf(cntxt->fdout,"date %d ", *(int*) val); 
		if( task->type == TYPE_daytime)
			mnstr_printf(cntxt->fdout,"daytime %d ", *(int*) val);
		if( task->type == TYPE_timestamp)
			mnstr_printf(cntxt->fdout,"int "LLFMT, *(lng*) val); 
	}
	mnstr_printf(cntxt->fdout,"\n");
}

void
MOSadvance_prefix(Client cntxt, MOStask task)
{
	int bits, bytes;
	int size;
	(void) cntxt;

	size = ATOMsize(task->type);
	if( ATOMstorage(task->type == TYPE_str))
			size =task->b->T->width;
	task->start += MOSgetCnt(task->blk);
	switch(size){
	case 1:
		{ bte *dst = (bte*)  (((char*) task->blk) + MosaicBlkSize);
		  bte mask = *dst++;
		  bte val = *dst++;
			bits = val & (~mask);
			// be aware that we use longs as bit vectors
			bytes = sizeof(unsigned long) * ((MOSgetCnt(task->blk) * bits)/64 + ((MOSgetCnt(task->blk) %64) != 0));
			task->blk = (MosaicBlk) (((char*) dst)  + wordaligned(bytes,sht)); 
			//mnstr_printf(cntxt->fdout,"advance mask width %d bytes %d %d \n",bits,bytes,(int)wordaligned(bytes,int));
		}
		break;
	case 2:
		{ sht *dst = (sht*)  (((char*) task->blk) + MosaicBlkSize);
		  sht mask = *dst++;
		  sht val = *dst++;
			bits = val & (~mask);
			bytes = sizeof(unsigned long) * ((MOSgetCnt(task->blk) * bits)/64 + ((MOSgetCnt(task->blk) %64) != 0));
			task->blk = (MosaicBlk) (((char*) dst)  + wordaligned(bytes,sht)); 
			//mnstr_printf(cntxt->fdout,"advance mask width %d bytes %d %d \n",bits,bytes,(int)wordaligned(bytes,int));
		}
		break;
	case 4:
		{ int *dst = (int*)  (((char*) task->blk) + MosaicBlkSize);
		  int mask = *dst++;
		  int val = *dst++;
			bits = val & (~mask);
			bytes = sizeof(unsigned long) * ((MOSgetCnt(task->blk) * bits)/64 + ((MOSgetCnt(task->blk) %64) != 0));
			task->blk = (MosaicBlk) (((char*) dst)  + wordaligned(bytes, int)); 
			//mnstr_printf(cntxt->fdout,"advance mask width %d bytes %d %d \n",bits,bytes,(int)wordaligned(bytes,int));
		}
		break;
	case 8:
		{ lng *dst = (lng*)  (((char*) task->blk) + MosaicBlkSize);
		  lng mask = *dst++;
		  lng val = *dst++;
			bits = val & (~mask);
			bytes = sizeof(unsigned long) * ((MOSgetCnt(task->blk) * bits)/64 + ((MOSgetCnt(task->blk) %64) != 0));
			task->blk = (MosaicBlk) (((char*) dst)  + wordaligned(bytes, lng)); 
			//mnstr_printf(cntxt->fdout,"advance mask width %d bytes %d %d \n",bits,bytes,(int)wordaligned(bytes,int));
		}
	}
}

void
MOSskip_prefix(Client cntxt, MOStask task)
{
	MOSadvance_prefix(cntxt, task);
	if ( MOSgetTag(task->blk) == MOSAIC_EOL)
		task->blk = 0; // ENDOFLIST
}

// later turn into logaritmic search
#define Prefix(Bits,Mask,X,Y,N) \
{ int k, m = 1; \
  for(k=0;k<N;k++, X>>=1, Y>>=1){\
	if( X == Y) break;\
	m= (m<<1)|1; \
  }\
  Bits = N-k;\
  Mask = ~(m >>1);\
} 


// calculate the expected reduction 
flt
MOSestimate_prefix(Client cntxt, MOStask task)
{	BUN i = -1;
	flt factor = 1.0;
	int bits, size;
	lng store;
	(void) cntxt;
	(void) bits;

	size = ATOMsize(task->type);
	if( ATOMstorage(task->type == TYPE_str))
			size =task->b->T->width;
	if( task->elm >= 2)
	switch(size){
	case 1:
		{	bte *v = (bte*) task->src, *w= v+1, val= *v,val2= *w, mask;
			// search first non-identical value
			for(i = 0;i < task->elm;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			Prefix(bits, mask, val, val2, 8);
			val = *v & mask;
			for(w=v, i = 0; i < task->elm; w++, i++){
				if ( val != (*w & mask) )
					break;
			}
			if ( i > MOSlimit() ) i = MOSlimit();
			bits = i * (8 -bits);
			store = bits/8 + ((bits % 8) >0);
			store = MosaicBlkSize + 2 * sizeof(int) + wordaligned( store,int);
			factor = ( (flt)i * sizeof(int))/ store;
		}
		break;
	case 2:
		{	sht *v = (sht*) task->src, *w= v+1, val= *v,val2= *w, mask;
			// search first non-identical value
			for(i = 0;i < task->elm;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			Prefix(bits, mask, val, val2, 16);
			val = *v & mask;
			for(w=v,i = 0; i < task->elm; w++, i++){
				if ( val != (*w & mask) )
					break;
			}
			if ( i > MOSlimit() ) i = MOSlimit();
			bits = i * (16 -bits);
			store = bits/8 + ((bits % 8) >0);
			store = MosaicBlkSize + 2 * sizeof(int) + wordaligned( store,int);
			factor = ( (flt)i * sizeof(int))/ store;
		}
		break;
	case 4:
		{	int *v = (int*) task->src, *w= v+1, val= *v,val2= *w, mask;
			// search first non-identical value
			for(i = 0;i < task->elm;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			Prefix(bits, mask, val, val2, 32);
			val = *v & mask;
			for(w=v,i = 0; i < task->elm; w++, i++){
				if ( val != (*w & mask) )
					break;
			}
				if ( i > MOSlimit() ) i = MOSlimit();
				bits = i * (32 -bits);
				store = bits/8 + ((bits % 8) >0);
				store = MosaicBlkSize + 2 * sizeof(int) + wordaligned( store,int);
				factor = ( (flt)i * sizeof(int))/ store;
			}
		break;
	case 8:
		{	lng *v = (lng*) task->src, *w= v+1, val= *v,val2= *w, mask;
			// search first non-identical value
			for(i = 0;i < task->elm;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			Prefix(bits, mask, val, val2, 64);
			val = *v & mask;
			for(w=v, i = 0; i < task->elm; w++, i++){
				if ( val != (*w & mask) )
					break;
			}
			if ( i > MOSlimit() ) i = MOSlimit();
			bits = i * (64 -bits);
			store = bits/8 + ((bits % 8) >0);
			store = MosaicBlkSize + 2 * sizeof(int) + wordaligned( store,int);
			factor = ( (flt)i * sizeof(int))/ store;
		}
	}
#ifdef _DEBUG_MOSAIC_
	mnstr_printf(cntxt->fdout,"#estimate rle "BUNFMT" elm %4.3f factor\n",i,factor);
#endif
	return factor;
}

#define compress(Vector,I, Bits, Value) \
{ int cell, lshift, rshift;\
	cell = (I * Bits)/64;\
	lshift= 64 -((I * Bits) % 64) ;\
	if ( lshift > Bits){\
		Vector[cell]= Vector[cell] | (((unsigned long)Value) << (lshift-Bits));\
	}else{ \
		rshift= 64 -  ((I+1) * Bits) % 64;\
		Vector[cell]= Vector[cell] | (((unsigned long)Value) >> (Bits-lshift));\
		Vector[cell+1]= 0 | (((unsigned long)Value)  << rshift);\
	}\
}

void
MOScompress_prefix(Client cntxt, MOStask task)
{
	BUN i ;
	int size;
	MosaicBlk blk = task->blk;

	(void) cntxt;
	MOSsetTag(blk, MOSAIC_PREFIX);

	size = ATOMsize(task->type);
	if( ATOMstorage(task->type == TYPE_str))
			size =task->b->T->width;
	if( task->elm >=2 )
	switch(size){
	case 1:
		{	bte *v = (bte*) task->src, *w= v+1, val = *v, val2 = *w, mask,m;
			bte *dst = (bte*)  (((char*) blk) + MosaicBlkSize);
			BUN limit = task->elm > MOSlimit()? MOSlimit(): task->elm;
			unsigned long *base;
			int bits, rbits; 
			// search first non-identical value
			for(i = 0;i < limit;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			w = v+1;
			Prefix(bits, mask, val, val2, 8);
			rbits = 8-bits;
			*dst++ = mask;
			*dst = *v & mask;
			*dst = *dst | rbits; // bits outside mask
			dst++;
			base  = (unsigned long*) dst; // start of bit vector
			*base = 0;
			
			val = *v & mask;	//reference value
			if( i < limit)
			for(w = v, i = 0; i < limit; w++, i++){
				if ( val  != (*w & mask) )
					break;
				m = (unsigned long)( *w & (~mask)); // residu
				compress(base,i,rbits,m);
			}
			MOSincCnt(blk,i);
			task->src += i;
			task->dst += (i * rbits)/8 + (i %8) != 0;
		}
		break;
	case 2:
		{	sht *v = (sht*) task->src, *w= v+1, val = *v, val2 = *w, mask,m;
			sht *dst = (sht*)  (((char*) blk) + MosaicBlkSize);
			BUN limit = task->elm > MOSlimit()? MOSlimit(): task->elm;
			unsigned long *base;
			int bits, rbits;

			// search first non-identical value
			for(i = 0;i < limit;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			w = v+1;
			Prefix(bits, mask, val, val2, 16);
			rbits = 16-bits;
			*dst++ = mask;
			*dst = *v & mask;
			*dst = *dst | rbits; // bits outside mask
			dst++;
			base  = (unsigned long*) dst; // start of bit vector
			*base = 0;
			
			val = *v & mask;	//reference value
			if( i < limit)
			for(w = v, i = 0; i < limit; w++, i++){
				if ( val  != (*w & mask) )
					break;
				m = *w & (~mask); // residu
				compress(base,i,rbits,m);
			}
			MOSincCnt(blk,i);
			task->src += i * 2;
			task->dst += (i * rbits)/8 + (i %8) != 0;
		}
		break;
	case 4:
		{	unsigned int *v = (unsigned int*) task->src, *w= v+1, val = *v, val2 = *w, mask,m;
			unsigned int *dst = (unsigned int*)  (((char*) blk) + MosaicBlkSize);
			BUN limit = task->elm > MOSlimit()? MOSlimit(): task->elm;
			unsigned long *base;
			int bits, rbits,lshift,rshift, cell;

			// search first non-identical value
			for(i = 0;i < limit;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			w = v+1;
			Prefix(bits, mask, val, val2, 32);
			rbits = 32-bits;
			*dst++ = mask;
			*dst = *v & mask;
			*dst = *dst | rbits; // bits outside mask
			dst++;
			base  = (unsigned long*) dst; // start of bit vector
			*base = 0;
			
			val = *v & mask;	//reference value
			//mnstr_printf(cntxt->fdout,"compress %o %o val %d bits %d, %d mask %o\n",*v,*w,val,bits, rbits,mask);
			if( i < limit)
			for(w = v, i = 0; i < limit; w++, i++){
				if ( val  != (*w & mask) )
					break;
				m = *w & (~mask); // residu
				//mnstr_printf(cntxt->fdout,"compress %d residu %d %o\n",*w,m,m);
				cell = (i * rbits)/64;
				lshift= 64 -((i * rbits) % 64) ;
				if ( lshift > rbits){
					base[cell]= base[cell] | (((long)m) << (lshift-rbits));
					//mnstr_printf(cntxt->fdout,"[%d] shift %d rbits %d cell %o\n",cell, lshift, rbits, base[cell]);
				}else{ 
					rshift= 64 -  ((i+1) * rbits) % 64;
					base[cell]= base[cell] | (((long)m) >> (rbits-lshift));
					base[cell+1]= 0 | (((long)m)  << rshift);
					//mnstr_printf(cntxt->fdout,"[%d] shift %d %d cell %o %o val %o %o\n", cell, lshift, rshift,
						//base[cell],base[cell+1], (m >> (rbits-lshift)),  (m <<rshift));
				}
			}
			MOSincCnt(blk,i);
			task->src += i * 4;
			task->dst += (i * rbits)/8 + (i %8) != 0;
		}
		break;
	case 8:
		{	lng *v = (lng*) task->src, *w= v+1, val = *v, val2 = *w, mask, m;
			lng *dst = (lng*)  (((char*) blk) + MosaicBlkSize);
			BUN limit = task->elm > MOSlimit()? MOSlimit(): task->elm;
			unsigned long *base;
			int bits, rbits; 

			// search first non-identical value
			for(i = 0;i < limit;i++, w++)
			if( *v != *w ){
				val2 = *w;
				break;
			}
			w = v+1;
			Prefix(bits, mask, val, val2, 64);
			rbits = 64-bits;
			*dst++ = mask;
			*dst = *v & mask;
			*dst = *dst | rbits; // bits outside mask
			dst++;
			base  = (unsigned long*) dst; // start of bit vector
			*base = 0;
			
			val = *v & mask;	//reference value
			//mnstr_printf(cntxt->fdout,"compress %o %o val %d bits %d, %d mask %o\n",*v,*w,val,bits, rbits,mask);
			if( i < limit)
			for(w = v, i = 0; i < limit; w++, i++){
				if ( val  != (*w & mask) )
					break;
				m = *w & (~mask); // residu
				compress(base,i,rbits,m);
			}
			MOSincCnt(blk,i);
			task->src += i * 8 ;
			task->dst += (i * rbits)/8 + (i %8) != 0;
		}
	}
#ifdef _DEBUG_MOSAIC_
	MOSdump_prefix(cntxt, task);
#endif
}

#define decompress(Vector,I)\
{	int cell = (I * rbits)/64, lshift,rshift;\
	lshift= 64 -((I * rbits) % 64) ;\
	if ( lshift > (int)rbits){\
		m1 = (Vector[cell]>> (lshift-rbits)) & ((unsigned long) m);\
		v = val  | m1;\
	  }else{ \
		rshift= 64 -  ((I+1) * rbits) % 64;\
		m1 =(Vector[cell] & (((unsigned long)m) >> (rbits-lshift)));\
		m2 = Vector[cell+1] >>rshift;\
		v= val | (m1 <<(rbits-lshift)) | m2;\
	  }\
}

void
MOSdecompress_prefix(Client cntxt, MOStask task)
{
	MosaicBlk blk =  ((MosaicBlk) task->blk);
	BUN i;
	int size;
	(void) cntxt;

	size = ATOMsize(task->type);
	if( ATOMstorage(task->type == TYPE_str))
			size =task->b->T->width;
	switch(size){
	case 1:
		{	bte *dst =  (bte*)  (((char*) blk) + MosaicBlkSize);
			bte mask = *dst++, val  =  *dst++, v;
			bte m,m1,m2;
			BUN lim= MOSgetCnt(blk);
			unsigned long *base;
			int rbits;

			m = ~mask;
			rbits = val & m;
			val = val & mask;
			base = (unsigned long*) dst;
			//mnstr_printf(cntxt->fdout,"decompress rbits %d mask %o val %d\n",rbits,m,val);
			for(i = 0; i < lim; i++){
				decompress(base,i);
				((bte*)task->src)[i] = v;
			}
			task->src += i;
		}
		break;
	case 2:
		{	sht *dst =  (sht*)  (((char*) blk) + MosaicBlkSize);
			sht mask = *dst++, val  =  *dst++, v;
			sht m,m1,m2;
			BUN lim= MOSgetCnt(blk);
			unsigned long *base;
			int rbits;

			m = ~mask;
			rbits = val & m;
			val = val & mask;
			base = (unsigned long*) dst;
			//mnstr_printf(cntxt->fdout,"decompress rbits %d mask %o val %d\n",rbits,m,val);
			for(i = 0; i < lim; i++){
				decompress(base,i);
				((sht*)task->src)[i] = v;
			}
			task->src += i * 2;
		}
		break;
	case 4:
		{	unsigned int *dst =  (unsigned int*)  (((char*) blk) + MosaicBlkSize);
			unsigned int mask = *dst++, val  =  *dst++, v;
			unsigned int m1,m2;
			BUN lim= MOSgetCnt(blk);
			unsigned long *base,m;
			int rbits, cell, lshift, rshift;

			m = ~mask;
			rbits = val & m;
			val = val & mask;
			base = (unsigned long*) dst;
			//mnstr_printf(cntxt->fdout,"decompress rbits %d mask %o val %d\n",rbits,m,val);
			for(i = 0; i < lim; i++){
				cell = (i * rbits)/64;
				lshift= 64 -((i * rbits) % 64) ;
				if ( lshift >rbits){
					m1 = (base[cell]>> (lshift-rbits)) & ((long)m);
					//mnstr_printf(cntxt->fdout,"[%d]cell %o lshift %d m %d\n", cell,  base[cell],lshift,m1);
					v = val  | m1;
				  }else{ 
					rshift= 64 -  ((i+1) * rbits) % 64;
					m1 =(base[cell] & ( ((long)m) >> (rbits-lshift)));
					m2 = base[cell+1] >>rshift;
					v= val | (m1 <<(rbits-lshift)) | m2;
					//mnstr_printf(cntxt->fdout,"[%d] shift %d %d cell %o %o val %o %o\n", cell, lshift, rshift,base[cell],base[cell+1], m1,  m2);
				  }
				((int*)task->src)[i] = v;
			}
			task->src += i * 4;
		}
		break;
	case 8:
		{	lng *dst =  (lng*)  (((char*) blk) + MosaicBlkSize);
			lng mask = *dst++, val  =  *dst++, v;
			lng m,m1,m2;
			BUN lim= MOSgetCnt(blk);
			unsigned long *base;
			int rbits;

			m = ~mask;
			rbits = val & m;
			val = val & mask;
			base = (unsigned long*) dst;
			//mnstr_printf(cntxt->fdout,"decompress rbits %d mask %o val %d\n",rbits,m,val);
			for(i = 0; i < lim; i++){
				decompress(base,i);
				((lng*)task->src)[i] = v;
			}
			task->src += i * 8;
		}
	}
}

// perform relational algebra operators over non-compressed chunks
// They are bound by an oid range and possibly a candidate list

#define  subselect_prefix(TPE, TPE2, BITS) \
{	TPE2 *dst =  (TPE2*)  (((char*) blk) + MosaicBlkSize);\
	TPE2 mask = *dst++;\
	TPE2  val  =  (TPE) *dst++,v;\
	TPE2 m,m1,m2;\
	TPE2 *base;\
	TPE2 rbits;\
	TPE value;\
	m = ~mask;\
	rbits = val & m;\
	val = val & mask;\
	base = (TPE2*) dst;\
	if( !*anti){\
		if( *(TPE*) low == TPE##_nil && *(TPE*) hgh == TPE##_nil){\
			for( ; first < last; first++){\
				MOSskipit();\
				*o++ = (oid) first;\
			}\
		} else\
		if( *(TPE*) low == TPE##_nil ){\
			for( ; first < last; first++,i++){\
				MOSskipit();\
				decompress(base,i);\
				value = (TPE) v;\
				cmp  =  ((*hi && value <= * (TPE*)hgh ) || (!*hi && value < *(TPE*)hgh ));\
				if (cmp )\
					*o++ = (oid) first;\
			}\
		} else\
		if( *(TPE*) hgh == TPE##_nil ){\
			for( ; first < last; first++,i++){\
				MOSskipit();\
				decompress(base,i);\
				value = (TPE) v;\
				cmp  =  ((*li && value >= * (TPE*)low ) || (!*li && value > *(TPE*)low ));\
				if (cmp )\
					*o++ = (oid) first;\
			}\
		} else{\
			for( ; first < last; first++,i++){\
				MOSskipit();\
				decompress(base,i);\
				value = (TPE) v;\
				cmp  =  ((*hi && value <= * (TPE*)hgh ) || (!*hi && value < *(TPE*)hgh )) &&\
						((*li && value >= * (TPE*)low ) || (!*li && value > *(TPE*)low ));\
				if (cmp )\
					*o++ = (oid) first;\
			}\
		}\
	} else {\
		if( *(TPE*) low == TPE##_nil && *(TPE*) hgh == TPE##_nil){\
			/* nothing is matching */\
		} else\
		if( *(TPE*) low == TPE##_nil ){\
			for( ; first < last; first++,i++){\
				MOSskipit();\
				decompress(base,i);\
				value = (TPE) v;\
				cmp  =  ((*hi && value <= * (TPE*)hgh ) || (!*hi && value < *(TPE*)hgh ));\
				if ( !cmp )\
					*o++ = (oid) first;\
			}\
		} else\
		if( *(TPE*) hgh == TPE##_nil ){\
			for( ; first < last; first++, val++,i++){\
				MOSskipit();\
				decompress(base,i);\
				value = (TPE) v;\
				cmp  =  ((*li && value >= * (TPE*)low ) || (!*li && value > *(TPE*)low ));\
				if ( !cmp )\
					*o++ = (oid) first;\
			}\
		} else{\
			for( ; first < last; first++, val++,i++){\
				MOSskipit();\
				decompress(base,i);\
				value = (TPE) v;\
				cmp  =  ((*hi && value <= * (TPE*)hgh ) || (!*hi && value < *(TPE*)hgh )) &&\
						((*li && value >= * (TPE*)low ) || (!*li && value > *(TPE*)low ));\
				if (!cmp)\
					*o++ = (oid) first;\
			}\
		}\
	}\
}

str
MOSsubselect_prefix(Client cntxt,  MOStask task, void *low, void *hgh, bit *li, bit *hi, bit *anti){
	oid *o;
	int cmp;
	BUN i = 0,first,last;
	MosaicBlk blk= task->blk;

	// set the oid range covered
	first = task->start;
	last = first + MOSgetCnt(task->blk);

	(void) cntxt;
	if (task->cl && *task->cl > last){
		MOSadvance_prefix(cntxt,task);
		return MAL_SUCCEED;
	}
	o = task->lb;

	switch(ATOMstorage(task->type)){
	case TYPE_bit: subselect_prefix(bit,unsigned int, 8); break;
	case TYPE_bte: subselect_prefix(bte,unsigned int, 8); break;
	case TYPE_sht: subselect_prefix(sht,unsigned int, 16); break;
	case TYPE_lng: subselect_prefix(lng,unsigned long, 64); break;
	case TYPE_oid: subselect_prefix(oid,unsigned long, 64); break;
	case TYPE_wrd: subselect_prefix(wrd,unsigned long, 64); break;
	case TYPE_flt: subselect_prefix(flt,unsigned int,  32); break;
	case TYPE_dbl: subselect_prefix(dbl,unsigned long, 64); break;
#ifdef HAVE_HGE
	case TYPE_hge: subselect_prefix(hge,unsigned long long,128); break;
#endif
	case TYPE_int:
	// Expanded MOSselect_prefix for debugging
	{	unsigned int *dst =  (unsigned int*)  (((char*) blk) + MosaicBlkSize);
		unsigned int mask = *dst++;
		int  val  =  (int) *dst++,v;
		unsigned int m,m1,m2;
		unsigned long *base;
		int rbits;

		m = ~mask;
		rbits = val & m;
		val = val & mask;
		base = (unsigned long*) dst;
		if( !*anti){
			if( *(int*) low == int_nil && *(int*) hgh == int_nil){
				for( ; first < last; first++){
					MOSskipit();
					*o++ = (oid) first;
				}
			} else
			if( *(int*) low == int_nil ){
				for( ; first < last; i++, first++){
					MOSskipit();
					decompress(base,i);
					cmp  =  ((*hi && v <= * (int*)hgh ) || (!*hi && v < *(int*)hgh ));
					if (cmp )
						*o++ = (oid) first;
				}
			} else
			if( *(int*) hgh == int_nil ){
				for( ; first < last; first++,i++){
					MOSskipit();
					decompress(base,i);
					cmp  =  ((*li && v >= * (int*)low ) || (!*li && v > *(int*)low ));
					if (cmp )
						*o++ = (oid) first;
				}
			} else{
				for( ; first < last; first++,i++){
					MOSskipit();
					decompress(base,i);
					cmp  =  ((*hi && v <= * (int*)hgh ) || (!*hi && v < *(int*)hgh )) &&
							((*li && v >= * (int*)low ) || (!*li && v > *(int*)low ));
					if (cmp )
					*o++ = (oid) first;
				}
			}
		} else {
			if( *(int*) low == int_nil && *(int*) hgh == int_nil){
				/* nothing is matching */
			} else
			if( *(int*) low == int_nil ){
				for( ; first < last; first++, i++){
					MOSskipit();
					decompress(base,i);
					cmp  =  ((*hi && v <= * (int*)hgh ) || (!*hi && v < *(int*)hgh ));
					if ( !cmp )
						*o++ = (oid) first;
				}
			} else
			if( *(int*) hgh == int_nil ){
				for( ; first < last; first++,i++){
					MOSskipit();
					decompress(base,i);
					cmp  =  ((*li && v >= * (int*)low ) || (!*li && v > *(int*)low ));
					if ( !cmp )
						*o++ = (oid) first;
				}
			} else{
				for( ; first < last; first++,i++){
					MOSskipit();
					decompress(base,i);
					cmp  =  ((*hi && v <= * (int*)hgh ) || (!*hi && v < *(int*)hgh )) &&
							((*li && v >= * (int*)low ) || (!*li && v > *(int*)low ));
					if (!cmp)
					*o++ = (oid) first;
				}
			}
		}
	}
/*
	break;
	case  TYPE_str:
		switch(task->b->T->width){
		case 1: break;
		case 2: break;
		case 4: break;
		case 8: break;
		}
		break;
*/
	default:
		if( task->type == TYPE_date)
			subselect_prefix(date,int,32); 
		if( task->type == TYPE_daytime)
			subselect_prefix(daytime,int,32); 
		if( task->type == TYPE_timestamp)
			subselect_prefix(lng,long,64); 
	}
	MOSadvance_prefix(cntxt,task);
	task->lb = o;
	return MAL_SUCCEED;
}

#define thetasubselect_prefix(TPE, TPE2, BITS)\
{ 	TPE low,hgh;\
    TPE2 *dst =  (TPE2*)  (((char*) blk) + MosaicBlkSize);\
    TPE2 mask = *dst++;\
    TPE2  val  =  (TPE) *dst++,v;\
    TPE2 m,m1,m2;\
    unsigned long *base;\
    TPE2 rbits;\
    TPE value;\
	m = ~mask;\
	rbits = val & m;\
	val = val & mask;\
	base = (unsigned long*) dst;\
	low= hgh = TPE##_nil;\
	if ( strcmp(oper,"<") == 0){\
		hgh= *(TPE*) input;\
		hgh = PREVVALUE##TPE(hgh);\
	} else\
	if ( strcmp(oper,"<=") == 0){\
		hgh= *(TPE*) input;\
	} else\
	if ( strcmp(oper,">") == 0){\
		low = *(TPE*) input;\
		low = NEXTVALUE##TPE(low);\
	} else\
	if ( strcmp(oper,">=") == 0){\
		low = *(TPE*) input;\
	} else\
	if ( strcmp(oper,"!=") == 0){\
		low = hgh = *(TPE*) input;\
		anti++;\
	} else\
	if ( strcmp(oper,"==") == 0){\
		hgh= low= *(TPE*) input;\
	} \
	if ( !anti)\
		for( ; first < last; first++,i++){\
			MOSskipit();\
			decompress(base,i);\
			value = (TPE) v;\
			if( (low == TPE##_nil || value >= low) && (value <= hgh || hgh == TPE##_nil) )\
			*o++ = (oid) first;\
		}\
	else\
		for( ; first < last; first++,i++){\
			MOSskipit();\
			decompress(base,i);\
			value = (TPE) v;\
			if( !( (low == TPE##_nil || value >= low) && (value <= hgh || hgh == TPE##_nil) ))\
				*o++ = (oid) first;\
		}\
}

str
MOSthetasubselect_prefix(Client cntxt,  MOStask task, void *input, str oper)
{
	oid *o;
	int anti=0;
	BUN i=0,first,last;
	MosaicBlk blk = task->blk;
	(void) cntxt;
	
	// set the oid range covered and advance scan range
	first = task->start;
	last = first + MOSgetCnt(task->blk);

	if (task->cl && *task->cl > last){
		MOSskip_prefix(cntxt,task);
		return MAL_SUCCEED;
	}
	o = task->lb;

	switch(ATOMstorage(task->type)){
	case TYPE_bit: thetasubselect_prefix(bit, unsigned int, 8); break;
	case TYPE_bte: thetasubselect_prefix(bte, unsigned int, 8); break;
	case TYPE_sht: thetasubselect_prefix(sht, unsigned int, 16); break;
	case TYPE_lng: thetasubselect_prefix(lng, unsigned long, 16); break;
	case TYPE_oid: thetasubselect_prefix(oid, unsigned long, 64); break;
	case TYPE_wrd: thetasubselect_prefix(wrd, unsigned long, 64); break;
	case TYPE_flt: thetasubselect_prefix(flt, unsigned int, 32); break;
	case TYPE_dbl: thetasubselect_prefix(dbl, unsigned long, 64); break;
#ifdef HAVE_HGE
	case TYPE_hge: thetasubselect_prefix(hge, unsigned long long,128); break;
#endif
	case TYPE_int:
		{ 	int low,hgh;
			unsigned int *dst =  (unsigned int*)  (((char*) blk) + MosaicBlkSize);
			unsigned int mask = *dst++;
			unsigned int  val  =  (unsigned int) *dst++,v;
			unsigned int m,m1,m2;
			unsigned long *base;
			int rbits;
			int value;

			m = ~mask;
			rbits = val & m;
			val = val & mask;
			base = (unsigned long*) dst;
			low= hgh = int_nil;
			if ( strcmp(oper,"<") == 0){
				hgh= *(int*) input;
				hgh = PREVVALUEint(hgh);
			} else
			if ( strcmp(oper,"<=") == 0){
				hgh= *(int*) input;
			} else
			if ( strcmp(oper,">") == 0){
				low = *(int*) input;
				low = NEXTVALUEint(low);
			} else
			if ( strcmp(oper,">=") == 0){
				low = *(int*) input;
			} else
			if ( strcmp(oper,"!=") == 0){
				low = hgh = *(int*) input;
				anti++;
			} else
			if ( strcmp(oper,"==") == 0){
				hgh= low= *(int*) input;
			} 
			if ( !anti)
				for( ; first < last; first++,i++){
					MOSskipit();
					decompress(base,i);
					value = (int) v;
					if( (low == int_nil || value >= low) && (value <= hgh || hgh == int_nil) ){
						*o++ = (oid) first;
					}
				}
			else
				for( ; first < last; first++,i++){
					MOSskipit();
					decompress(base,i);
					value = (int) v;
					if( !((low == int_nil || value >= low) && (value <= hgh || hgh == int_nil) )){
							*o++ = (oid) first;
					}
				}
		}
		break;
	case  TYPE_str:
		// we only have to look at the index width, not the values
		switch(task->b->T->width){
		case 1: break;
		case 2: break;
		case 4: break;
		case 8: break;
		}
		break;
	}
	MOSskip_prefix(cntxt,task);
	task->lb =o;
	return MAL_SUCCEED;
}

#define leftfetchjoin_prefix(TPE, TPE2)\
{	TPE *r;\
    TPE2 *dst =  (TPE2*)  (((char*) blk) + MosaicBlkSize);\
    TPE2 mask = *dst++;\
    TPE2  val  =  (TPE) *dst++,v;\
    TPE2 m,m1,m2;\
    unsigned long *base;\
    int rbits;\
    TPE value;\
	m = ~mask;\
	rbits = val & m;\
	val = val & mask;\
	base = (unsigned long*) dst;\
	r= (TPE*) task->src;\
	for(; first < last; first++,i++){\
		MOSskipit();\
		decompress(base,i);\
		value = (TPE) v;\
		*r++ = value;\
		task->n--;\
	}\
	task->src = (char*) r;\
}

str
MOSleftfetchjoin_prefix(Client cntxt,  MOStask task)
{
	BUN i=0, first,last;
	MosaicBlk blk= task->blk;
	(void) cntxt;

	// set the oid range covered and advance scan range
	first = task->start;
	last = first + MOSgetCnt(task->blk);

	switch(ATOMstorage(task->type)){
		case TYPE_bit: leftfetchjoin_prefix(bit, unsigned char); break;
		case TYPE_bte: leftfetchjoin_prefix(bte, unsigned char); break;
		case TYPE_sht: leftfetchjoin_prefix(sht, unsigned short); break;
		case TYPE_lng: leftfetchjoin_prefix(lng, unsigned long); break;
		case TYPE_oid: leftfetchjoin_prefix(oid, unsigned long); break;
		case TYPE_wrd: leftfetchjoin_prefix(wrd, unsigned long); break;
		case TYPE_flt: leftfetchjoin_prefix(flt, unsigned int); break;
		case TYPE_dbl: leftfetchjoin_prefix(dbl, unsigned long); break;
#ifdef HAVE_HGE
		case TYPE_hge: leftfetchjoin_prefix(hge, unsigned long long); break;
#endif
		case TYPE_int:
		{	int *r;
			unsigned int *dst =  (unsigned int*)  (((char*) blk) + MosaicBlkSize);
			unsigned int mask = *dst++;
			unsigned int  val  =  (unsigned int) *dst++,v;
			unsigned int m,m1,m2;
			unsigned long *base;
			int rbits;
			int value;
			m = ~mask;
			rbits = val & m;
			val = val & mask;
			base = (unsigned long*) dst;
			r= (int*) task->src;
			for(; first < last; first++,i++){
				MOSskipit();
				decompress(base,i);
				value = (int) v;
				*r++ = value;
				task->n--;
			}
			task->src = (char*) r;
		}
		break;
	case  TYPE_str:
		// we only have to look at the index width, not the values
		switch(task->b->T->width){
		case 1: leftfetchjoin_prefix(bte, unsigned char); break;
		case 2: leftfetchjoin_prefix(sht, unsigned short); break;
		case 4: leftfetchjoin_prefix(int, unsigned int); break;
		case 8: leftfetchjoin_prefix(lng, unsigned long); break;
		}
		break;
	}
	MOSskip_prefix(cntxt,task);
	return MAL_SUCCEED;
}

#define join_prefix(TPE,TPE2)\
{   TPE *w;\
	TPE2 *dst =  (TPE2*)  (((char*) blk) + MosaicBlkSize);\
	TPE2 mask = *dst++;\
	TPE2  val  =  (TPE2) *dst++,v;\
	TPE2 m,m1,m2;\
	unsigned long *base;\
	int rbits;\
	TPE value;\
	m = ~mask;\
	rbits = val & m;\
	val = val & mask;\
	base = (unsigned long*) dst;\
	w = (TPE*) task->src;\
	for(n = task->elm, o = 0; n -- > 0; w++,o++){\
		for(i=0, oo= (oid) first; oo < (oid) last; v++, oo++,i++){\
			decompress(base,i);\
			value = (TPE) v;\
			if ( *w == value){\
				BUNappend(task->lbat, &oo, FALSE);\
				BUNappend(task->rbat, &o, FALSE);\
			}\
		}\
	}\
}

str
MOSjoin_prefix(Client cntxt,  MOStask task)
{
	BUN i= 0,n,first,last;
	MosaicBlk blk= task->blk;
	oid o, oo;
	(void) cntxt;

	// set the oid range covered and advance scan range
	first = task->start;
	last = first + MOSgetCnt(task->blk);

	switch(ATOMstorage(task->type)){
		case TYPE_bit: join_prefix(bit,unsigned char); break;
		case TYPE_bte: join_prefix(bte,unsigned char); break;
		case TYPE_sht: join_prefix(sht,unsigned short); break;
		case TYPE_lng: join_prefix(lng,unsigned long); break;
		case TYPE_oid: join_prefix(oid,unsigned long); break;
		case TYPE_wrd: join_prefix(wrd,unsigned long); break;
		case TYPE_flt: join_prefix(flt,unsigned int); break;
		case TYPE_dbl: join_prefix(dbl,unsigned long); break;
#ifdef HAVE_HGE
		case TYPE_hge: join_prefix(hge,unsigned long long); break;
#endif
		case TYPE_int:
        {   int *w;
            unsigned int *dst =  (unsigned int*)  (((char*) blk) + MosaicBlkSize);
            unsigned int mask = *dst++;
            unsigned int  val  =  (unsigned int) *dst++,v;
            unsigned int m,m1,m2;
            unsigned long *base;
            int rbits;
            int value;
            m = ~mask;
            rbits = val & m;
            val = val & mask;
            base = (unsigned long*) dst;

			w = (int*) task->src;
			for(n = task->elm, o = 0; n -- > 0; w++,o++){
				for(i=0, oo= (oid) first; oo < (oid) last; v++, oo++,i++){
					decompress(base,i);
					value = (int) v;
					if ( *w == value){
						BUNappend(task->lbat, &oo, FALSE);
						BUNappend(task->rbat, &o, FALSE);
					}
				}
			}
		}
		break;
		case  TYPE_str:
		// we only have to look at the index width, not the values
		switch(task->b->T->width){
		case 1: join_prefix(bte, unsigned char); break;
		case 2: join_prefix(sht, unsigned short); break;
		case 4: join_prefix(int, unsigned int); break;
		case 8: join_prefix(lng, unsigned long); break;
		}
	}
	MOSskip_prefix(cntxt,task);
	return MAL_SUCCEED;
}
