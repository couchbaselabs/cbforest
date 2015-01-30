//
//  DataWriter.mm
//  CBForest
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "DataWriter.hh"


namespace forestdb {


    void dataWriter::write(id obj) {
        if ([obj isKindOfClass: [NSString class]]) {
            writeString([obj UTF8String]);
        } else if ([obj isKindOfClass: [NSNumber class]]) {
            switch ([obj objCType][0]) {
                case 'c':
                    writeBool([obj boolValue]);
                    break;
                case 'f':
                case 'd':
                    writeDouble([obj doubleValue]);
                    break;
                case 'Q':
                    writeUInt([obj unsignedLongLongValue]);
                    break;
                default:
                    writeInt([obj longLongValue]);
                    break;
            }
        } else if ([obj isKindOfClass: [NSDictionary class]]) {
            beginDict([obj count]);
            for (NSString* key in obj) {
                writeString([key UTF8String]);
                write([obj objectForKey: key]);
            }
            endDict();
        } else if ([obj isKindOfClass: [NSArray class]]) {
            beginArray([obj count]);
            for (NSString* item in obj) {
                write(item);
            }
            endArray();
        } else if ([obj isKindOfClass: [NSData class]]) {
            writeData(slice([obj bytes], [obj length]));
        } else if ([obj isKindOfClass: [NSDate class]]) {
            writeDate((std::time_t)[obj timeIntervalSince1970]);
        } else if ([obj isKindOfClass: [NSNull class]]) {
            writeNull();
        } else {
            NSCAssert(NO, @"Objects of class %@ are not JSON-compatible", [obj class]);
        }
    }

}