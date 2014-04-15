//
//  rev_tree.h
//  couchstore
//
//  Created by Jens Alfke on 11/23/12.
//  Copyright (c) 2012 Couchbase, Inc. All rights reserved.
//

#ifndef COUCHSTORE_REV_TREE_H
#define COUCHSTORE_REV_TREE_H
#import "forestdb.h"
#include <stdbool.h>
#include <stdlib.h>


// If defined, previous revision bodies will be stored as file offsets to their obsolete docs
#define REVTREE_USES_FILE_OFFSETS


typedef struct {
    void* buf;
    size_t size;
} sized_buf;


/** RevNode.parentIndex value denoting "no parent". */
#define kRevNodeParentIndexNone UINT16_MAX

/** RevNode.sequence value meaning "no sequence yet" (for unsaved revisions.) */
#define kRevNodeSequenceNone 0


enum {
    kRevTreeErrDocNotFound = 1,
    kRevTreeErrAllocFailed
};


enum {
    kRevNodeIsDeleted = 0x01,    /**< Is this revision a deletion/tombstone? */
    kRevNodeIsLeaf    = 0x02,    /**< Is this revision a leaf (no children?) */
    kRevNodeIsNew     = 0x04     /**< Has this node been inserted since decoding? */
};
typedef uint8_t RevNodeFlags;


/** In-memory representation of a single revision's metadata. */
typedef struct RevNode {
    sized_buf   revID;          /**< Revision ID */
    sized_buf   data;           /**< Revision body (JSON), or empty if not stored in this tree */
#ifdef REVTREE_USES_FILE_OFFSETS
    uint64_t    oldBodyOffset;  /**< File offset of doc containing revision body, or else 0 */
#endif
    fdb_seqnum_t sequence;      /**< DB sequence number that this revision has/had */
    uint16_t    parentIndex;    /**< Index in tree's node[] array of parent revision, if any */
    RevNodeFlags flags;         /**< Leaf/deleted flags */
} RevNode;


/** In-memory representation of a revision tree. Basically just a dynamic array of RevNodes.
    The node at index 0 is always the current default/winning revision. */
typedef struct RevTree RevTree;


/** Allocates a new empty RevTree. */
RevTree* RevTreeNew(unsigned capacity);

/** Frees a RevTree. (Does not free the raw_tree it was decoded from, if any.) */
static inline void RevTreeFree(RevTree *tree) {free(tree);}

/** Converts a serialized RevTree into in-memory form.
 *  The RevTree contains pointers into the serialized data, so the memory pointed to by
 *  raw_tree must remain valid until after the RevTree* is freed.
 *  @param raw_tree  The serialized data to read from.
 *  @param extraCapacity  Number of extra nodes to leave room for as an optimization.
 *  @param sequence  The sequence # of the document the raw tree is read from.
 *  @param bodyOffset  The file offset of the document body containing the serialized tree. */
RevTree* RevTreeDecode(sized_buf raw_tree, unsigned extraCapacity,
                       fdb_seqnum_t sequence, uint64_t bodyOffset);

/** Serializes a RevTree. Caller is responsible for freeing the returned block. */
sized_buf RevTreeEncode(RevTree *tree);


/** Returns the number of nodes in a tree. */
unsigned RevTreeGetCount(RevTree *tree);

/** Returns the current/default/winning node. */
const RevNode* RevTreeGetCurrentNode(RevTree *tree);

/** Gets a node from the tree by its index. Returns NULL if index is out of range. */
const RevNode* RevTreeGetNode(RevTree *tree, unsigned index);

/** Finds a node in a tree given its rev ID. */
const RevNode* RevTreeFindNode(RevTree *tree, sized_buf revID);

/** Gets a node in a rev tree by index, without parsing it first.
 *  This is more efficient if you only need to look up one node.
 *  Returns NULL if index is out of range. */
bool RevTreeRawGetNode(sized_buf raw_tree, unsigned index, RevNode *outNode);

/** Finds a node in a rev tree without parsing it first.
 *  This is more efficient if you only need to look up one node. */
bool RevTreeRawFindNode(sized_buf raw_tree, sized_buf revID, RevNode *outNode);

/** Returns true if the tree has a conflict (multiple nondeleted leaf revisions.) */
bool RevTreeHasConflict(RevTree *tree);


/** Reserves room for up to 'extraCapacity' insertions.
    May reallocate the tree, invalidating all existing RevNode pointers. */
bool RevTreeReserveCapacity(RevTree **pTree, unsigned extraCapacity);

/** Adds a revision to a tree. The tree's capacity MUST be greater than its count.
 *  The memory pointed to by revID and data MUST remain valid until after the tree is freed. */
bool RevTreeInsert(RevTree **treeP,
                   sized_buf revID,
                   sized_buf data,
                   bool deleted,
                   sized_buf parentRevID,
                   bool allowConflict);

/** Somewhat lower-level version of RevTreeInsert that takes a RevNode* for the parent and returns
    a RevNode* for the new node. It also does _not_ verify that the new revID doesn't exist! */
const RevNode* RevTreeInsertPtr(RevTree **treeP,
                                sized_buf revID,
                                sized_buf data,
                                bool deleted,
                                const RevNode* parent,
                                bool allowConflict);

/** Adds a revision along with its ancestry.
    @param treeP  Pointer to the RevTree (may be changed if the tree has to be grown)
    @param history  Rev IDs of new revision and its ancestors (reverse chronological order)
    @param historyCount  Number of items in the history array
    @param data  The body of the new revision
    @param deleted  True if the new revision is a deletion/tombstone
    @return  The number of revisions added, or 0 if the new revision already exists,
             or a negative number on failure. */
int RevTreeInsertWithHistory(RevTree** treeP,
                             const sized_buf history[],
                             unsigned historyCount,
                             sized_buf data,
                             bool deleted);

/** Limits the maximum depth of the tree by removing the oldest nodes, if necessary. */
void RevTreePrune(RevTree* tree, unsigned maxDepth);

/** Sorts the nodes of a tree so that the current node(s) come first.
    Nodes are normally sorted already, but RevTreeInsert will leave them unsorted.
    Note that sorting will invalidate any pre-existing RevNode pointers!
    RevTreeSort is called automatically by RevTreeEncode. */
void RevTreeSort(RevTree *tree);

#ifdef REVTREE_USES_FILE_OFFSETS
/** Removes all body file offsets in an encoded tree; should be called as part of a document
    mutator during compaction, since compaction invalidates all existing file offsets.
    Returns true if any changes were made. */
bool RevTreeRawClearBodyOffsets(sized_buf *raw_tree);
#endif


/** Parses a revision ID into its generation-number prefix and digest suffix.
    The generation number will be nonzero. The digest will be non-empty.
    Returns false if the ID is not of the right form. */
bool RevIDParse(sized_buf rev, unsigned *generation, sized_buf *digest);

/** Parses a compacted revision ID. */
bool RevIDParseCompacted(sized_buf rev, unsigned *generation, sized_buf *digest);

/** Compacts a compressed revision ID. The compact form is written to dstrev->buf (which must
    be large enough to contain it), and dstrev->size is updated.
    Returns false if the input ID is not parseable. */
bool RevIDCompact(sized_buf srcrev, sized_buf *dstrev);

/** Returns the expanded size of a compressed revision ID: the number of bytes of the expanded form.
    If an uncompressed revision ID is given, 0 is returned to signal that expansion isn't needed. */
size_t RevIDExpandedSize(sized_buf rev);

/** Expands a compressed revision ID. The expanded form is written to expanded_rev->buf (which must
    be large enough to contain it), and expanded_rev->size is updated.
    If the revision ID is not compressed, it's copied as-is to expanded_rev. */
void RevIDExpand(sized_buf rev, sized_buf* expanded_rev);


#endif