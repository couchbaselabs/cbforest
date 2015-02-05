//
//  EncodingWriter.cc
//  CBForest
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#include "EncodingWriter.hh"
#include "Endian.h"
#include "varint.hh"


namespace forestdb {

    static size_t kMinSharedStringLength = 4, kMaxSharedStringLength = 100;

    dataWriter::dataWriter(Writer& out,
                           const std::unordered_map<std::string, uint32_t> *externStrings)
    :_out(out),
     _externStrings(externStrings)
    {
        pushState();
        _state->count = 0;
        _enableSharedStrings = false;
    }

    void dataWriter::addUVarint(uint64_t n) {
        char buf[kMaxVarintLen64];
        _out.write(buf, PutUVarInt(buf, n));
    }

    void dataWriter::writeNull() {
        addTypeCode(value::kNullCode);
    }

    void dataWriter::writeBool(bool b) {
        addTypeCode(b ? value::kTrueCode : value::kFalseCode);
    }

    void dataWriter::writeInt(int64_t i) {
        char buf[8];
        size_t size;
        int64_t swapped = _enc64(i);
        memcpy(&buf, &swapped, 8);
        value::typeCode code;
        if (i >= INT8_MIN && i <= INT8_MAX) {
            code = value::kInt8Code;
            size = 1;
        } else if (i >= INT16_MIN && i <= INT16_MAX) {
            code = value::kInt16Code;
            size = 2;
        } else if (i >= INT32_MIN && i <= INT32_MAX) {
            code = value::kInt32Code;
            size = 4;
        } else {
            code = value::kInt64Code;
            size = 8;
        }
        addTypeCode(code);
        _out.write(&buf[8-size], size);
    }

    void dataWriter::writeUInt(uint64_t u) {
        if (u < INT64_MAX)
            return writeInt((int64_t)u);
        u = _enc64(u);
        addTypeCode(value::kUInt64Code);
        _out.write((const char*)&u, 8);
    }

    void dataWriter::writeDouble(double n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int64_t)n)
            return writeInt((int64_t)n);
        swappedDouble swapped = _encdouble(n);
        addTypeCode(value::kFloat64Code);
        _out.write((const char*)&swapped, 8);
    }

    void dataWriter::writeFloat(float n) {
        if (isnan(n))
            throw "Can't write NaN";
        if (n == (int32_t)n)
            return writeInt((int32_t)n);
        swappedFloat swapped = _encfloat(n);
        addTypeCode(value::kFloat32Code);
        _out.write((const char*)&swapped, 4);
    }

    void dataWriter::writeRawNumber(slice s) {
        addTypeCode(value::kRawNumberCode);
        addUVarint(s.size);
        _out << s;
    }

    void dataWriter::writeDate(std::time_t dateTime) {
        addTypeCode(value::kDateCode);
        addUVarint(dateTime);
    }

    void dataWriter::writeData(slice s) {
        addTypeCode(value::kDataCode);
        addUVarint(s.size);
        _out << s;
    }

    void dataWriter::writeString(slice s) {
        if (_externStrings || (_enableSharedStrings && s.size >= kMinSharedStringLength
                                                    && s.size <= kMaxSharedStringLength)) {
            return writeString(std::string(s));
        } else {
            // not shareable so no need to convert to std::string
            addTypeCode(value::kStringCode);
            addUVarint(s.size);
            _out << s;
        }
    }

    void dataWriter::writeString(std::string str) {
        if (_externStrings) {
            auto externID = _externStrings->find(str);
            if (externID != _externStrings->end()) {
                // Write reference to extern string:
                addTypeCode(value::kExternStringRefCode);
                addUVarint(externID->second);
                return;
            }
        }

        size_t len = str.length();
        if (_enableSharedStrings && len >= kMinSharedStringLength
                                 && len <= kMaxSharedStringLength) {
            size_t curOffset = _out.length();
            if (curOffset > UINT32_MAX)
                throw "output too large";
            size_t sharedOffset = _sharedStrings[str];
            if (sharedOffset > 0) {
                // Change previous string opcode to shared:
                value::typeCode code = value::kSharedStringCode;
                _out.rewrite(sharedOffset, slice(&code,sizeof(value::typeCode)));

                // Write reference to previous string:
                addTypeCode(value::kSharedStringRefCode);
                addUVarint(curOffset - sharedOffset);
                return;
            }
            _sharedStrings[str] = (uint32_t)curOffset;
        }

        // First appearance, or unshareable, so write the string itself:
        addTypeCode(value::kStringCode);
        addUVarint(len);
        _out << str;
    }

    void dataWriter::pushState() {
        _states.push_back(state());
        _state = &_states[_states.size()-1];
        _state->hashes = NULL;
    }

    void dataWriter::popState() {
        if (_state->i != _state->count)
            throw "dataWriter: mismatched count";
        delete[] _state->hashes;
        _states.pop_back();
        _state = &_states[_states.size()-1];
    }

    void dataWriter::pushCount(uint32_t count) {
        addUVarint(count);
        pushState();
        _state->count = count;
        _state->i = 0;
    }

    void dataWriter::beginArray(uint32_t count) {
        addTypeCode(value::kArrayCode);
        pushCount(count);
    }

    void dataWriter::beginDict(uint32_t count) {
        addTypeCode(value::kDictCode);
        pushCount(count);
        // Write an empty/garbage hash list as a placeholder to fill in later:
        _state->hashes = new uint16_t[count];
        _state->indexPos = _out.length();
        _out.write((char*)_state->hashes, (std::streamsize)(_state->count*sizeof(uint16_t)));
    }

    void dataWriter::writeKey(std::string s) {
        // Go back and write the hash code to the index:
        _state->hashes[_state->i] = dict::hashCode(s);
        writeString(s);
        --_state->i; // the key didn't 'count' as a dict item
    }

    void dataWriter::writeKey(slice str) {
        return writeKey(std::string(str));
    }

    void dataWriter::endDict() {
        _out.rewrite(_state->indexPos, slice(_state->hashes, _state->count*sizeof(uint16_t)));
        popState();
    }

}
