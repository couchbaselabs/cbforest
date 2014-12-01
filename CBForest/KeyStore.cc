//
//  KeyStore.cc
//  CBForest
//
//  Created by Jens Alfke on 11/12/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//

#include "KeyStore.hh"
#include "Document.hh"
#include "LogInternal.hh"

namespace forestdb {

    static void check(fdb_status status) {
        if (status != FDB_RESULT_SUCCESS) {
            WarnError("FORESTDB ERROR %d\n", status);
            throw error{status};
        }
    }


    KeyStore::KeyStore(const Database* db, std::string name)
    :_handle(NULL)
    {
        check(::fdb_kvs_open(db->_fileHandle, &_handle, name.c_str(), NULL));
    }

    KeyStore::kvinfo KeyStore::getInfo() const {
        kvinfo i;
        check(fdb_get_kvs_info(_handle, &i));
        return i;
    }

    std::string KeyStore::name() const {
        return std::string(getInfo().name);
    }

    sequence KeyStore::lastSequence() const {
        fdb_seqnum_t seq;
        check(fdb_get_kvs_seqnum(_handle, &seq));
        return seq;
    }

    static bool checkGet(fdb_status status) {
        if (status == FDB_RESULT_KEY_NOT_FOUND)
            return false;
        check(status);
        return true;
    }

    Document KeyStore::get(slice key, contentOptions options) const {
        Document doc(key);
        read(doc, options);
        return doc;
    }

    Document KeyStore::get(sequence seq, contentOptions options) const {
        Document doc;
        doc._doc.seqnum = seq;
        if (options & kMetaOnly)
            check(fdb_get_metaonly_byseq(_handle, &doc._doc));
        else
            check(fdb_get_byseq(_handle, doc));
        return doc;
    }

    bool KeyStore::read(Document& doc, contentOptions options) const {
        doc.clearMetaAndBody();
        if (options & kMetaOnly)
            return checkGet(fdb_get_metaonly(_handle, doc));
        else
            return checkGet(fdb_get(_handle, doc));
    }

    Document KeyStore::getByOffset(uint64_t offset, sequence seq) const {
        Document doc;
        doc._doc.offset = offset;
        doc._doc.seqnum = seq;
        checkGet(fdb_get_byoffset(_handle, doc));
        return doc;
    }

    void KeyStore::deleteKeyStore(bool recreate) {
        kvinfo i = getInfo();
        std::string name;
        if (recreate)
            name = i.name; // save a copy of the name
        check(fdb_kvs_close(_handle));
        _handle = NULL;
        check(fdb_kvs_remove(i.file, i.name));
        if (recreate)
            check(::fdb_kvs_open(i.file, &_handle, name.c_str(), NULL));
    }


#pragma mark - KEYSTOREWRITER:


    void KeyStoreWriter::rollbackTo(sequence seq) {
        check(fdb_rollback(&_handle, seq));
    }

    void KeyStoreWriter::write(Document &doc) {
        check(fdb_set(_handle, doc));
    }

    sequence KeyStoreWriter::set(slice key, slice meta, slice body) {
        if ((size_t)key.buf & 0x03) {
            // Workaround for unaligned-access crashes on ARM (down in forestdb's crc_32_8 fn)
            void* keybuf = alloca(key.size);
            memcpy(keybuf, key.buf, key.size);
            key.buf = keybuf;
        }
        // for GCC 4.9?? compiler
        fdb_doc doc;
        doc.key = (void*)key.buf;
        doc.keylen = key.size;
        doc.meta = (void*)meta.buf;
        doc.metalen = meta.size;
        doc.body  = (void*)body.buf;
        doc.bodylen = body.size;
        doc.size_ondisk = 0;
        doc.seqnum = 0;
        doc.offset = 0;
        doc.deleted = (bool)0;
        check(fdb_set(_handle, &doc));
        if (meta.buf) {
            Log("DB %p: added %s --> %s (meta %s) (seq %llu)\n",
                    _handle,
                    key.hexString().c_str(),
                    body.hexString().c_str(),
                    meta.hexString().c_str(),
                    doc.seqnum);
        } else {
            Log("DB %p: added %s --> %s (seq %llu)\n",
                    _handle,
                    key.hexString().c_str(),
                    body.hexString().c_str(),
                    doc.seqnum);
        }
        return doc.seqnum;
    }

    bool KeyStoreWriter::del(forestdb::Document &doc) {
        return checkGet(fdb_del(_handle, doc));
    }

    bool KeyStoreWriter::del(forestdb::slice key) {
        if ((size_t)key.buf & 0x03) {
            // Workaround for unaligned-access crashes on ARM (down in forestdb's crc_32_8 fn)
            void* keybuf = alloca(key.size);
            memcpy(keybuf, key.buf, key.size);
            key.buf = keybuf;
        }
        // for GCC 4.9?? compiler
        fdb_doc doc;
        doc.key = (void*)key.buf;
        doc.keylen = key.size;
        doc.meta = (void*)0;
        doc.metalen = 0;
        doc.body  = (void*)0;
        doc.bodylen = 0;
        doc.size_ondisk = 0;
        doc.seqnum = 0;
        doc.offset = 0;
        doc.deleted = (bool)0;      
        return checkGet(fdb_del(_handle, &doc));
    }

    bool KeyStoreWriter::del(sequence seq) {
        Document doc;
        doc._doc.seqnum = seq;
        return checkGet(fdb_get_metaonly_byseq(_handle, doc))
            && del(doc);
    }

}
