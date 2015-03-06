//
//  Delta.cc
//  CBForest
//
//  Created by Jens Alfke on 1/21/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#include "Delta.hh"
#include "Error.hh"

extern "C" {
#include "zdlib.h"
#include "zd_mem.h"
#include "zutil.h"
}

namespace forestdb {

    static int compress(int level, int windowBits,
                        const Bytef *ref, uLong rsize,
                        const Bytef *tar, uLong tsize,
                        Bytef **delta, uLongf *dsize);
    static int uncompress(int windowBits,
                          const Bytef *ref, uLong rsize,
                          Bytef **tar,  uLongf *tsize,
                          const Bytef *delta, uLong dsize);
    

    alloc_slice CreateDelta(slice reference, slice target, DeltaFlags flags) {
        int level = (flags & 0xF);
        if (level == 0)
            level = ZD_DEFAULT_COMPRESSION;
        int windowBits = DEF_WBITS;
        if (flags & kNoChecksum)
            windowBits = -windowBits;
        Bytef* delta = NULL;
        uLongf deltaSize = 0;
        int status = compress(level, windowBits,
                              (const Bytef*)reference.buf, reference.size,
                              (const Bytef*)target.buf, target.size,
                              &delta, &deltaSize);
        if (status != ZD_OK)
            throw error(error::ZDeltaError);
        return alloc_slice::adopt(delta, deltaSize);
    }

    alloc_slice ApplyDelta(slice reference, slice delta, DeltaFlags flags) {
        Bytef* target = NULL;
        uLongf targetSize = 0;
        int windowBits = DEF_WBITS;
        if (flags & kNoChecksum)
            windowBits = -windowBits;
        int status = uncompress(windowBits,
                                (const Bytef*)reference.buf, reference.size,
                                &target, &targetSize,
                                (const Bytef*)delta.buf, delta.size);
        if (status != ZD_OK)
            throw error(error::ZDeltaError);
        return alloc_slice::adopt(target, targetSize);
    }


#define EXPECTED_RATIO 4


    static int compress(int level, int windowBits,
                        const Bytef *ref, uLong rsize,
                        const Bytef *tar, uLong tsize,
                        Bytef **delta, uLongf *dsize)
    {
        int rval;
        zd_stream strm;
        zd_mem_buffer dbuf;

        /* the zstream output buffer should have size greater than zero try to
         * guess buffer size, such that no memory realocation will be needed
         */
        if(!(*dsize)) *dsize = tsize/EXPECTED_RATIO + 64; /* *dsize should not be 0*/

        /* init io buffers */
        strm.base[0]  = (Bytef*) ref;
        strm.base_avail[0] = rsize;
        strm.base_out[0] = 0;
        strm.refnum      = 1;

        strm.next_in  = (Bytef*) tar;
        strm.total_in = 0;
        strm.avail_in = (uInt)tsize;

        /* allocate the output buffer */
        zd_alloc(&dbuf, *dsize);

        strm.next_out  = dbuf.pos;
        strm.total_out = 0;
        strm.avail_out = (uInt)*dsize;

        strm.zalloc = (alloc_func)0;
        strm.zfree = (free_func)0;
        strm.opaque = (voidpf)0;

        /* init huffman coder */
        rval = zd_deflateInit2(&strm, level,
                               ZD_DEFLATED, windowBits, DEF_MEM_LEVEL,
                               ZD_DEFAULT_STRATEGY);
        if (rval != ZD_OK)
        {
            fprintf(stderr, "%s error: %d\n", "deflateInit", rval);
            return rval;
        }

        /* compress the data */
        while((rval = zd_deflate(&strm,ZD_FINISH)) == ZD_OK){

            /* set correctly the mem_buffef internal pointer */
            dbuf.pos = strm.next_out;

            /* allocate more memory */
            zd_realloc(&dbuf,dbuf.size);

            /* restore zstream internal pointer */
            strm.next_out = (uch*)dbuf.pos;
            strm.avail_out = (uInt)( dbuf.size - strm.total_out );
        }
        
        /* set correcty the mem_buffer pointers */
        dbuf.pos = strm.next_out; 
        
        if(rval != ZD_STREAM_END){
            fprintf(stderr, "%s error: %d\n", "deflateInit", rval);
            zd_free(&dbuf);
            zd_deflateEnd(&strm);
            return rval;
        }
        
        *delta = dbuf.buffer;
        *dsize = (uLong) strm.total_out;
        
        /* release memory */
        return zd_deflateEnd(&strm);
    }


    static int uncompress(int windowBits,
                          const Bytef *ref, uLong rsize,
                          Bytef **tar,  uLongf *tsize,
                          const Bytef *delta, uLong dsize)
    {
        int rval;
        int f = ZD_SYNC_FLUSH;
        zd_mem_buffer tbuf;
        zd_stream strm;

        /* zdelta: tsize must not be 0; try to guess a good output buffer size */
        if(!(*tsize)) *tsize = rsize*2+64; /* *tsize should not be 0*/

        /* init io buffers */
        strm.base[0]       = (Bytef*) ref;
        strm.base_avail[0] = rsize;
        strm.refnum        = 1;

        /* allocate initial size delta buffer */
        zd_alloc(&tbuf,*tsize);
        strm.avail_out = (uInt)*tsize;
        strm.next_out  = tbuf.buffer;
        strm.total_out = 0;

        strm.avail_in = (uInt)dsize;
        strm.next_in  = (Bytef*) delta;
        strm.total_in = 0;

        strm.zalloc = (alloc_func)0;
        strm.zfree  = (free_func)0;
        strm.opaque = (voidpf)0;
        rval = zd_inflateInit2(&strm, windowBits);
        if (rval != ZD_OK)
        {
            fprintf(stderr, "%s error: %d\n", "zd_InflateInit", rval);
            return rval;
        }

        while((rval = zd_inflate(&strm,f)) == ZD_OK){
            /* add more output memory */
            /*
             if(strm.avail_out!=0){
             rval = ZD_STREAM_END;
             break;
             }
             */
            /* set correctly the mem_buffer internal pointer */
            tbuf.pos = strm.next_out;

            /* allocate more memory */
            zd_realloc(&tbuf,tbuf.size);

            /* restore zstream internal pointer */
            strm.next_out = tbuf.pos;
            strm.avail_out = (uInt)( tbuf.size - strm.total_out );
        }
        
        /* set correctly the mem_buffer internal pointer */
        tbuf.pos = strm.next_out; 
        
        if(rval != ZD_STREAM_END){
            zd_free(&tbuf);
            if(strm.msg!=NULL) fprintf(stderr,"%s\n",strm.msg);
            zd_inflateEnd(&strm);
            return rval;
        }
        
        *tar   = tbuf.buffer;
        *tsize = strm.total_out;
        
        /* free memory */
        return zd_inflateEnd(&strm);
    }

}