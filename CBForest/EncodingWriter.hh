//
//  EncodingWriter.h
//  CBForest
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef __CBForest__DataWriter__
#define __CBForest__DataWriter__

#include "Encoding.hh"
#include <ctime>
#include <iostream>
#include <unordered_map>

namespace forestdb {


    class dataWriter {
    public:
        dataWriter(std::ostream&,
                   const std::unordered_map<std::string, uint32_t> *externStrings = NULL);

        void writeNull();
        void writeBool (bool);

        void writeInt(int64_t);
        void writeUInt(uint64_t);
        void writeFloat(float);
        void writeDouble(double);

        void writeDate(std::time_t);

        void writeString(std::string);
        void writeString(slice);

        void writeData(slice);

        void beginArray(uint64_t count);
        void endArray()                             { }

        void beginDict(uint64_t count);
        void writeKey(std::string);
        void writeKey(slice);
        void endDict();

        // Note: overriding <<(bool) would be dangerous due to implicit conversion
        dataWriter& operator<< (int64_t i)          {writeInt(i); return *this;}
        dataWriter& operator<< (double d)           {writeDouble(d); return *this;}
        dataWriter& operator<< (float f)            {writeFloat(f); return *this;}
        dataWriter& operator<< (std::string str)    {writeString(str); return *this;}
        dataWriter& operator<< (slice s)            {writeString(s); return *this;}

#ifdef __OBJC__
        void write(id);
#endif

    private:
        void addTypeCode(value::typeCode code)      {_out.write((char*)&code, 1);}
        void addUVarint(uint64_t);

        std::ostream& _out;
        size_t _indexPos;
        std::vector<size_t> _savedIndexPos;
        std::unordered_map<std::string, size_t> _sharedStrings;
        const std::unordered_map<std::string, uint32_t> *_externStrings;
    };


}

#endif /* defined(__CBForest__DataWriter__) */
