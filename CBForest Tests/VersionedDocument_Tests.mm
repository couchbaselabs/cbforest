//
//  VersionedDocument_Tests.mm
//  CBForest
//
//  Created by Jens Alfke on 5/15/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//

#import <XCTest/XCTest.h>
#import "VersionedDocument.hh"
#import "testutil.h"

using namespace forestdb;

@interface VersionedDocument_Tests : XCTestCase
@end

@implementation VersionedDocument_Tests
{
    Database* db;
}


static std::string kDBPath;


static revidBuffer stringToRev(NSString* str) {
    revidBuffer buf(str);
    return buf;
}

+ (void) initialize {
    if (self == [VersionedDocument_Tests class]) {
        LogLevel = kWarning;
        kDBPath = [NSTemporaryDirectory() stringByAppendingPathComponent: @"forest_temp.fdb"].fileSystemRepresentation;
    }
}

- (void)setUp
{
    ::unlink(kDBPath.c_str());
    [super setUp];
    db = new Database(kDBPath, Database::defaultConfig());
}

- (void)tearDown
{
    delete db;
    [super tearDown];
}


- (void) test01_Empty {
    VersionedDocument v(*db, @"foo");
    AssertEqual((NSString*)v.docID(), @"foo");
    Assert(v.revID() == NULL);
    AssertEq(v.flags(), 0);
    XCTAssert(v.get(stringToRev(@"1-aaaa")) == NULL);
}


- (void) test02_RevTreeInsert {
    RevTree tree;
    const Revision* rev;
    revidBuffer rev1ID(forestdb::slice("1-aaaa"));
    forestdb::slice rev1Data("body of revision");
    int httpStatus;
    rev = tree.insert(rev1ID, rev1Data, false, false,
                      revid(), false, httpStatus);
    Assert(rev);
    AssertEq(httpStatus, 201);
    Assert(rev->revID == rev1ID);
    Assert(rev->inlineBody() == rev1Data);
    AssertEq(rev->parent(), (Revision*)NULL);
    Assert(!rev->isDeleted());

    revidBuffer rev2ID(forestdb::slice("2-bbbb"));
    forestdb::slice rev2Data("second revision");
    auto rev2 = tree.insert(rev2ID, rev2Data, false, false, rev1ID, false, httpStatus);
    Assert(rev2);
    AssertEq(httpStatus, 201);
    Assert(rev2->revID == rev2ID);
    Assert(rev2->inlineBody() == rev2Data);
    Assert(!rev2->isDeleted());

    tree.sort();
    rev = tree.get(rev1ID);
    rev2 = tree.get(rev2ID);
    Assert(rev);
    Assert(rev2);
    AssertEq(rev2->parent(), rev);
    Assert(rev->parent() == NULL);

    AssertEq(tree.currentRevision(), rev2);
    Assert(!tree.hasConflict());

    tree.sort();
    AssertEq(tree[0], rev2);
    AssertEq(tree[1], rev);
    AssertEq(rev->index(), 1u);
    AssertEq(rev2->index(), 0u);

    alloc_slice ext = tree.encode();

    RevTree tree2(ext, 12, 1234);
}

- (void) test03_AddRevision {
    NSString *revID = @"1-fadebead", *body = @"{\"hello\":true}";
    revidBuffer revIDBuf(revID);
    VersionedDocument v(*db, @"foo");
    int httpStatus;
    v.insert(revIDBuf, nsstring_slice(body), false, false, NULL, false, httpStatus);
    AssertEq(httpStatus, 201);

    const Revision* node = v.get(stringToRev(revID));
    Assert(node);
    Assert(!node->isDeleted());
    Assert(node->isLeaf());
    Assert(node->isActive());
    AssertEq(v.size(), 1u);
    AssertEq(v.currentRevisions().size(), 1u);
    AssertEq(v.currentRevisions()[0], v.currentRevision());
}

- (void) test04_MultipleRevisions {
    // Create first revision:
    int httpStatus;
    NSString *docID = @"foo";
    NSString *body1 = @"{\"type\":\"test\",\"hello\":true}";
    revidBuffer rev1ID(@"1-fadebead");
    {
        VersionedDocument v(*db, docID);
        v.insert(rev1ID, nsstring_slice(body1), false, false, NULL, false, httpStatus);
        AssertEq(httpStatus, 201);

        Transaction t(db);
        v.save(t);
    }
    // Create 2nd revision:
    revidBuffer rev2ID(@"2-cafed00d");
    NSString *body2 = @"{\"type\":\"test\",\"hello\":false}";
    {
        VersionedDocument v(*db, docID);
        const Revision* rev1 = v[rev1ID];
        Assert(rev1);
        v.insert(rev2ID, nsstring_slice(body2), false, false, rev1, false, httpStatus);
        AssertEq(httpStatus, 201);

        Transaction t(db);
        v.save(t);
    }
    // Create 3rd revision:
    revidBuffer rev3ID(@"3-feed1337");
    NSString *body3 = @"{\"type\":\"test\",\"hello\":-1,\"sum\":5}";
    {
        VersionedDocument v(*db, docID);
        const Revision* rev2 = v[0];
        Assert(rev2);
        AssertEq(rev2->revID, rev2ID);
        v.insert(rev3ID, nsstring_slice(body3), false, false, rev2, false, httpStatus);
        AssertEq(httpStatus, 201);

        Transaction t(db);
        v.save(t);
    }
    // Read back doc and check both revisions:
    {
        VersionedDocument v(*db, docID);
        const Revision* rev = v[rev1ID];
        Assert(rev);
        AssertEq(rev->revID, rev1ID);
        Assert(!rev->isLeaf());
        Assert(!rev->isDeleted());
        Assert(!rev->hasAttachments());
        Assert(!rev->isNew());
        Assert(rev->isCompressed());
        alloc_slice body = rev->readBody();
        AssertEqual((NSString*)body, body1);

        rev = v[rev2ID];
        Assert(rev);
        Assert(!rev->isLeaf());
        Assert(!rev->isDeleted());
        Assert(!rev->hasAttachments());
        Assert(!rev->isNew());
        Assert(rev->isCompressed());
        body = rev->readBody();
        AssertEqual((NSString*)body, body2);

        rev = v[rev3ID];
        Assert(rev);
        Assert(rev->isLeaf());
        Assert(!rev->isDeleted());
        Assert(!rev->hasAttachments());
        Assert(!rev->isNew());
        Assert(!rev->isCompressed());
        body = rev->readBody();
        AssertEqual((NSString*)body, body3);
    }
}


@end
