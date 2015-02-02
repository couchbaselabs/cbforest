//
//  Encoding.hh
//  CBForest
//
//  Created by Jens Alfke on 1/25/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef CBForest_Data_h
#define CBForest_Data_h
#include "slice.hh"
#include <ctime>
#include <stdint.h>
#include <vector>


namespace forestdb {

    class array;
    class dict;
    class dictIterator;
        

    /* Types of values */
    enum valueType : uint8_t {
        kNull = 0,
        kBoolean,
        kNumber,
        kDate,
        kString,
        kData,
        kArray,
        kDict
    };


    /* A raw value */
    class value {
    public:
        /** Makes sure the slice contains a valid value (and nothing else.)
            If so, returns a pointer to it; else NULL. */
        static const value* validate(slice);

        valueType type() const;
        const value* next() const;

        bool asBool() const;
        int64_t asInt() const;
        uint64_t asUnsigned() const;
        double asDouble() const;
        bool isInteger() const                  {return _typeCode >= kInt8Code
                                                     && _typeCode <= kUInt64Code;}
        bool isUnsigned() const                 {return _typeCode == kUInt64Code;}

        std::time_t asDate() const;
        std::string asDateString() const;

        /** Returns the exact contents of a (non-extern) string, data, or raw number. */
        slice asString() const;
        bool isExternString() const             {return _typeCode == kExternStringRefCode;}
        bool isSharedString() const             {return _typeCode == kSharedStringCode
                                                     || _typeCode == kSharedStringRefCode;}

        /** If this is an extern string, returns its external identifier.
            If this is a shared string, returns an opaque value identifying it; all instances of
            the same shared string in the same document will have the same identifier. */
        uint64_t stringToken() const;

        const array* asArray() const;
        const dict* asDict() const;

        /** Converts any non-collection type (except externString) to string form. */
        std::string toString() const;

        void writeJSON(std::ostream&, const std::vector<std::string> *externStrings) const;
        std::string toJSON(const std::vector<std::string> *externStrings =NULL) const;

#ifdef __OBJC__
        id asNSObject(NSArray* externStrings =nil) const;
        id asNSObject(NSMapTable *sharedStrings, NSArray* externStrings =nil) const;
        static NSMapTable* createSharedStringsTable();
#endif

    protected:
        enum typeCode : uint8_t {
            kNullCode = 0,
            kFalseCode, kTrueCode,
            kInt8Code, kInt16Code, kInt32Code, kInt64Code, kUInt64Code,
            kFloat32Code, kFloat64Code,
            kRawNumberCode,
            kDateCode,
            kStringCode, kSharedStringCode, kSharedStringRefCode, kExternStringRefCode,
            kDataCode,
            kArrayCode,
            kDictCode,
        };
        
        typeCode _typeCode;
        uint8_t _paramStart[1];

        size_t getParam() const;
        size_t getParam(const uint8_t* &after) const;
        static bool validate(const void* start, slice&);
        friend class dataWriter;
};


    class array : public value {
    public:
        size_t count() const            {return getParam();}
        const value* first() const;
    };


    class dict : public value {
    public:
        class iterator {
        public:
            iterator(const dict*);
            const value* key() const    {return _key;}
            const value* value() const  {return _value;}

            operator bool() const       {return _count > 0;}
            iterator& operator++();

        private:
            size_t _count;
            const class value* _key;
            const class value* _value;
        };

        size_t count() const                {return getParam();}
        const value* get(slice key) const   {return get(key, hashCode(key));}
        const value* get(slice key, uint16_t hashCode) const;

        static uint16_t hashCode(slice);

    private:
        const value* firstKey(size_t &count) const;
        friend class iterator;
        friend class value;
    };

}

#endif
