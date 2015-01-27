//
//  DataWriter.h
//  CBForest
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#ifndef __CBForest__DataWriter__
#define __CBForest__DataWriter__

#include "Data.hh"
#include <iostream>
#include <unordered_map>

namespace forestdb {


    class dataWriter {
    public:
        dataWriter(std::ostream&);

        dataWriter& writeNull();
        dataWriter& writeBool (bool); // overriding <<(bool) is dangerous due to implicit conversion

        dataWriter& writeInt(int64_t);
        dataWriter& writeFloat(float);
        dataWriter& writeDouble(double);

        dataWriter& writeString(std::string);
        dataWriter& writeString(slice);
        dataWriter& writeExternString(unsigned stringID);

        dataWriter& beginArray(uint64_t count);
        dataWriter& endArray()                      {return *this;}

        dataWriter& beginDict(uint64_t count);
        dataWriter& writeKey(std::string);
        dataWriter& writeKey(slice);
        dataWriter& endDict();

        dataWriter& operator<< (int64_t i)          {return writeInt(i);}
        dataWriter& operator<< (double d)          {return writeDouble(d);}
        dataWriter& operator<< (float f)          {return writeFloat(f);}
        dataWriter& operator<< (std::string str)        {return writeString(str);}
        dataWriter& operator<< (slice s)            {return writeString(s);}

    private:
        void addTypeCode(uint8_t code)              {_out.write((char*)&code, 1);}
        void addUVarint(uint64_t);

        std::ostream& _out;
        size_t _indexPos;
        std::vector<size_t> _savedIndexPos;
        std::unordered_map<std::string, size_t> _sharedStrings;
    };


}

#endif /* defined(__CBForest__DataWriter__) */
