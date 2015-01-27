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

- (void)testNumbers {
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

- (void)testStrings {
    std::stringstream out;
    {
        dataWriter writer(out);
        writer << "hey there";
        writer << std::string("hi there");
        writer << slice("ho there");
        writer << "";
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
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string("ho there"));
    v = v->next();
    AssertEq(v->type(), kString);
    AssertEq(v->asString(), std::string(""));
    v = v->next();
    AssertEq(v, s.end());
}

- (void)testArrays {
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

    const value* v = (const value*)s.buf;
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
}

- (void)testDicts {
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
        writer.writeKey("no");
        writer.writeBool(false);
        writer.endDict();
        writer.endDict();
    }
    std::string str = out.str();
    slice s = (slice)str;
    NSLog(@"Encoded = %@", s.uncopiedNSData());

    // Iterate:
    const value* v = (const value*)s.buf;
    AssertEq(v->type(), kDict);
    const dict* d1 = v->asDict();
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

    AssertEq(iter2.key()->asString(), std::string("no"));
    AssertEq(iter2.value()->asInt(), 0);
    ++iter2;

    Assert(!iter2);

    ++iter1;
    Assert(!iter1);
    AssertEq(iter1.key(), s.end());

    // Random-access lookup:
    v = d1->get(slice("twelve"));
    AssertEq(v->asInt(), 12);
    v = d1->get(slice("nested"));
    AssertEq(v->type(), kDict);
    v = d1->get(slice("bogus"));
    Assert(v == NULL);
}

@end
