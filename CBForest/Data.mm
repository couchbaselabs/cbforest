//
//  Data.mm
//  CBForest
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "Data.hh"


namespace forestdb {


    id value::asNSObject(NSArray* externStrings) const {
        NSMapTable* strings = [[NSMapTable alloc]
               initWithKeyOptions: NSPointerFunctionsIntegerPersonality |
                                   NSPointerFunctionsOpaqueMemory
               valueOptions: NSPointerFunctionsStrongMemory
               capacity: 10];
        return asNSObject(strings, externStrings);
    }


    id value::asNSObject(NSMapTable *sharedStrings, NSArray* externStrings) const {
        switch (_typeCode) {
            case kNullCode:
                return [NSNull null];
            case kFalseCode:
                return (__bridge id)kCFBooleanFalse;
            case kTrueCode:
                return (__bridge id)kCFBooleanTrue;
            case kInt8Code...kInt64Code:
                return @(asInt());
            case kUInt64Code:
                return @((uint64_t)asInt());
            case kFloat32Code:
            case kFloat64Code:
            case kRawNumberCode:
                return @(asDouble());
            case kDateCode:
                return [NSDate dateWithTimeIntervalSince1970: getParam()];
            case kStringCode:
                return (NSString*)asString();
            case kSharedStringCode: {
                NSString* str = (NSString*)asString();
                NSMapInsertIfAbsent(sharedStrings,
                                    (const void*)stringToken(),
                                    CFBridgingRetain(str));
                return str;
            }
            case kSharedStringRefCode:
                return (__bridge NSString*)NSMapGet(sharedStrings, (const void*)stringToken());
            case kExternStringRefCode:
                if (!externStrings)
                    throw "unexpected extern string";
                return externStrings[stringToken() - 1];
            case kDataCode:
                return asString().copiedNSData();
            case kArrayCode: {
                const array* a = asArray();
                size_t count = a->count();
                const value* v = a->first();
                NSMutableArray* result = [NSMutableArray arrayWithCapacity: count];
                while (count-- > 0) {
                    [result addObject: v->asNSObject(sharedStrings, externStrings)];
                    v = v->next();
                }
                return result;
            }
            case kDictCode: {
                const dict* d = asDict();
                size_t count = d->count();
                NSMutableDictionary* result = [NSMutableDictionary dictionaryWithCapacity: count];
                for (dict::iterator iter(d); iter; ++iter) {
                    NSString* key = iter.key()->asNSObject(sharedStrings, externStrings);
                    result[key] = iter.value()->asNSObject(sharedStrings, externStrings);
                }
                return result;
            }
            default:
                throw "illegal typecode";
        }
    }


}
