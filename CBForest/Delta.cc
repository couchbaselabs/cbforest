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
}

namespace forestdb {

    alloc_slice CreateDelta(slice reference, slice target) {
        Bytef* delta = NULL;
        uLongf deltaSize = 0;
        // http://cis.poly.edu/zdelta/manual.shtml#compress1
        int status = ::zd_compress1((const Bytef*)reference.buf, reference.size,
                                    (const Bytef*)target.buf, target.size,
                                    &delta, &deltaSize);
        if (status != ZD_OK)
            throw error(error::ZDeltaError);
        return alloc_slice::adopt(delta, deltaSize);
    }

    alloc_slice ApplyDelta(slice reference, slice delta) {
        Bytef* target = NULL;
        uLongf targetSize = 0;
        // http://cis.poly.edu/zdelta/manual.shtml#uncompress1
        int status = zd_uncompress1((const Bytef*)reference.buf, reference.size,
                                    &target, &targetSize,
                                    (const Bytef*)delta.buf, delta.size);
        if (status != ZD_OK)
            throw error(error::ZDeltaError);
        return alloc_slice::adopt(target, targetSize);
    }

}