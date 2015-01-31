//
//  EncodingWriter.mm
//  CBForest
//
//  Created by Jens Alfke on 1/29/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "EncodingWriter.hh"


namespace forestdb {


    void dataWriter::write(__unsafe_unretained id obj) {
        if ([obj isKindOfClass: [NSString class]]) {
            nsstring_slice slice(obj);
            writeString(slice);
        } else if ([obj isKindOfClass: [NSNumber class]]) {
            switch ([obj objCType][0]) {
                case 'c':
                    // The only way to tell whether an NSNumber with 'char' type is a boolean is to
                    // compare it against the singleton kCFBoolean objects:
                    if (obj == (id)kCFBooleanTrue)
                        writeBool(true);
                    else if (obj == (id)kCFBooleanFalse)
                        writeBool(false);
                    else
                        writeInt([obj charValue]);
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
                nsstring_slice slice(key);
                writeString(slice);
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