//
//  Key.swift
//  SwiftForest
//
//  Created by Jens Alfke on 4/10/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

import Foundation


/** Error code for C4Error */
let InvalidKey: Int32 = 2000


/** Internal class representing a view index key in Collatable format */
class Key {

    init(handle: COpaquePointer) {
        self.handle = handle
    }

    init(obj: AnyObject?) throws {
        if obj != nil {
            handle = c4key_new()
            do {
                try add(obj!)
            } catch {
                c4key_free(handle)
                throw error
            }
        } else {
            handle = nil
        }
    }

    deinit {
        c4key_free(handle)
    }

    private func add(obj: AnyObject) throws {
        switch obj {
        case is NSNull:
            c4key_addNull(handle)
        case let b as Bool:
            c4key_addBool(handle, b)
            
        case let i as Int64:
            c4key_addNumber(handle, Double(i))
        case let d as Double:
            c4key_addNumber(handle, d)
        case let n as NSNumber:
            c4key_addNumber(handle, n.doubleValue)

        case let s as String:
            c4key_addString(handle, C4Slice(s))

        case let a as [AnyObject]:
            c4key_beginArray(handle)
            for item in a {
                try add(item)
            }
            c4key_endArray(handle)

        case let d as [String:AnyObject]:
            c4key_beginMap(handle)
            for (k, v) in d {
                c4key_addMapKey(handle, C4Slice(k))
                try add(v)
            }
            c4key_endMap(handle)
        case let d as NSDictionary:
            c4key_beginMap(handle)
            for (k, v) in d {
                guard let keyStr = k as? NSString else {
                    throw C4Error(domain: .C4Domain, code: InvalidKey)
                }
                c4key_addMapKey(handle, C4Slice(keyStr))
                try add(v)
            }
            c4key_endMap(handle)
        default:
            throw C4Error(domain: .C4Domain, code: Int32(InvalidKey))
        }
    }

    /** The internal C4Key* handle */
    let handle: COpaquePointer


    /** Reads key data from a KeyReader and parses it into Swift objects. */
    class func readFrom(reader: C4KeyReader) -> AnyObject? {
        guard reader.bytes != nil else { return nil }
        var r = reader
        return readFrom(&r)
    }

    private class func readFrom(inout reader: C4KeyReader) -> AnyObject {
        switch c4key_peek(&reader) {
        case .Null:     return NSNull()
        case .Bool:     return c4key_readBool(&reader)
        case .Number:   return c4key_readNumber(&reader)
        case .String:   return c4key_readString(&reader).asString()!
        case .Array:
            var result = [AnyObject]()
            c4key_skipToken(&reader)
            while c4key_peek(&reader) != .EndSequence {
                result.append(readFrom(&reader))
            }
            return result
        case .Map:
            var result = JSONDict()
            c4key_skipToken(&reader)
            while c4key_peek(&reader) != .EndSequence {
                let key = c4key_readString(&reader).asString()!
                result[key] = readFrom(&reader)
            }
            return result
        default:
            abort()
        }
    }
}