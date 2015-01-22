//
//  Delta.hh
//  CBForest
//
//  Created by Jens Alfke on 1/21/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef __CBForest__Delta__
#define __CBForest__Delta__

#include "Delta.hh"
#include "slice.hh"

namespace forestdb {

    /** Creates a delta representing the change from `reference` to `target`. */
    alloc_slice CreateDelta(slice reference, slice target);

    /** Applies a `delta` to `reference`, recreating the `target`. */
    alloc_slice ApplyDelta(slice reference, slice delta);
    
}

#endif /* defined(__CBForest__Delta__) */
