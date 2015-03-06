//
//  Delta.hh
//  CBForest
//
//  Created by Jens Alfke on 1/21/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef __CBForest__Delta__
#define __CBForest__Delta__

#include "slice.hh"

namespace forestdb {

    typedef unsigned DeltaFlags;
    enum : DeltaFlags {
        kNoChecksum = 16,        // Don't write or check 32-bit Adler checksum
    };

    /** Creates a delta representing the change from `reference` to `target`. */
    alloc_slice CreateDelta(slice reference, slice target, DeltaFlags = 0);

    /** Applies a `delta` to `reference`, recreating the `target`. */
    alloc_slice ApplyDelta(slice reference, slice delta, DeltaFlags = 0);

}

#endif /* defined(__CBForest__Delta__) */
