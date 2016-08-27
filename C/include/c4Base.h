//
//  c4Base.h
//  Couchbase Lite Core
//
//  Basic types and functions for C API.
//
//  Created by Jens Alfke on 9/8/15.
//  Copyright (c) 2015-2016 Couchbase. All rights reserved.
//

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef _MSC_VER
#define __has_extension
#define __has_feature
#define CBINLINE __forceinline
#else
#define CBINLINE inline
#endif

// Macros for defining typed enumerations and option flags.
// To define an enumeration whose values won't be combined:
//      typedef C4_ENUM(baseIntType, name) { ... };
// To define an enumeration of option flags that will be ORed together:
//      typedef C4_OPTIONS(baseIntType, name) { ... };
// These aren't just a convenience; they are required for Swift bindings.
#if APPLE
    #include <CoreFoundation/CFBase.h>      /* for CF_ENUM and CF_OPTIONS macros */
    #define C4_ENUM CF_ENUM
    #define C4_OPTIONS CF_OPTIONS
#else
    #if (__cplusplus && __cplusplus >= 201103L && (__has_extension(cxx_strong_enums) || __has_feature(objc_fixed_enum))) || (!__cplusplus && __has_feature(objc_fixed_enum))
        #define C4_ENUM(_type, _name)     enum _name : _type _name; enum _name : _type
        #if (__cplusplus)
            #define C4_OPTIONS(_type, _name) _type _name; enum : _type
        #else
            #define C4_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
        #endif
    #else
        #define C4_ENUM(_type, _name) _type _name; enum
        #define C4_OPTIONS(_type, _name) _type _name; enum
    #endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#ifdef CBLCORE_EXPORTS
#define CBL_CORE_API __declspec(dllexport)
#else
#define CBL_CORE_API __declspec(dllimport)
#endif
#else // _MSC_VER
#define CBL_CORE_API
#endif


/** A database sequence number, representing the order in which a revision was created. */
typedef uint64_t C4SequenceNumber;


//////// ERRORS:


typedef C4_ENUM(uint32_t, C4ErrorDomain) {
    CBLCoreDomain = 1, // code is a CBL Core error code (see below)
    POSIXDomain,        // code is an errno
    ForestDBDomain,     // code is a fdb_status
    SQLiteDomain,       // code is a SQLite error
};


// CBLCoreDomain error codes:
// (These are identical to the internal C++ error::CBLCoreError enum values.)
enum {
    kC4ErrorAssertionFailed = 1,    // Internal assertion failure
    kC4ErrorUnimplemented,          // Oops, an unimplemented API call
    kC4ErrorNoSequences,            // This KeyStore does not support sequences
    kC4ErrorUnsupportedEncryption,  // Unsupported encryption algorithm
    kC4ErrorNoTransaction,          // Function must be called within a transaction
    kC4ErrorBadRevisionID,          // Invalid revision ID syntax
    kC4ErrorBadVersionVector,       // Invalid version vector syntax
    kC4ErrorCorruptRevisionData,    // Revision contains corrupted/unreadable data
    kC4ErrorCorruptIndexData,       // Index contains corrupted/unreadable data
    kC4ErrorTokenizerError,         // can't create text tokenizer for FTS
    kC4ErrorNotOpen,                // Database/KeyStore/index is not open
    kC4ErrorNotFound,               // Document not found
    kC4ErrorDeleted,                // Document has been deleted
    kC4ErrorConflict,               // Document update conflict
    kC4ErrorInvalidParameter,       // Invalid function parameter or struct value
    kC4ErrorDatabaseError,          // Lower-level database error (ForestDB or SQLite)
    kC4ErrorUnexpectedError,        // Internal unexpected C++ exception
    kC4ErrorCantOpenFile,           // Database file can't be opened; may not exist
    kC4ErrorIOError,                // File I/O error
    kC4ErrorCommitFailed,           // Transaction commit failed
    kC4ErrorMemoryError,            // Memory allocation failed (out of memory?)
    kC4ErrorNotWriteable,           // File is not writeable
    kC4ErrorCorruptData,            // Data is corrupted
    kC4ErrorBusy,                   // Database is busy/locked
    kC4ErrorNotInTransaction,       // Function cannot be called while in a transaction
    kC4ErrorTransactionNotClosed,   // Database can't be closed while a transaction is open
    kC4ErrorIndexBusy,              // View can't be closed while index is enumerating
    kC4ErrorUnsupported,            // Operation not supported in this database
    kC4ErrorNotADatabaseFile,       // File is not a database, or encryption key is wrong
    kC4ErrorWrongFormat,            // Database exists but not in the format/storage requested
};


/** An error value. These are returned by reference from API calls whose last parameter is a
    C4Error*. The semantics are based on Cocoa's usage of NSError:
    * A caller can pass NULL if it doesn't care about the error.
    * The error is filled in only if the function fails, as indicated by its return value
      (e.g. false or NULL.) If the function doesn't fail, it does NOT zero out the error, so its
      contents should be considered uninitialized garbage. */
typedef struct {
    C4ErrorDomain domain;
    int32_t code;
} C4Error;


//////// SLICES:


#ifndef C4_IMPL

/** A slice is simply a pointer to a range of bytes, usually interpreted as a UTF-8 string.
    A "null slice" has chars==NULL and length==0.
    A slice with length==0 is not necessarily null; if chars!=NULL it's an empty string.
    A slice as a function parameter is temporary and read-only: the function will not alter or free
    the bytes, and the pointer won't be accessed after the function returns.
    A slice _returned from_ a function points to newly-allocated memory and must be freed by the
    caller, with c4slice_free(). */
typedef struct {
    const void *buf;
    size_t size;
} C4Slice;

/** Denotes a slice returned from a function, which needs to have its buf freed by the caller. */
typedef C4Slice C4SliceResult;

/** Creates a slice pointing to the contents of a C string. */
static CBINLINE C4Slice c4str(const char *str) {
    C4Slice foo = { str, str ? strlen(str) : 0 };
    return foo;
}

// Macro version of c4str, for use in initializing compile-time constants.
// STR must be a C string literal.
#ifdef _MSC_VER
#define C4STR(STR) {("" STR), sizeof(("" STR))-1}
#else
#define C4STR(STR) ((C4Slice){("" STR), sizeof(("" STR))-1})
#endif

// A convenient constant denoting a null slice.
#ifdef _MSC_VER
const C4Slice kC4SliceNull = { NULL, 0 };
#else
#define kC4SliceNull ((C4Slice){NULL, 0})
#endif

#endif // C4_IMPL

/** Returns true if two slices have equal contents. */
bool c4SliceEqual(C4Slice a, C4Slice b);

/** Frees the memory of a heap-allocated slice by calling free(buf). */
void c4slice_free(C4Slice);


/** Returns an error message describing a C4Error. Remember to free the result. */
C4SliceResult c4error_getMessage(C4Error error);

/** Writes an error message describing a C4Error to a buffer, as a C string.
    It will not write past the end of the buffer; the message will be truncated if necessary.
    @param error  The error to describe
    @param buffer  Where to write the C string to
    @param bufferSize  The size of the buffer
    @return  A pointer to the string, i.e. to the first byte of the buffer. */
char* c4error_getMessageC(C4Error error, char buffer[], size_t bufferSize);


/** Logging levels. */
typedef C4_ENUM(uint8_t, C4LogLevel) {
    kC4LogDebug,
    kC4LogInfo,
    kC4LogWarning,
    kC4LogError
};

/** A logging callback that the application can register. */
typedef void (*C4LogCallback)(C4LogLevel level, C4Slice message);

/** Registers (or unregisters) a log callback, and sets the minimum log level to report.
    Before this is called, logs are by default written to stderr for warnings and errors.
    Note that this setting is global to the entire process.
    @param level  The minimum level of message to log.
    @param callback  The logging callback, or NULL to disable logging entirely. */
void c4log_register(C4LogLevel level, C4LogCallback callback);

/** Changes the log level. */
void c4log_setLevel(C4LogLevel level);


/** Returns the number of objects that have been created but not yet freed. */
int c4_getObjectCount(void);


#ifdef __cplusplus
}
#endif
