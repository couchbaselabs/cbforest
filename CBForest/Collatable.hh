//
//  Collatable.hh
//  CBForest
//
//  Created by Jens Alfke on 5/14/14.
//  Copyright (c) 2014 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

#ifndef __CBForest__Collatable__
#define __CBForest__Collatable__
#include <stdint.h>
#include <string>
#include "Writer.hh"
#include "slice.hh"
#include "Geohash.hh"

namespace forestdb {

    class CollatableTypes {
    public:
        typedef enum {
            kEndSequence = 0,   // Returned to indicate the end of an array/dict
            kNull,
            kFalse,
            kTrue,
            kNegative,
            kPositive,
            kString,
            kArray,
            kMap,
            kGeohash,           // Geohash string
            kSpecial,           // Placeholder for doc (Only used in values, not keys)
            kError = 255        // Something went wrong. (Never stored, only returned from peekTag)
        } Tag;
    };

    /** A binary encoding of JSON-compatible data, that collates with CouchDB-compatible semantics
        using a dumb binary compare (like memcmp).
        Data format spec: https://github.com/couchbaselabs/cbforest/wiki/Collatable-Data-Format
        Collatable owns its data, in the form of a C++ string object. */
    class Collatable : public CollatableTypes {
    public:
        Collatable();
        Collatable(const Collatable&);
        Collatable(Collatable&&);
        Collatable(slice, bool);        // Imports data previously saved in collatable format

        template<typename T> explicit Collatable(const T &t)    {*this << t;}

        Collatable& operator= (Collatable&&);

        Collatable& addNull()                       {addTag(kNull); return *this;}
        Collatable& addBool (bool); // overriding <<(bool) is dangerous due to implicit conversion

        Collatable& operator<< (double);

        Collatable& operator<< (const Collatable&);
        Collatable& operator<< (std::string str)    {return addEncoded(kString, slice(str));}
        Collatable& operator<< (const char* cstr)   {return operator<<(slice(cstr));}
        Collatable& operator<< (slice str)          {return addEncoded(kString, str);}

        Collatable& operator<< (const geohash::hash& h)   {return addEncoded(kGeohash, slice(h));}

        Collatable& beginArray()                    {addTag(kArray); return *this;}
        Collatable& endArray()                      {addTag(kEndSequence); return *this;}

        Collatable& beginMap()                      {addTag(kMap); return *this;}
        Collatable& endMap()                        {addTag(kEndSequence); return *this;}

#ifdef __OBJC__
        Collatable(id obj)                          { if (obj) *this << obj;}
        Collatable& operator<< (id);
#endif

        Collatable& addSpecial()                    {addTag(kSpecial); return *this;}

        operator slice() const                      {return _out.output();}
        size_t size() const                         {return _out.length();}
        bool empty() const                          {return size() == 0;}
        bool operator< (const Collatable& c) const  {return _out.output().compare(c) < 0;}
        bool operator== (const Collatable& c) const {return _out.output().compare(c) == 0;}

        std::string toJSON() const;

    private:
        void addTag(Tag t)                          {_out << (uint8_t)t;}
        void add(slice s)                           {_out << s;}
        Collatable& addEncoded(Tag t, slice s);
        Writer _out;
    };


    /** A decoder of Collatable-format data. Does _not_ own its data (reads from a slice.) */
    class CollatableReader : public CollatableTypes {
    public:
        CollatableReader(slice s);

        slice data() const                  {return _data;}
        bool atEnd() const                  {return _data.size == 0;}
        
        Tag peekTag() const;
        void skipTag()                      {if (_data.size > 0) _skipTag();}

        int64_t readInt();
        double readDouble();
        alloc_slice readString();
        geohash::hash readGeohash();

#ifdef __OBJC__
        id readNSObject();
        NSString* readNSString();
#endif

        /** Reads (skips) an entire object of any type, returning its data in Collatable form. */
        slice read();

        void beginArray();
        void endArray();
        void beginMap();
        void endMap();

        void writeJSONTo(std::ostream &out);
        std::string toJSON();

        static uint8_t* getInverseCharPriorityMap();

    private:
        void expectTag(Tag tag);
        void _skipTag()                     {_data.moveStart(1);} // like skipTag but unsafe
        alloc_slice readString(Tag);

        slice _data;
    };

}

#endif /* defined(__CBForest__Collatable__) */
