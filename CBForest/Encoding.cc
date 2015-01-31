//
//  Encoding.cc
//  CBForest
//
//  Created by Jens Alfke on 1/26/15.
//  Copyright (c) 2015 Couchbase. All rights reserved.
//

#include "Encoding.hh"
#include "Endian.h"
#include "varint.hh"
extern "C" {
#include "murmurhash3_x86_32.h"
}


namespace forestdb {

    static uint8_t kValueTypes[] = {
        kNull,
        kBoolean, kBoolean,
        kNumber, kNumber, kNumber, kNumber, kNumber,
        kNumber, kNumber,
        kNumber,
        kDate,
        kString, kString, kString, kString,
        kData,
        kArray,
        kDict
    };

#pragma mark - VALUE:

    valueType value::type() const {
        return _typeCode < sizeof(kValueTypes) ? (valueType)kValueTypes[_typeCode] : kNull;
    }

    size_t value::getParam() const {
        uint64_t param;
        forestdb::GetUVarInt(slice(_paramStart, 99), &param);
        return param;
    }

    size_t value::getParam(const uint8_t* &after) const {
        uint64_t param;
        after = _paramStart + forestdb::GetUVarInt(slice(_paramStart, 99), &param);
        return param;
    }

    const value* value::next() const {
        const uint8_t* end = _paramStart;
        switch (_typeCode) {
            case kNullCode...kTrueCode:  return (const value*)(end + 0);
            case kInt8Code:              return (const value*)(end + 1);
            case kInt16Code:             return (const value*)(end + 2);
            case kInt32Code:
            case kFloat32Code:           return (const value*)(end + 4);
            case kInt64Code:
            case kUInt64Code:
            case kFloat64Code:           return (const value*)(end + 8);
            default: break;
        }

        uint64_t param = getParam(end);

        switch (_typeCode) {
            case kRawNumberCode:
            case kStringCode:
            case kSharedStringCode:
            case kDataCode:
                end += param;
                break;
            case kDateCode:
            case kSharedStringRefCode:
            case kExternStringRefCode:
                break;
            case kArrayCode: {
                // This is somewhat expensive: have to traverse all values in the array
                const value* v = (const value*)end;
                for (; param > 0; --param)
                    v = v->next();
                return v;
            }
            case kDictCode: {
                // This is somewhat expensive: have to traverse all keys+values in the dict
                size_t count;
                const value* key = ((const dict*)this)->firstKey(count);
                for (; count > 0; --count)
                    key = key->next()->next();
                return key;
            }
            default:
                throw "bad typecode";
        }
        return (const value*)end;
    }

    bool value::asBool() const {
        switch (_typeCode) {
            case kNullCode:
            case kFalseCode:
                return false;
                break;
            case kInt8Code...kRawNumberCode:
                return asInt() != 0;
            default:
                return true;
        }
    }

    int64_t value::asInt() const {
        switch (_typeCode) {
            case kNullCode:
            case kFalseCode:
                return 0;
            case kTrueCode:
                return 1;
            case kInt8Code:
                return *(int8_t*)_paramStart;
            case kInt16Code:
                return (int16_t) _dec16(*(int16_t*)_paramStart);
            case kInt32Code:
                return (int32_t) _dec32(*(int32_t*)_paramStart);
            case kInt64Code:
            case kUInt64Code:
                return (int64_t) _dec64(*(int64_t*)_paramStart);
            case kFloat32Code:
                return (int64_t) _decfloat(*(swappedFloat*)_paramStart);
            case kFloat64Code:
                return (int64_t) _decdouble(*(swappedDouble*)_paramStart);
            case kDateCode:
                return getParam();
            default:
                throw "value is not a number";
        }
    }

    double value::asDouble() const {
        switch (_typeCode) {
            case kFloat32Code:
                return _decfloat(*(swappedFloat*)_paramStart);
            case kFloat64Code:
                return _decdouble(*(swappedDouble*)_paramStart);
            default:
                return (double)asInt();
        }
    }

    std::time_t value::asDate() const {
        if (_typeCode != kDateCode)
            throw "value is not a date";
        return (std::time_t)getParam();
    }

    slice value::asString() const {
        const uint8_t* payload;
        uint64_t param = getParam(payload);
        switch (_typeCode) {
            case kStringCode:
            case kSharedStringCode:
            case kDataCode:
                return slice(payload, (size_t)param);
            case kSharedStringRefCode: {
                const value* str = (const value*)offsetby(this, -param);
                if (str->_typeCode != kSharedStringCode)
                    throw "invalid shared-string";
                param = str->getParam(payload);
                return slice(payload, (size_t)param);
            }
            case kExternStringRefCode:
                throw "can't dereference extern string without table";
            //TODO: Should be able to format dates (kDateCode) as strings
            default:
                throw "value is not a string";
        }
    }

    uint64_t value::stringToken() const {
        uint64_t param = getParam();
        switch (_typeCode) {
            case kSharedStringCode:
                return (uint64_t)this;
            case kSharedStringRefCode:
                return (uint64_t)this - param;
            case kExternStringRefCode:
                return param;
            default:
                throw "value is not shared or extern string";
        }
    }

    const array* value::asArray() const {
        if (_typeCode != kArrayCode)
            throw "value is not array";
        return (const array*)this;
    }

    const dict* value::asDict() const {
        if (_typeCode != kDictCode)
            throw "value is not dict";
        return (const dict*)this;
    }

#pragma mark - VALIDATION:

    const value* value::validate(slice s) {
        slice s2 = s;
        if (validate(s.buf, s2) && s2.size == 0)
            return (const value*)s.buf;
        else
            return NULL;
    }

    bool value::validate(const void *start, slice& s) {
        if (s.size < 1)
            return false;
        const void *valueStart = s.buf;
        typeCode type = *(const typeCode*)valueStart;
        s.moveStart(1);  // consume type-code

        if (type <= kFloat64Code) {
            size_t size;
            switch (type) {
                case kNullCode...kTrueCode:  return true;
                case kInt8Code:              size = 1; break;
                case kInt16Code:             size = 2; break;
                case kInt32Code:
                case kFloat32Code:           size = 4; break;
                case kInt64Code:
                case kUInt64Code:
                case kFloat64Code:           size = 8; break;
                default:                     return false;
            }
            return s.checkedMoveStart(size);
        }

        uint64_t param;
        if (!ReadUVarInt(&s, &param))
            return false;

        switch (type) {
            case kRawNumberCode:            //TODO: Check for purely numeric data
            case kStringCode:
            case kSharedStringCode:         //TODO: Check for valid UTF-8 data
            case kDataCode:
                if (!s.checkedMoveStart(param))
                    return false;
                return true;
            case kDateCode:                 //TODO: Check for valid date format
            case kExternStringRefCode:
                return true;
            case kSharedStringRefCode: {
                // Get pointer to original string:
                slice origString;
                origString.buf = offsetby(valueStart, -param);
                if (origString.buf < start || origString.buf >= s.buf)
                    return false;
                // Check that it's marked as a shared string:
                if (*(const typeCode*)origString.buf != kSharedStringCode)
                    return false;
                // Validate it:
                origString.setEnd(s.end());
                return validate(start, origString);
            }
            case kArrayCode: {
                for (; param > 0; --param)
                    if (!validate(start, s))
                        return false;
                return true;
            }
            case kDictCode: {
                // Skip hash codes:
                if (!s.checkedMoveStart(param*sizeof(uint16_t)))
                    return false;
                for (; param > 0; --param) {
                    auto v = (const value*)s.buf;
                    if (!validate(start, s) || (v->type() != kString))
                        return false;
                    if (!validate(start, s))
                        return false;
                }
                return true;
            }
            default:
                return false;
        }
    }

#pragma mark - ARRAY:

    const value* array::first() const {
        const uint8_t* f;
        getParam(f);
        return (const value*)f;
    }

#pragma mark - DICT:

    uint16_t dict::hashCode(slice s) {
        uint32_t result;
        MurmurHash3_x86_32(s.buf, (int)s.size, 0, &result);
        return result & 0xFFFF;
    }

    const value* dict::get(forestdb::slice keyToFind, uint16_t hashToFind) const {
        const uint8_t* after;
        size_t count = getParam(after);
        auto hashes = (const uint16_t*)after;

        size_t keyIndex = 0;
        const value* key = (const value*)&hashes[count];
        for (size_t i = 0; i < count; i++) {
            if (hashes[i] == hashToFind) {
                while (keyIndex < i) {
                    key = key->next()->next();
                    ++keyIndex;
                }
                if (keyToFind.compare(key->asString()) == 0)
                    return key->next();
            }
        }
        return NULL;
    }

    const value* dict::firstKey(size_t &count) const {
        const uint8_t* after;
        count = getParam(after);
        return (value*) offsetby(after, count * sizeof(uint16_t));
    }

    dict::iterator::iterator(const dict* d) {
        _key = d->firstKey(_count);
        _value = _count > 0 ? _key->next() : NULL;
    }

    dict::iterator& dict::iterator::operator++() {
        if (_count == 0)
            throw "iterating past end of dict";
        --_count;
        _key = _value->next();
        _value = _key->next();
        return *this;
    }

}
