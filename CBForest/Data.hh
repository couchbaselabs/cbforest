//
//  Data.hh
//  CBForest
//
//  Created by Jens Alfke on 1/25/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef CBForest_Data_h
#define CBForest_Data_h
#include "slice.hh"
#include <stdint.h>


namespace forestdb {

    class array;
    class dict;
    class dictIterator;
        

    /* Types of values */
    enum valueType {
        kNull = 0,
        kBoolean,
        kNumber,
        kString,
        kData,
        kArray,
        kDict
    };


    /* A raw value */
    class value {
    public:
        valueType type() const;
        const value* next() const;

        bool asBool() const;
        int64_t asInt() const;
        double asDouble() const;
        slice asString() const;
        bool isExternString() const             {return _typeCode == kExternStringCode;}
        uint64_t externStringIndex() const;

        const array* asArray() const;
        const dict* asDict() const;

    protected:
        enum typeCodes {
            kNullCode = 0,
            kFalseCode, kTrueCode,
            kInt8Code, kInt16Code, kInt32Code, kInt64Code, kUInt64Code,
            kFloat32Code, kFloat64Code,
            kRawNumberCode,
            kStringCode, kSharedStringCode, kExternStringCode,
            kDataCode,
            kArrayCode,
            kDictCode,
        };
        
        uint8_t _typeCode;
        uint8_t _paramStart[1];

        size_t getParam() const;
        size_t getParam(const uint8_t* &after) const;
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
        const value* get(slice key, uint16_t hash) const;

        static uint16_t hashCode(slice);

    private:
        const value* firstKey(size_t &count) const;
        friend class iterator;
        friend class value;
    };


#if 0 // not doing it this way(?)
    class dict : public value {
    public:
        class iterator {
        public:
            iterator(const dict*);
            uint16_t hash() const       {return _entry->hash;}  //TODO: Endian conversion
            const value* key() const;
            const value* value() const  {return key()->next();}

            operator bool() const       {return _count > 0;}
            inline iterator& operator++();

        private:
            struct raw {
                uint16_t hash;
                uint32_t offset;
            };
            const dict* _dict;
            size_t _count;
            const raw* _entry;
            uint8_t _step;
            friend class dict;
        };

        size_t count() const                                {return getParam();}
        const value* get(slice key) const                   {return get(key, hashCode(key));}
        const value* get(slice key, uint16_t hash) const;
    };
    dict::iterator& dict::iterator::operator++() {
        --_count;
        _entry = (const raw*) offsetby(_entry, _step);
        return *this;
    }
#endif


}

#endif
