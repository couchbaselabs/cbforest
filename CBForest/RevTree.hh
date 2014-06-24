//
//  RevTree.hh
//  CBForest
//
//  Created by Jens Alfke on 5/13/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//

#ifndef __CBForest__RevTree__
#define __CBForest__RevTree__

#include "slice.hh"
#include "RevID.hh"
#include "Database.hh"
#include <vector>


namespace forestdb {

    class RevTree;
    class RawRevision;

    /** In-memory representation of a single revision's metadata. */
    class Revision {
    public:
        const RevTree*  owner;
        revid           revID;      /**< Revision ID (compressed) */
        fdb_seqnum_t    sequence;   /**< DB sequence number that this revision has/had */
        slice           body;       /**< Revision body (JSON), or empty if not stored in this tree */

        inline bool isBodyAvailable() const;
        inline alloc_slice readBody() const;

        bool isLeaf() const    {return (flags & kLeaf) != 0;}
        bool isDeleted() const {return (flags & kDeleted) != 0;}
        bool isNew() const     {return (flags & kNew) != 0;}
        bool isActive() const  {return isLeaf() && !isDeleted();}

        unsigned index() const;
        const Revision* parent() const;
        std::vector<const Revision*> history() const;

        bool operator< (const Revision& rev) const;

    private:
        enum Flags : uint8_t {
            kDeleted = 0x01,    /**< Is this revision a deletion/tombstone? */
            kLeaf    = 0x02,    /**< Is this revision a leaf (no children?) */
            kNew     = 0x04     /**< Has this rev been inserted since decoding? */
        };

        static const uint16_t kNoParent = UINT16_MAX;
        
        uint64_t    oldBodyOffset;  /**< File offset of doc containing revision body, or else 0 */
        uint16_t    parentIndex;    /**< Index in tree's rev[] array of parent revision, if any */
        Flags       flags;          /**< Leaf/deleted flags */

        void read(const RawRevision *src);
        RawRevision* write(RawRevision* dst, uint64_t bodyOffset) const;
        size_t sizeToWrite() const;
        void addFlag(Flags f)      {flags = (Flags)(flags | f);}
        void clearFlag(Flags f)    {flags = (Flags)(flags & ~f);}
        friend class RevTree;
        friend class RawRevision;
    };


    /** A serializable tree of Revisions. */
    class RevTree {
    public:
        RevTree();
        RevTree(slice raw_tree, sequence seq, uint64_t docOffset);
        virtual ~RevTree();

        void decode(slice raw_tree, sequence seq, uint64_t docOffset);

        alloc_slice encode();

        size_t size() const                             {return _revs.size();}
        const Revision* get(unsigned index) const;
        const Revision* get(revid) const;
        const Revision* operator[](unsigned index) const {return get(index);}
        const Revision* operator[](revid revID) const    {return get(revID);}

        const std::vector<Revision>& allRevisions() const    {return _revs;}
        const Revision* currentRevision();
        std::vector<const Revision*> currentRevisions() const;
        bool hasConflict() const;

        const Revision* insert(revid, slice body, bool deleted,
                              revid parentRevID,
                              bool allowConflict,
                              int &httpStatus);
        const Revision* insert(revid, slice body, bool deleted,
                              const Revision* parent,
                              bool allowConflict,
                              int &httpStatus);
        int insertHistory(const std::vector<revid> history, slice body, bool deleted);

        unsigned prune(unsigned maxDepth);
        std::vector<revid> purge(const std::vector<revid>revIDs);

        void sort();

    protected:
        virtual bool isBodyOfRevisionAvailable(const Revision*, uint64_t atOffset) const;
        virtual alloc_slice readBodyOfRevision(const Revision*, uint64_t atOffset) const;

    private:
        friend class Revision;
        const Revision* _insert(revid, slice body, const Revision *parentRev, bool deleted);
        void compact();
        RevTree(const RevTree&); // forbidden

        uint64_t    _bodyOffset;     // File offset of body this tree was read from
        bool        _sorted;         // Are the revs currently sorted?
        std::vector<Revision> _revs;
        std::vector<alloc_slice> _insertedData;
    protected:
        bool _changed;
        bool _unknown;
    };


    inline bool Revision::isBodyAvailable() const {
        return owner->isBodyOfRevisionAvailable(this, oldBodyOffset);
    }
    inline alloc_slice Revision::readBody() const {
        return owner->readBodyOfRevision(this, oldBodyOffset);
    }

}

#endif /* defined(__CBForest__RevTree__) */