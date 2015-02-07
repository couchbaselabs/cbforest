//
//  EncodingContext.hh
//  CBForest
//
//  Created by Jens Alfke on 2/7/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef __CBForest__EncodingContext__
#define __CBForest__EncodingContext__
#include <unordered_map>
#include <vector>


#ifndef __OBJC__
typedef const struct __CFString *CFStringRef;
#endif


struct stringInfo {
    std::string string;
    CFStringRef cfString;
    uint32_t externRef;
    uint16_t hash;
};


class encodingContext {
public:
    stringInfo* lookup(const std::string&) const;
    stringInfo* lookup(uint32_t) const;
    stringInfo* lookup(CFStringRef);

    stringInfo* add(const stringInfo&);

private:
    std::vector<stringInfo> _byRef;
    std::unordered_map<std::string, uint32_t> _byString;
};

#endif /* defined(__CBForest__EncodingContext__) */
