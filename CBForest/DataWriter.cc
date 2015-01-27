//
//  DataWriter.cc
//  CBForest
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#include "DataWriter.hh"
#include "varint.hh"


namespace forestdb {

    dataWriter::dataWriter(std::ostream& out)
    :_out(out)
    { }

    void dataWriter::addUVarint(uint64_t n) {
        char buf[kMaxVarintLen64];
        _out.write(buf, PutUVarInt(buf, n));
    }

    dataWriter& dataWriter::writeNull() {
        addTypeCode(value::kNullCode);
        return *this;
    }

    dataWriter& dataWriter::writeBool(bool b) {
        addTypeCode(b ? value::kTrueCode : value::kFalseCode);
        return *this;
    }

    dataWriter& dataWriter::writeInt(int64_t i) {
        char buf[9];
        size_t size;
        memcpy(&buf[1], &i, 8);         //FIX: Endian conversion
        if (i >= -0x80 && i < 0x80) {
            buf[0] = value::kInt8Code;
            size = 2;
        } else if (i >= -0x8000 && i < 0x8000) {
            buf[0] = value::kInt16Code;
            size = 3;
        } else if (i >= -0x80000000 && i < 0x80000000) {
            buf[0] = value::kInt32Code;
            size = 5;
        } else {
            buf[0] = value::kInt64Code;
            size = 9;
        }
        _out.write(buf, size);
        return *this;
    }

    dataWriter& dataWriter::writeDouble(double n) {
        addTypeCode(value::kFloat64Code);
        _out.write((const char*)&n, 8);         //FIX: Endian conversion
        return *this;
    }

    dataWriter& dataWriter::writeFloat(float n) {
        addTypeCode(value::kFloat32Code);
        _out.write((const char*)&n, 4);         //FIX: Endian conversion
        return *this;
    }

    dataWriter& dataWriter::writeString(slice s) {
        addTypeCode(value::kStringCode);
        addUVarint(s.size);
        _out.write((const char*)s.buf, s.size);
        return *this;
    }

    dataWriter& dataWriter::writeString(std::string str) {
        size_t offset = _sharedStrings[str];
        size_t curOffset = _out.tellp();
        if (offset > 0) {
            addTypeCode(value::kSharedStringCode);
            addUVarint(curOffset - offset);
        } else {
            size_t len = str.length();
            addTypeCode(value::kStringCode);
            addUVarint(len);
            _out << str;
            if (str.le)
        }
        return *this;
    }

    dataWriter& dataWriter::writeExternString(unsigned stringID) {
        addTypeCode(value::kExternStringCode);
        addUVarint(stringID);
        return *this;
    }


    dataWriter& dataWriter::beginArray(uint64_t count) {
        addTypeCode(value::kArrayCode);
        addUVarint(count);
        return *this;
    }

    dataWriter& dataWriter::beginDict(uint64_t count) {
        addTypeCode(value::kDictCode);
        addUVarint(count);
        // Write an empty hash list:
        _savedIndexPos.push_back(_indexPos);
        _indexPos = _out.tellp();
        uint16_t hash = 0;
        for (; count > 0; --count)
            _out.write((char*)&hash, sizeof(hash));
        return *this;
    }

    dataWriter& dataWriter::writeKey(slice s) {
        // Go back and write the hash code to the index:
        uint16_t hashCode = dict::hashCode(s);
        uint64_t pos = _out.tellp();
        _out.seekp(_indexPos);
        _out.write((char*)&hashCode, 2);
        _indexPos += 2;
        _out.seekp(pos);

        writeString(s);
        return *this;
    }

    dataWriter& dataWriter::writeKey(std::string str) {
        return writeKey(slice(str));
    }

    dataWriter& dataWriter::endDict() {
        _indexPos = _savedIndexPos.back();
        _savedIndexPos.pop_back();
        return *this;
    }

}
