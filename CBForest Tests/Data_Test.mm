//
//  Data_Test.mm
//  CBForest
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#import "Data.hh"
#import "DataWriter.hh"
#import <XCTest/XCTest.h>
#import "testutil.h"
#import <sstream>
#import <unordered_map>

using namespace forestdb;


@interface Data_Test : XCTestCase
@end

@implementation Data_Test

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)test01_Numbers {
    std::stringstream out;
    {
        dataWriter writer(out);
        writer.writeNull();
        writer.writeBool(false);
        writer.writeBool(true);
        writer.writeInt(0);
        writer.writeInt(42);
        writer.writeInt(-42);
        writer.writeInt(12345);
        writer.writeInt(-12345);
        writer.writeInt(123456789);
        writer.writeInt(-123456789);
        writer.writeInt(INT64_MAX);
        writer.writeInt(INT64_MIN);
    }
    std::string str = out.str();
    slice s = (slice)str;
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    const value* v = (const value*)s.buf;
    AssertEq(v->type(), kNull);
    v = v->next();
    AssertEq(v->type(), kBoolean);
    AssertEq(v->asBool(), false);
    v = v->next();
    AssertEq(v->type(), kBoolean);
    AssertEq(v->asBool(), true);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 0);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 42);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), -42);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 12345);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), -12345);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 123456789);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), -123456789);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), INT64_MAX);
    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), INT64_MIN);
    v = v->next();
    AssertEq(v, s.end());
}

- (void)test02_Strings {
    std::stringstream out;
    {
        dataWriter writer(out);
        writer << "hey there";
        writer << std::string("hi there");
        writer << slice("ho there");
        writer << "";
        writer << std::string("hi there"); // repeated; will become shared ref
    }
    std::string str = out.str();
    slice s = (slice)str;
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    const value* v = (const value*)s.buf;
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("hey there"));
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("hi there"));
    Assert(v->isSharedString());
    uint64_t token = v->stringToken();
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("ho there"));
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string(""));
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("hi there"));
    Assert(v->isSharedString());
    AssertEq(v->stringToken(), token);
    v = v->next();
    AssertEq(v, s.end());

    // Make sure repeated string "hi there" is only stored once:
    AssertEq(str.find("hi there"), str.rfind("hi there"));
}

- (void)test03_ExternStrings {
    std::unordered_map<std::string, uint32_t> externStrings;
    externStrings["hi there"] = 1;
    externStrings["ho there"] = 2;

    std::stringstream out;
    {
        dataWriter writer(out, &externStrings);
        writer << "hey there";
        writer << std::string("hi there");
        writer << slice("ho there");
        writer << "";
        writer << std::string("hi there");
    }
    std::string str = out.str();
    slice s = (slice)str;
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    const value* v = (const value*)s.buf;
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("hey there"));
    v = v->next();
    AssertEq(v->type(), kString);
    Assert(v->isExternString());
    AssertEq(v->stringToken(), 1u);
    v = v->next();
    AssertEq(v->type(), kString);
    Assert(v->isExternString());
    AssertEq(v->stringToken(), 2u);
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string(""));
    v = v->next();
    AssertEq(v->type(), kString);
    Assert(v->isExternString());
    AssertEq(v->stringToken(), 1u);
    v = v->next();
    AssertEq(v, s.end());

    // Make sure string "hi there" is not stored:
    AssertEq(str.find("hi there"), std::string::npos);
    AssertEq(str.find("ho there"), std::string::npos);
}

- (void)test04_Arrays {
    std::stringstream out;
    {
        dataWriter writer(out);
        writer.beginArray(3);
        writer.writeInt(12);
        writer << "hi there";
        writer.beginArray(2);
        writer.writeInt(665544);
        writer.writeBool(false);
        writer.endArray();
        writer.endArray();
    }
    std::string str = out.str();
    slice s = (slice)str;
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    const value* outer = (const value*)s.buf;
    const value* v = outer;
    AssertEq(v->type(), kArray);
    const array* a1 = v->asArray();
    AssertEq(a1->count(), 3);
    v = a1->first();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 12);
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("hi there"));
    v = v->next();
    AssertEq(v->type(), kArray);
    const array* a2 = v->asArray();
    AssertEq(a2->count(), 2);
    v = a2->first();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 665544);
    v = v->next();
    AssertEq(v->type(), kBoolean);
    AssertEq(v->asBool(), false);
    v = v->next();

    AssertEq(v, s.end());

    AssertEqual(outer->asNSObject(), (@[@12, @"hi there", @[@665544, @NO]]));
}

- (void)test05_Dicts {
    std::stringstream out;
    {
        dataWriter writer(out);
        writer.beginDict(3);
        writer.writeKey("twelve");
        writer.writeInt(12);
        writer.writeKey("greeting");
        writer << "hi there";
        writer.writeKey("nested");
        writer.beginDict(2);
        writer.writeKey("big");
        writer.writeInt(665544);
        writer.writeKey("greeting");    // will be shared
        writer.writeBool(false);
        writer.endDict();
        writer.endDict();
    }
    std::string str = out.str();
    slice s = (slice)str;
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    // Iterate:
    const value* outer = (const value*)s.buf;
    AssertEq(outer->type(), kDict);
    const dict* d1 = outer->asDict();
    AssertEq(d1->count(), 3);

    auto iter1 = dict::iterator(d1);
    Assert(iter1);
    AssertEq(iter1.key()->asString(), std::string("twelve"));
    AssertEq(iter1.value()->asInt(), 12);
    ++iter1;

    Assert(iter1);
    AssertEq(iter1.key()->asString(), std::string("greeting"));
    AssertEq(iter1.value()->asString(), std::string("hi there"));
    ++iter1;

    Assert(iter1);
    AssertEq(iter1.key()->asString(), std::string("nested"));
    AssertEq(iter1.value()->type(), kDict);
    const dict* d2 = iter1.value()->asDict();
    AssertEq(d2->count(), 2);

    auto iter2 = dict::iterator(d2);
    Assert(iter2);
    AssertEq(iter2.key()->asString(), std::string("big"));
    AssertEq(iter2.value()->asInt(), 665544);
    ++iter2;

    AssertEq(iter2.key()->asString(), std::string("greeting"));
    AssertEq(iter2.value()->asInt(), 0);
    ++iter2;

    Assert(!iter2);

    ++iter1;
    Assert(!iter1);
    AssertEq(iter1.key(), s.end());

    // Random-access lookup:
    const value* v = d1->get(slice("twelve"));
    AssertEq(v->asInt(), 12);
    v = d1->get(slice("nested"));
    AssertEq(v->type(), kDict);
    v = d1->get(slice("bogus"));
    Assert(v == NULL);

    // Make sure repeated key "greeting" is only stored once:
    AssertEq(str.find("greeting"), str.rfind("greeting"));

    NSDictionary* nsDict = outer->asNSObject();
    AssertEqual(nsDict, (@{@"twelve": @12,
                           @"greeting": @"hi there",
                           @"nested": @{
                                   @"big": @665544,
                                   @"greeting": @NO}
                           }));
}

- (id) objCFixture {
    uint8_t rawData[] = {0x17, 0x76, 0xFC, 0xAA, 0x00};
    NSData* data = [NSData dataWithBytes: rawData length: sizeof(rawData)];
    NSDate* today = [NSDate dateWithTimeIntervalSinceReferenceDate: 444272101];
    return @{@"twelve": @12,
             @"greeting": @"hi there",
             @"": @[@((double)3.1415926), @(UINT64_MAX), @1e17, @"two\nlines"],
             @"nested": @{
                     @"big": @665544,
                     @"greeting": @NO,
                     @"whatIsThisCrap": data},
             @"date": today
             };
}

- (void) test06_ObjC {
    std::stringstream out;
    dataWriter writer(out);
    writer.write(self.objCFixture);
    std::string str = out.str();
    slice s = (slice)str;
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    const value* outer = (const value*)s.buf;
    AssertEqual(outer->asNSObject(), self.objCFixture);
}

- (void) test07_JSON {
    std::stringstream out;
    dataWriter writer(out);
    writer.write(self.objCFixture);
    std::string str = out.str();
    slice s = (slice)str;

    const value* outer = (const value*)s.buf;
    std::stringstream json;
    outer->writeJSON(json, NULL);
    NSString* jsonStr = (NSString*)(slice)json.str();
    NSLog(@"JSON = %@", jsonStr);
    AssertEqual(jsonStr, @"{\"\":[3.1415926,18446744073709551615,100000000000000000,\"two\\nlines\"],\"nested\":{\"big\":665544,\"greeting\":false,\"whatIsThisCrap\":\"F3b8qgA=\"},\"date\":\"2015-01-30T00:55:01Z\",\"twelve\":12,\"greeting\":\"hi there\"}");
}

@end
