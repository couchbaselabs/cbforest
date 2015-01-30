//
//  Endian.h
//  CBForest
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef CBForest_Endian_h
#define CBForest_Endian_h
extern "C" {
    #include "forestdb_endian.h"
}

namespace forestdb {

    union swappedFloat {
        float asFloat;
        uint32_t asRaw;
    };

    static inline swappedFloat _encfloat(float f) {
        swappedFloat swapped;
        swapped.asFloat = f;
        swapped.asRaw = _enc32(swapped.asRaw);
        return swapped;
    }

    static inline float _decfloat(swappedFloat swapped) {
        swapped.asRaw = _dec32(swapped.asRaw);
        return swapped.asFloat;
    }
    
    union swappedDouble {
        double asDouble;
        uint64_t asRaw;
    };

    static inline swappedDouble _encdouble(double d) {
        swappedDouble swapped;
        swapped.asDouble = d;
        swapped.asRaw = _enc64(swapped.asRaw);
        return swapped;
    }

    static inline double _decdouble(swappedDouble swapped) {
        swapped.asRaw = _dec64(swapped.asRaw);
        return swapped.asDouble;
    }

}

#endif
