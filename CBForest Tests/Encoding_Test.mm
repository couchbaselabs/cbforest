//
//  Encoding_Test.mm
//  CBForest
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#import "Encoding.hh"
#import "EncodingWriter.hh"
#import <XCTest/XCTest.h>
#import "testutil.h"
#import <math.h>
#import <sstream>
#import <unordered_map>


using namespace forestdb;


@interface Data_Test : XCTestCase
@end

@implementation Data_Test
{
    bool _shareStrings;
}

// Overridden to run each test twice: once with shared strings enabled, once without
- (void)invokeTest {
    NSLog(@"-- without shared strings --");
    _shareStrings = false;
    [super invokeTest];
    NSLog(@"-- with shared strings --");
    _shareStrings = true;
    [super invokeTest];
}


- (void)test01_Numbers {
    alloc_slice s;
    {
        Writer out(32);
        dataWriter writer(out);
        writer.enableSharedStrings(_shareStrings);
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
        writer.writeUInt(UINT64_MAX);
        writer.writeFloat((float)M_PI);
        writer.writeDouble(M_PI);
        writer.writeRawNumber("12345678901234567890123456789012345678901234567890.1234567890e-08");
        s = alloc_slice::adopt(out.extractOutput());
    }
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    const value* v = (const array*)s.buf;
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
    AssertEq(v->asUnsigned(), 123456789);
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
    AssertEq(v->type(), kNumber);
    AssertEq(v->asUnsigned(), UINT64_MAX);

    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 3);
    XCTAssertEqualWithAccuracy(v->asDouble(), 3.14159, 1e-5);

    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq(v->asInt(), 3);
    XCTAssertEqualWithAccuracy(v->asDouble(), 3.14159265359, 1e-10);

    v = v->next();
    AssertEq(v->type(), kNumber);
    AssertEq((std::string)v->asString(),
             "12345678901234567890123456789012345678901234567890.1234567890e-08");
    XCTAssertEqualWithAccuracy(v->asDouble(), 1.2345678901234568E+41, 1);
    v = v->next();
    AssertEq(v, s.end());
}

- (void)test02_Strings {
    alloc_slice s;
    {
        Writer out(32);
        dataWriter writer(out);
        writer.enableSharedStrings(_shareStrings);
        writer << "hey there";
        writer << std::string("hi there");
        writer << slice("ho there");
        writer << "";
        writer << std::string("hi there"); // repeated; will become shared ref
        s = alloc_slice::adopt(out.extractOutput());
    }
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    const value* v = (const value*)s.buf;
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("hey there"));
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("hi there"));
    if (_shareStrings)
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
    if (_shareStrings)
        Assert(v->isSharedString());
    AssertEq(v->stringToken(), token);
    v = v->next();
    AssertEq(v, s.end());

    if (_shareStrings) {
        // Make sure repeated string "hi there" is only stored once:
        std::string str((char*)s.buf, s.size);
        AssertEq(str.find("hi there"), str.rfind("hi there"));
    }
}

- (void)test03_ExternStrings {
    std::unordered_map<std::string, uint32_t> externStrings;
    externStrings["hi there"] = 1;
    externStrings["ho there"] = 2;

    alloc_slice s;
    {
        Writer out(32);
        dataWriter writer(out, &externStrings);
        writer.enableSharedStrings(_shareStrings);
        writer << "hey there";
        writer << std::string("hi there");
        writer << slice("ho there");
        writer << "";
        writer << std::string("hi there");
        s = alloc_slice::adopt(out.extractOutput());
    }
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
    std::string str((char*)s.buf, s.size);
    AssertEq(str.find("hi there"), std::string::npos);
    AssertEq(str.find("ho there"), std::string::npos);
}

- (void)test04_Arrays {
    alloc_slice s;
    {
        Writer out(32);
        dataWriter writer(out);
        writer.enableSharedStrings(_shareStrings);
        writer.beginArray(3);
        writer.writeInt(12);
        writer << "hi there";
        writer.beginArray(2);
        writer.writeInt(665544);
        writer.writeBool(false);
        writer.endArray();
        writer.endArray();
        s = alloc_slice::adopt(out.extractOutput());
    }
    NSLog(@"Encoded = %@", s.uncopiedNSData());
    Assert(value::validate(s));

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

- (void) checkValidation: (slice)s {
    Assert(value::validate(s));
    while (s.size > 0) {
        --s.size;
        Assert(!value::validate(s));
    }
}

- (void)test05_Dicts {
    alloc_slice s;
    {
        Writer out(32);
        dataWriter writer(out);
        writer.enableSharedStrings(_shareStrings);
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
        s = alloc_slice::adopt(out.extractOutput());
    }
    NSLog(@"Encoded = %@", s.uncopiedNSData());
    [self checkValidation: s];

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

    // Random-access lookup:
    const value* v = d1->get(slice("twelve"));
    Assert(v != NULL);
    AssertEq(v->asInt(), 12);
    v = d1->get(slice("nested"));
    Assert(v != NULL);
    AssertEq(v->type(), kDict);
    v = d1->get(slice("bogus"));
    Assert(v == NULL);

    if (_shareStrings) {
        // Make sure repeated key "greeting" is only stored once:
        std::string str((char*)s.buf, s.size);
        AssertEq(str.find("greeting"), str.rfind("greeting"));
    }

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
    Writer out(32);
    dataWriter writer(out);
    writer.enableSharedStrings(_shareStrings);
    writer.write(self.objCFixture);
    alloc_slice s = alloc_slice::adopt(out.extractOutput());
    NSLog(@"Encoded = %@", s.uncopiedNSData());
    [self checkValidation: s];

    const value* outer = (const value*)s.buf;
    AssertEqual(outer->asNSObject(), self.objCFixture);
}

- (void) test07_JSON {
    Writer out(32);
    dataWriter writer(out);
    writer.enableSharedStrings(_shareStrings);
    writer.write(self.objCFixture);
    alloc_slice s = alloc_slice::adopt(out.extractOutput());
    [self checkValidation: s];

    const value* outer = (const value*)s.buf;
    std::stringstream json;
    outer->writeJSON(json, NULL);
    NSString* jsonStr = (NSString*)(slice)json.str();
    NSLog(@"JSON = %@", jsonStr);
    AssertEqual(jsonStr, @"{\"\":[3.1415926,18446744073709551615,100000000000000000,\"two\\nlines\"],\"nested\":{\"big\":665544,\"greeting\":false,\"whatIsThisCrap\":\"F3b8qgA=\"},\"date\":\"2015-01-30T00:55:01Z\",\"twelve\":12,\"greeting\":\"hi there\"}");
}

@end
