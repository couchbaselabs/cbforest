package com.couchbase.cbforest;

import java.util.List;
import java.util.Map;


public class View {

    //////// VIEWS

    public View(Database sourceDB, String viewDbPath,
                int viewDbFlags, int encryptionAlgorithm, byte[] encryptionKey,
                String viewName, String version) throws ForestException {
        _dbHandle = sourceDB._handle;
        _handle = _open(sourceDB._handle, viewDbPath, viewDbFlags, encryptionAlgorithm,
                        encryptionKey, viewName, version);
    }

    protected void finalize() {
        closeView();
    }

    public void closeView(){
        close();
        _handle = 0;
        _dbHandle = 0;
    }

    // native methods for view
    private native long _open(long sourceDbHandle, String viewDbPath, int viewDbFlags,
                              int encryptionAlgorithm, byte[] encryptionKey,
                              String viewName, String version) throws ForestException;
    public native void close();
    public native void rekey(int encryptionAlgorithm, byte[] encryptionKey) throws ForestException;
    public native void eraseIndex() throws ForestException;
    public native void delete() throws ForestException;
    public native long getTotalRows();
    public native long getLastSequenceIndexed();
    public native long getLastSequenceChangedAt();

    protected long _handle;    // handle to native C4View*
    protected long _dbHandle; // handle to native C4Database*


    //////// QUERYING:
    public QueryIterator query() throws ForestException {
        return new QueryIterator(query(_handle));
    }

    public QueryIterator query(long skip,
                               long limit,
                               boolean descending,
                               boolean inclusiveStart,
                               boolean inclusiveEnd,
                               Object startKey,
                               Object endKey,
                               String startKeyDocID,
                               String endKeyDocID) throws ForestException
    {
        return new QueryIterator(query(_handle,
                skip,
                limit,
                descending,
                inclusiveStart,
                inclusiveEnd,
                objectToKey(startKey, false),
                objectToKey(endKey, false),
                startKeyDocID,
                endKeyDocID));
    }


    public QueryIterator query(long skip,
                               long limit,
                               boolean descending,
                               boolean inclusiveStart,
                               boolean inclusiveEnd,
                               Object keys[]) throws ForestException
    {
        long keyHandles[] = new long[keys.length];
        int i = 0;
        for (Object key : keys) {
            keyHandles[i++] = objectToKey(key, true);
        }
        return new QueryIterator(query(_handle, skip, limit, descending,
                                       inclusiveStart, inclusiveEnd, keyHandles));
    }

    // native methods for query

    private static native long query(long viewHandle) throws ForestException;

    private static native long query(long viewHandle,   // C4View*
                                     long skip,
                                     long limit,
                                     boolean descending,
                                     boolean inclusiveStart,
                                     boolean inclusiveEnd,
                                     long startKey,     // C4Key*
                                     long endKey,       // C4Key*
                                     String startKeyDocID,
                                     String endKeyDocID) throws ForestException;

    private static native long query(long viewHandle,   // C4View*
                                     long skip,
                                     long limit,
                                     boolean descending,
                                     boolean inclusiveStart,
                                     boolean inclusiveEnd,
                                     long keys[])  // array of C4Key*
            throws ForestException;


    //////// KEY:

    /**
     * Key generation:
     *
     * @param o
     * @param bNullKey if true, create null-key if o is null,
     *                 otherwise, return 0 which indicates no-key
     * @return address to key instance in native layer
     */
    static long objectToKey(Object o, boolean bNullKey) {
        if (o == null && !bNullKey) {
            // 0 -> noKey
            return 0;
        } else {
            // o == null -> NullKey
            long key = newKey();
            try {
                keyAdd(key, o);
                return key;
            } catch (Error t) {
                freeKey(key);
                throw t; //? Is this correct?
            }
        }
    }

    private static void keyAdd(long key, Object o) {
        if (o == null) {
            keyAddNull(key);
        } else if (o instanceof Boolean) {
            keyAdd(key, ((Boolean)o).booleanValue());
        } else if (o instanceof Number) {
            keyAdd(key, ((Number)o).doubleValue());
        } else if (o instanceof String) {
            keyAdd(key, (String)o);
        } else if (o instanceof Object[]) {
            keyBeginArray(key);
            for (Object item : (Object[])o) {
                keyAdd(key, item);
            }
            keyEndArray(key);
        } else if (o instanceof List) {
            keyBeginArray(key);
            for (Object item : (List)o) {
                keyAdd(key, item);
            }
            keyEndArray(key);
        } else if (o instanceof Map) {
            keyBeginMap(key);
            //FIX: The next line gives a warning about an unchecked cast. How to fix it?
            for (Map.Entry entry : ((Map<String,Object>)o).entrySet()) {
                keyAdd(key, (String)entry.getKey());
                keyAdd(key, entry.getValue());
            }
            keyEndMap(key);
        } else {
            throw new Error("invalid class for JSON"); //FIX: What's the correct error class?
        }
    }

    // native methods for Key
    static native long   newKey();
    static native void   freeKey(long key);
    static native void   keyAddNull(long key);
    static native void   keyAdd(long key, boolean b);
    static native void   keyAdd(long key, double d);
    static native void   keyAdd(long key, String s);
    static native void   keyBeginArray(long key);
    static native void   keyEndArray(long key);
    static native void   keyBeginMap(long key);
    static native void   keyEndMap(long key);
    static native String keyToJSON(long key);
    static native long   keyReader(long key);

    // native methods for KeyReader
    static native int     keyPeek(long reader);
    static native void    keySkipToken(long reader);
    static native boolean keyReadBool(long reader);
    static native double  keyReadNumber(long reader);
    static native String  keyReadString(long reader);
    static native void    freeKeyReader(long reader);
}
