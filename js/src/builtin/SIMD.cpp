/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS SIMD pseudo-module.
 * Specification matches polyfill:
 * https://github.com/johnmccutchan/ecmascript_simd/blob/master/src/ecmascript_simd.js
 * The objects float32x4 and int32x4 are installed on the SIMD pseudo-module.
 */

#include "builtin/SIMD.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "builtin/TypedObject.h"
#include "jsobjinlines.h"

using namespace js;

using mozilla::ArrayLength;
using mozilla::IsFinite;
using mozilla::IsNaN;

namespace js {
extern const JSFunctionSpec Float32x4Methods[];
extern const JSFunctionSpec Int32x4Methods[];
}

///////////////////////////////////////////////////////////////////////////
// X4

#define LANE_ACCESSOR(Type32x4, lane) \
    bool Type32x4##Lane##lane(JSContext *cx, unsigned argc, Value *vp) { \
        static const char *laneNames[] = {"lane 0", "lane 1", "lane 2", "lane3"}; \
        CallArgs args = CallArgsFromVp(argc, vp); \
        if(!args.thisv().isObject() || !IsTypedDatum(args.thisv().toObject())) { \
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO, \
                                 X4Type::class_.name, laneNames[lane], \
                                 InformalValueTypeName(args.thisv())); \
            return false; \
        } \
        TypedDatum &datum = AsTypedDatum(args.thisv().toObject()); \
        TypeRepresentation *typeRepr = datum.datumTypeRepresentation(); \
        if (typeRepr->kind() != TypeRepresentation::X4 || typeRepr->asX4()->type() != Type32x4::type) { \
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO, \
                                 X4Type::class_.name, laneNames[lane], \
                                 InformalValueTypeName(args.thisv())); \
            return false; \
        } \
        Type32x4::Elem *data = reinterpret_cast<Type32x4::Elem *>(datum.typedMem()); \
        Type32x4::setReturn(args, data[lane]); \
        return true; \
    }
    LANE_ACCESSOR(Float32x4, 0);
    LANE_ACCESSOR(Float32x4, 1);
    LANE_ACCESSOR(Float32x4, 2);
    LANE_ACCESSOR(Float32x4, 3);
    LANE_ACCESSOR(Int32x4, 0);
    LANE_ACCESSOR(Int32x4, 1);
    LANE_ACCESSOR(Int32x4, 2);
    LANE_ACCESSOR(Int32x4, 3);
#undef LANE_ACCESSOR

#define SIGN_MASK(Type32x4) \
    bool Type32x4##SignMask(JSContext *cx, unsigned argc, Value *vp) { \
        CallArgs args = CallArgsFromVp(argc, vp); \
        if(!args.thisv().isObject() || !IsTypedDatum(args.thisv().toObject())) { \
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO, \
                                 X4Type::class_.name, "signMask", \
                                 InformalValueTypeName(args.thisv())); \
            return false; \
        } \
        TypedDatum &datum = AsTypedDatum(args.thisv().toObject()); \
        TypeRepresentation *typeRepr = datum.datumTypeRepresentation(); \
        if (typeRepr->kind() != TypeRepresentation::X4 || typeRepr->asX4()->type() != Type32x4::type) { \
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO, \
                                 X4Type::class_.name, "signMask", \
                                 InformalValueTypeName(args.thisv())); \
            return false; \
        } \
        Type32x4::Elem *data = reinterpret_cast<Type32x4::Elem *>(datum.typedMem()); \
        int32_t mx = data[0] < 0.0 ? 1 : 0; \
        int32_t my = data[1] < 0.0 ? 1 : 0; \
        int32_t mz = data[2] < 0.0 ? 1 : 0; \
        int32_t mw = data[3] < 0.0 ? 1 : 0; \
        int32_t result = mx | my << 1 | mz << 2 | mw << 3; \
        args.rval().setInt32(result); \
        return true; \
    }
    SIGN_MASK(Float32x4);
    SIGN_MASK(Int32x4);
#undef SIGN_MASK

const Class X4Type::class_ = {
    "X4",
    JSCLASS_HAS_RESERVED_SLOTS(JS_TYPEOBJ_X4_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,             /* finalize    */
    nullptr,             /* checkAccess */
    call,                /* call        */
    nullptr,             /* hasInstance */
    nullptr,             /* construct   */
    nullptr
};

// These classes just exist to group together various properties and so on.
namespace js {
class Int32x4Defn {
  public:
    static const X4TypeRepresentation::Type type = X4TypeRepresentation::TYPE_INT32;
    static const JSFunctionSpec TypeDescriptorMethods[];
    static const JSPropertySpec TypeObjectProperties[];
    static const JSFunctionSpec TypeObjectMethods[];
};
class Float32x4Defn {
  public:
    static const X4TypeRepresentation::Type type = X4TypeRepresentation::TYPE_FLOAT32;
    static const JSFunctionSpec TypeDescriptorMethods[];
    static const JSPropertySpec TypeObjectProperties[];
    static const JSFunctionSpec TypeObjectMethods[];
};
} // namespace js

const JSFunctionSpec js::Float32x4Defn::TypeDescriptorMethods[] = {
    JS_FN("toSource", TypeObjectToSource, 0, 0),
    JS_SELF_HOSTED_FN("handle", "HandleCreate", 2, 0),
    JS_SELF_HOSTED_FN("array", "ArrayShorthand", 1, 0),
    JS_SELF_HOSTED_FN("equivalent", "TypeObjectEquivalent", 1, 0),
    JS_FS_END
};

const JSPropertySpec js::Float32x4Defn::TypeObjectProperties[] = {
    JS_PSG("x", Float32x4Lane0, JSPROP_PERMANENT),
    JS_PSG("y", Float32x4Lane1, JSPROP_PERMANENT),
    JS_PSG("z", Float32x4Lane2, JSPROP_PERMANENT),
    JS_PSG("w", Float32x4Lane3, JSPROP_PERMANENT),
    JS_PSG("signMask", Float32x4SignMask, JSPROP_PERMANENT),
    JS_PS_END
};

const JSFunctionSpec js::Float32x4Defn::TypeObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "X4ToSource", 0, 0),
    JS_FS_END
};

const JSFunctionSpec js::Int32x4Defn::TypeDescriptorMethods[] = {
    JS_FN("toSource", TypeObjectToSource, 0, 0),
    JS_SELF_HOSTED_FN("handle", "HandleCreate", 2, 0),
    JS_SELF_HOSTED_FN("array", "ArrayShorthand", 1, 0),
    JS_SELF_HOSTED_FN("equivalent", "TypeObjectEquivalent", 1, 0),
    JS_FS_END,
};

const JSPropertySpec js::Int32x4Defn::TypeObjectProperties[] = {
    JS_PSG("x", Int32x4Lane0, JSPROP_PERMANENT),
    JS_PSG("y", Int32x4Lane1, JSPROP_PERMANENT),
    JS_PSG("z", Int32x4Lane2, JSPROP_PERMANENT),
    JS_PSG("w", Int32x4Lane3, JSPROP_PERMANENT),
    JS_PSG("signMask", Int32x4SignMask, JSPROP_PERMANENT),
    JS_PS_END
};

const JSFunctionSpec js::Int32x4Defn::TypeObjectMethods[] = {
    JS_SELF_HOSTED_FN("toSource", "X4ToSource", 0, 0),
    JS_FS_END
};

template<typename T>
static JSObject *
CreateX4Class(JSContext *cx, Handle<GlobalObject*> global)
{
    RootedObject funcProto(cx, global->getOrCreateFunctionPrototype(cx));
    if (!funcProto)
        return nullptr;

    // Create type representation.

    RootedObject typeReprObj(cx);
    typeReprObj = X4TypeRepresentation::Create(cx, T::type);
    if (!typeReprObj)
        return nullptr;

    // Create prototype property, which inherits from Object.prototype.

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;
    RootedObject proto(cx);
    proto = NewObjectWithGivenProto(cx, &JSObject::class_, objProto, global, SingletonObject);
    if (!proto)
        return nullptr;

    // Create type constructor itself.

    RootedObject x4(cx);
    x4 = NewObjectWithClassProto(cx, &X4Type::class_, funcProto, global);
    if (!x4 ||
        !InitializeCommonTypeDescriptorProperties(cx, x4, typeReprObj) ||
        !DefinePropertiesAndBrand(cx, proto, nullptr, nullptr))
    {
        return nullptr;
    }

    // Link type constructor to the type representation.

    x4->initReservedSlot(JS_TYPEOBJ_SLOT_TYPE_REPR, ObjectValue(*typeReprObj));

    // Link constructor to prototype and install properties.

    if (!JS_DefineFunctions(cx, x4, T::TypeDescriptorMethods))
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, x4, proto) ||
        !DefinePropertiesAndBrand(cx, proto, T::TypeObjectProperties,
                                  T::TypeObjectMethods))
    {
        return nullptr;
    }

    return x4;
}

bool
X4Type::call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    const uint32_t LANES = 4;

    if (args.length() < LANES) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             args.callee().getClass()->name, "3", "s");
        return false;
    }

    double values[LANES];
    for (uint32_t i = 0; i < LANES; i++) {
        if (!ToNumber(cx, args[i], &values[i]))
            return false;
    }

    RootedObject typeObj(cx, &args.callee());
    Rooted<TypedObject*> result(cx, TypedObject::createZeroed(cx, typeObj, 0));
    if (!result)
        return false;

    X4TypeRepresentation *typeRepr = typeRepresentation(*typeObj)->asX4();
    switch (typeRepr->type()) {
#define STORE_LANES(_constant, _type, _name)                                  \
      case _constant:                                                         \
      {                                                                       \
        _type *mem = reinterpret_cast<_type*>(result->typedMem());            \
        for (uint32_t i = 0; i < LANES; i++) {                                \
            mem[i] = ConvertScalar<_type>(values[i]);                         \
        }                                                                     \
        break;                                                                \
      }
      JS_FOR_EACH_X4_TYPE_REPR(STORE_LANES)
#undef STORE_LANES
    }
    args.rval().setObject(*result);
    return true;
}

///////////////////////////////////////////////////////////////////////////
// SIMD class

static const JSConstInt32Spec simd_constants[] = {
    {0x00, "XXXX", 0, {0,0,0}},
    {0x40, "XXXY", 0, {0,0,0}},
    {0x80, "XXXZ", 0, {0,0,0}},
    {0xC0, "XXXW", 0, {0,0,0}},
    {0x10, "XXYX", 0, {0,0,0}},
    {0x50, "XXYY", 0, {0,0,0}},
    {0x90, "XXYZ", 0, {0,0,0}},
    {0xD0, "XXYW", 0, {0,0,0}},
    {0x20, "XXZX", 0, {0,0,0}},
    {0x60, "XXZY", 0, {0,0,0}},
    {0xA0, "XXZZ", 0, {0,0,0}},
    {0xE0, "XXZW", 0, {0,0,0}},
    {0x30, "XXWX", 0, {0,0,0}},
    {0x70, "XXWY", 0, {0,0,0}},
    {0xB0, "XXWZ", 0, {0,0,0}},
    {0xF0, "XXWW", 0, {0,0,0}},
    {0x04, "XYXX", 0, {0,0,0}},
    {0x44, "XYXY", 0, {0,0,0}},
    {0x84, "XYXZ", 0, {0,0,0}},
    {0xC4, "XYXW", 0, {0,0,0}},
    {0x14, "XYYX", 0, {0,0,0}},
    {0x54, "XYYY", 0, {0,0,0}},
    {0x94, "XYYZ", 0, {0,0,0}},
    {0xD4, "XYYW", 0, {0,0,0}},
    {0x24, "XYZX", 0, {0,0,0}},
    {0x64, "XYZY", 0, {0,0,0}},
    {0xA4, "XYZZ", 0, {0,0,0}},
    {0xE4, "XYZW", 0, {0,0,0}},
    {0x34, "XYWX", 0, {0,0,0}},
    {0x74, "XYWY", 0, {0,0,0}},
    {0xB4, "XYWZ", 0, {0,0,0}},
    {0xF4, "XYWW", 0, {0,0,0}},
    {0x08, "XZXX", 0, {0,0,0}},
    {0x48, "XZXY", 0, {0,0,0}},
    {0x88, "XZXZ", 0, {0,0,0}},
    {0xC8, "XZXW", 0, {0,0,0}},
    {0x18, "XZYX", 0, {0,0,0}},
    {0x58, "XZYY", 0, {0,0,0}},
    {0x98, "XZYZ", 0, {0,0,0}},
    {0xD8, "XZYW", 0, {0,0,0}},
    {0x28, "XZZX", 0, {0,0,0}},
    {0x68, "XZZY", 0, {0,0,0}},
    {0xA8, "XZZZ", 0, {0,0,0}},
    {0xE8, "XZZW", 0, {0,0,0}},
    {0x38, "XZWX", 0, {0,0,0}},
    {0x78, "XZWY", 0, {0,0,0}},
    {0xB8, "XZWZ", 0, {0,0,0}},
    {0xF8, "XZWW", 0, {0,0,0}},
    {0x0C, "XWXX", 0, {0,0,0}},
    {0x4C, "XWXY", 0, {0,0,0}},
    {0x8C, "XWXZ", 0, {0,0,0}},
    {0xCC, "XWXW", 0, {0,0,0}},
    {0x1C, "XWYX", 0, {0,0,0}},
    {0x5C, "XWYY", 0, {0,0,0}},
    {0x9C, "XWYZ", 0, {0,0,0}},
    {0xDC, "XWYW", 0, {0,0,0}},
    {0x2C, "XWZX", 0, {0,0,0}},
    {0x6C, "XWZY", 0, {0,0,0}},
    {0xAC, "XWZZ", 0, {0,0,0}},
    {0xEC, "XWZW", 0, {0,0,0}},
    {0x3C, "XWWX", 0, {0,0,0}},
    {0x7C, "XWWY", 0, {0,0,0}},
    {0xBC, "XWWZ", 0, {0,0,0}},
    {0xFC, "XWWW", 0, {0,0,0}},
    {0x01, "YXXX", 0, {0,0,0}},
    {0x41, "YXXY", 0, {0,0,0}},
    {0x81, "YXXZ", 0, {0,0,0}},
    {0xC1, "YXXW", 0, {0,0,0}},
    {0x11, "YXYX", 0, {0,0,0}},
    {0x51, "YXYY", 0, {0,0,0}},
    {0x91, "YXYZ", 0, {0,0,0}},
    {0xD1, "YXYW", 0, {0,0,0}},
    {0x21, "YXZX", 0, {0,0,0}},
    {0x61, "YXZY", 0, {0,0,0}},
    {0xA1, "YXZZ", 0, {0,0,0}},
    {0xE1, "YXZW", 0, {0,0,0}},
    {0x31, "YXWX", 0, {0,0,0}},
    {0x71, "YXWY", 0, {0,0,0}},
    {0xB1, "YXWZ", 0, {0,0,0}},
    {0xF1, "YXWW", 0, {0,0,0}},
    {0x05, "YYXX", 0, {0,0,0}},
    {0x45, "YYXY", 0, {0,0,0}},
    {0x85, "YYXZ", 0, {0,0,0}},
    {0xC5, "YYXW", 0, {0,0,0}},
    {0x15, "YYYX", 0, {0,0,0}},
    {0x55, "YYYY", 0, {0,0,0}},
    {0x95, "YYYZ", 0, {0,0,0}},
    {0xD5, "YYYW", 0, {0,0,0}},
    {0x25, "YYZX", 0, {0,0,0}},
    {0x65, "YYZY", 0, {0,0,0}},
    {0xA5, "YYZZ", 0, {0,0,0}},
    {0xE5, "YYZW", 0, {0,0,0}},
    {0x35, "YYWX", 0, {0,0,0}},
    {0x75, "YYWY", 0, {0,0,0}},
    {0xB5, "YYWZ", 0, {0,0,0}},
    {0xF5, "YYWW", 0, {0,0,0}},
    {0x09, "YZXX", 0, {0,0,0}},
    {0x49, "YZXY", 0, {0,0,0}},
    {0x89, "YZXZ", 0, {0,0,0}},
    {0xC9, "YZXW", 0, {0,0,0}},
    {0x19, "YZYX", 0, {0,0,0}},
    {0x59, "YZYY", 0, {0,0,0}},
    {0x99, "YZYZ", 0, {0,0,0}},
    {0xD9, "YZYW", 0, {0,0,0}},
    {0x29, "YZZX", 0, {0,0,0}},
    {0x69, "YZZY", 0, {0,0,0}},
    {0xA9, "YZZZ", 0, {0,0,0}},
    {0xE9, "YZZW", 0, {0,0,0}},
    {0x39, "YZWX", 0, {0,0,0}},
    {0x79, "YZWY", 0, {0,0,0}},
    {0xB9, "YZWZ", 0, {0,0,0}},
    {0xF9, "YZWW", 0, {0,0,0}},
    {0x0D, "YWXX", 0, {0,0,0}},
    {0x4D, "YWXY", 0, {0,0,0}},
    {0x8D, "YWXZ", 0, {0,0,0}},
    {0xCD, "YWXW", 0, {0,0,0}},
    {0x1D, "YWYX", 0, {0,0,0}},
    {0x5D, "YWYY", 0, {0,0,0}},
    {0x9D, "YWYZ", 0, {0,0,0}},
    {0xDD, "YWYW", 0, {0,0,0}},
    {0x2D, "YWZX", 0, {0,0,0}},
    {0x6D, "YWZY", 0, {0,0,0}},
    {0xAD, "YWZZ", 0, {0,0,0}},
    {0xED, "YWZW", 0, {0,0,0}},
    {0x3D, "YWWX", 0, {0,0,0}},
    {0x7D, "YWWY", 0, {0,0,0}},
    {0xBD, "YWWZ", 0, {0,0,0}},
    {0xFD, "YWWW", 0, {0,0,0}},
    {0x02, "ZXXX", 0, {0,0,0}},
    {0x42, "ZXXY", 0, {0,0,0}},
    {0x82, "ZXXZ", 0, {0,0,0}},
    {0xC2, "ZXXW", 0, {0,0,0}},
    {0x12, "ZXYX", 0, {0,0,0}},
    {0x52, "ZXYY", 0, {0,0,0}},
    {0x92, "ZXYZ", 0, {0,0,0}},
    {0xD2, "ZXYW", 0, {0,0,0}},
    {0x22, "ZXZX", 0, {0,0,0}},
    {0x62, "ZXZY", 0, {0,0,0}},
    {0xA2, "ZXZZ", 0, {0,0,0}},
    {0xE2, "ZXZW", 0, {0,0,0}},
    {0x32, "ZXWX", 0, {0,0,0}},
    {0x72, "ZXWY", 0, {0,0,0}},
    {0xB2, "ZXWZ", 0, {0,0,0}},
    {0xF2, "ZXWW", 0, {0,0,0}},
    {0x06, "ZYXX", 0, {0,0,0}},
    {0x46, "ZYXY", 0, {0,0,0}},
    {0x86, "ZYXZ", 0, {0,0,0}},
    {0xC6, "ZYXW", 0, {0,0,0}},
    {0x16, "ZYYX", 0, {0,0,0}},
    {0x56, "ZYYY", 0, {0,0,0}},
    {0x96, "ZYYZ", 0, {0,0,0}},
    {0xD6, "ZYYW", 0, {0,0,0}},
    {0x26, "ZYZX", 0, {0,0,0}},
    {0x66, "ZYZY", 0, {0,0,0}},
    {0xA6, "ZYZZ", 0, {0,0,0}},
    {0xE6, "ZYZW", 0, {0,0,0}},
    {0x36, "ZYWX", 0, {0,0,0}},
    {0x76, "ZYWY", 0, {0,0,0}},
    {0xB6, "ZYWZ", 0, {0,0,0}},
    {0xF6, "ZYWW", 0, {0,0,0}},
    {0x0A, "ZZXX", 0, {0,0,0}},
    {0x4A, "ZZXY", 0, {0,0,0}},
    {0x8A, "ZZXZ", 0, {0,0,0}},
    {0xCA, "ZZXW", 0, {0,0,0}},
    {0x1A, "ZZYX", 0, {0,0,0}},
    {0x5A, "ZZYY", 0, {0,0,0}},
    {0x9A, "ZZYZ", 0, {0,0,0}},
    {0xDA, "ZZYW", 0, {0,0,0}},
    {0x2A, "ZZZX", 0, {0,0,0}},
    {0x6A, "ZZZY", 0, {0,0,0}},
    {0xAA, "ZZZZ", 0, {0,0,0}},
    {0xEA, "ZZZW", 0, {0,0,0}},
    {0x3A, "ZZWX", 0, {0,0,0}},
    {0x7A, "ZZWY", 0, {0,0,0}},
    {0xBA, "ZZWZ", 0, {0,0,0}},
    {0xFA, "ZZWW", 0, {0,0,0}},
    {0x0E, "ZWXX", 0, {0,0,0}},
    {0x4E, "ZWXY", 0, {0,0,0}},
    {0x8E, "ZWXZ", 0, {0,0,0}},
    {0xCE, "ZWXW", 0, {0,0,0}},
    {0x1E, "ZWYX", 0, {0,0,0}},
    {0x5E, "ZWYY", 0, {0,0,0}},
    {0x9E, "ZWYZ", 0, {0,0,0}},
    {0xDE, "ZWYW", 0, {0,0,0}},
    {0x2E, "ZWZX", 0, {0,0,0}},
    {0x6E, "ZWZY", 0, {0,0,0}},
    {0xAE, "ZWZZ", 0, {0,0,0}},
    {0xEE, "ZWZW", 0, {0,0,0}},
    {0x3E, "ZWWX", 0, {0,0,0}},
    {0x7E, "ZWWY", 0, {0,0,0}},
    {0xBE, "ZWWZ", 0, {0,0,0}},
    {0xFE, "ZWWW", 0, {0,0,0}},
    {0x03, "WXXX", 0, {0,0,0}},
    {0x43, "WXXY", 0, {0,0,0}},
    {0x83, "WXXZ", 0, {0,0,0}},
    {0xC3, "WXXW", 0, {0,0,0}},
    {0x13, "WXYX", 0, {0,0,0}},
    {0x53, "WXYY", 0, {0,0,0}},
    {0x93, "WXYZ", 0, {0,0,0}},
    {0xD3, "WXYW", 0, {0,0,0}},
    {0x23, "WXZX", 0, {0,0,0}},
    {0x63, "WXZY", 0, {0,0,0}},
    {0xA3, "WXZZ", 0, {0,0,0}},
    {0xE3, "WXZW", 0, {0,0,0}},
    {0x33, "WXWX", 0, {0,0,0}},
    {0x73, "WXWY", 0, {0,0,0}},
    {0xB3, "WXWZ", 0, {0,0,0}},
    {0xF3, "WXWW", 0, {0,0,0}},
    {0x07, "WYXX", 0, {0,0,0}},
    {0x47, "WYXY", 0, {0,0,0}},
    {0x87, "WYXZ", 0, {0,0,0}},
    {0xC7, "WYXW", 0, {0,0,0}},
    {0x17, "WYYX", 0, {0,0,0}},
    {0x57, "WYYY", 0, {0,0,0}},
    {0x97, "WYYZ", 0, {0,0,0}},
    {0xD7, "WYYW", 0, {0,0,0}},
    {0x27, "WYZX", 0, {0,0,0}},
    {0x67, "WYZY", 0, {0,0,0}},
    {0xA7, "WYZZ", 0, {0,0,0}},
    {0xE7, "WYZW", 0, {0,0,0}},
    {0x37, "WYWX", 0, {0,0,0}},
    {0x77, "WYWY", 0, {0,0,0}},
    {0xB7, "WYWZ", 0, {0,0,0}},
    {0xF7, "WYWW", 0, {0,0,0}},
    {0x0B, "WZXX", 0, {0,0,0}},
    {0x4B, "WZXY", 0, {0,0,0}},
    {0x8B, "WZXZ", 0, {0,0,0}},
    {0xCB, "WZXW", 0, {0,0,0}},
    {0x1B, "WZYX", 0, {0,0,0}},
    {0x5B, "WZYY", 0, {0,0,0}},
    {0x9B, "WZYZ", 0, {0,0,0}},
    {0xDB, "WZYW", 0, {0,0,0}},
    {0x2B, "WZZX", 0, {0,0,0}},
    {0x6B, "WZZY", 0, {0,0,0}},
    {0xAB, "WZZZ", 0, {0,0,0}},
    {0xEB, "WZZW", 0, {0,0,0}},
    {0x3B, "WZWX", 0, {0,0,0}},
    {0x7B, "WZWY", 0, {0,0,0}},
    {0xBB, "WZWZ", 0, {0,0,0}},
    {0xFB, "WZWW", 0, {0,0,0}},
    {0x0F, "WWXX", 0, {0,0,0}},
    {0x4F, "WWXY", 0, {0,0,0}},
    {0x8F, "WWXZ", 0, {0,0,0}},
    {0xCF, "WWXW", 0, {0,0,0}},
    {0x1F, "WWYX", 0, {0,0,0}},
    {0x5F, "WWYY", 0, {0,0,0}},
    {0x9F, "WWYZ", 0, {0,0,0}},
    {0xDF, "WWYW", 0, {0,0,0}},
    {0x2F, "WWZX", 0, {0,0,0}},
    {0x6F, "WWZY", 0, {0,0,0}},
    {0xAF, "WWZZ", 0, {0,0,0}},
    {0xEF, "WWZW", 0, {0,0,0}},
    {0x3F, "WWWX", 0, {0,0,0}},
    {0x7F, "WWWY", 0, {0,0,0}},
    {0xBF, "WWWZ", 0, {0,0,0}},
    {0xFF, "WWWW", 0, {0,0,0}},
    {0, 0, 0,{0,0,0}}
};

const Class SIMDObject::class_ = {
        "SIMD",
        JSCLASS_HAS_CACHED_PROTO(JSProto_SIMD),
        JS_PropertyStub,         /* addProperty */
        JS_DeletePropertyStub,   /* delProperty */
        JS_PropertyStub,         /* getProperty */
        JS_StrictPropertyStub,   /* setProperty */
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub,
        nullptr,             /* finalize    */
        nullptr,             /* checkAccess */
        nullptr,             /* call        */
        nullptr,             /* hasInstance */
        nullptr,             /* construct   */
        nullptr
};

JSObject *
SIMDObject::initClass(JSContext *cx, Handle<GlobalObject *> global)
{
    // Create SIMD Object.
    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if(!objProto)
        return nullptr;
    RootedObject SIMD(cx, NewObjectWithGivenProto(cx, &SIMDObject::class_, objProto,
                                                  global, SingletonObject));
    if (!SIMD)
        return nullptr;

    // float32x4

    RootedObject float32x4Object(cx, CreateX4Class<Float32x4Defn>(cx, global));
    if (!float32x4Object)
        return nullptr;

    // Define float32x4 functions and install as a property of the SIMD object.
    global->setFloat32x4TypeObject(*float32x4Object);
    RootedValue float32x4Value(cx, ObjectValue(*float32x4Object));
    if (!JS_DefineFunctions(cx, float32x4Object, Float32x4Methods) ||
        !JSObject::defineProperty(cx, SIMD, cx->names().float32x4,
                                  float32x4Value, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    // int32x4

    RootedObject int32x4Object(cx, CreateX4Class<Int32x4Defn>(cx, global));
    if (!int32x4Object)
        return nullptr;

    // Define int32x4 functions and install as a property of the SIMD object.
    global->setInt32x4TypeObject(*int32x4Object);
    RootedValue int32x4Value(cx, ObjectValue(*int32x4Object));
    if (!JS_DefineFunctions(cx, int32x4Object, Int32x4Methods) ||
        !JS_DefineConstInt32s(cx, SIMD, simd_constants) ||
        !JSObject::defineProperty(cx, SIMD, cx->names().int32x4,
                                  int32x4Value, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_PERMANENT))
    {
        return nullptr;
    }

    RootedValue SIMDValue(cx, ObjectValue(*SIMD));
    global->setConstructor(JSProto_SIMD, SIMDValue);

    // Everything is set up, install SIMD on the global object.
    if (!JSObject::defineProperty(cx, global, cx->names().SIMD,  SIMDValue, nullptr, nullptr, 0)) {
        return nullptr;
    }

    return SIMD;
}

JSObject *
js_InitSIMDClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());
    return SIMDObject::initClass(cx, global);
}

template<typename V>
static bool
ObjectIsVector(JSObject &obj) {
    if (!IsTypedDatum(obj))
        return false;
    TypeRepresentation *typeRepr = AsTypedDatum(obj).datumTypeRepresentation();
    if (typeRepr->kind() != TypeRepresentation::X4)
        return false;
    return typeRepr->asX4()->type() == V::type;
}

template<typename V>
TypedObject *
js::CreateZeroedSIMDWrapper(JSContext *cx)
{
    RootedObject typeObj(cx, &V::GetTypeObject(*cx->global()));
    JS_ASSERT(typeObj);

    return TypedObject::createZeroed(cx, typeObj, 0);
}

template<typename V>
JSObject *
js::Create(JSContext *cx, typename V::Elem *data)
{
    Rooted<TypedObject *> result(cx, CreateZeroedSIMDWrapper<V>(cx));
    if (!result)
        return nullptr;

    typename V::Elem *resultMem = reinterpret_cast<typename V::Elem *>(result->typedMem());
    memcpy(resultMem, data, sizeof(typename V::Elem) * V::lanes);
    return result;
}

namespace js {
template TypedObject *CreateZeroedSIMDWrapper<Float32x4>(JSContext *cx);
template TypedObject *CreateZeroedSIMDWrapper<Int32x4>(JSContext *cx);
template JSObject *Create<Float32x4>(JSContext *cx, Float32x4::Elem *data);
template JSObject *Create<Int32x4>(JSContext *cx, Int32x4::Elem *data);
}

namespace js {
template<typename T, typename V>
struct Abs {
    static inline T apply(T x, T zero) {return V::toType(x < 0 ? -1 * x : x);}
};
template<typename T, typename V>
struct Neg {
    static inline T apply(T x, T zero) {return V::toType(-1 * x);}
};
template<typename T, typename V>
struct Not {
    static inline T apply(T x, T zero) {return V::toType(~x);}
};
template<typename T, typename V>
struct Rec {
    static inline T apply(T x, T zero) {return V::toType(1 / x);}
};
template<typename T, typename V>
struct RecSqrt {
    static inline T apply(T x, T zero) {return V::toType(1 / sqrt(x));}
};
template<typename T, typename V>
struct Sqrt {
    static inline T apply(T x, T zero) {return V::toType(sqrt(x));}
};
template<typename T, typename V>
struct Add {
    static inline T apply(T l, T r) {return V::toType(l + r);}
};
template<typename T, typename V>
struct Sub {
    static inline T apply(T l, T r) {return V::toType(l - r);}
};
template<typename T, typename V>
struct Div {
    static inline T apply(T l, T r) {return V::toType(l / r);}
};
template<typename T, typename V>
struct Mul {
    static inline T apply(T l, T r) {return V::toType(l * r);}
};
template<typename T, typename V>
struct Minimum {
    static inline T apply(T l, T r) {return V::toType(l < r ? l : r);}
};
template<typename T, typename V>
struct Maximum {
    static inline T apply(T l, T r) {return V::toType(l > r ? l : r);}
};
template<typename T, typename V>
struct LessThan {
    static inline T apply(T l, T r) {return V::toType(l < r ? 0xFFFFFFFF: 0x0);}
};
template<typename T, typename V>
struct LessThanOrEqual {
    static inline T apply(T l, T r) {return V::toType(l <= r ? 0xFFFFFFFF: 0x0);}
};
template<typename T, typename V>
struct GreaterThan {
    static inline T apply(T l, T r) {return V::toType(l > r ? 0xFFFFFFFF: 0x0);}
};
template<typename T, typename V>
struct GreaterThanOrEqual {
    static inline T apply(T l, T r) {return V::toType(l >= r ? 0xFFFFFFFF: 0x0);}
};
template<typename T, typename V>
struct Equal {
    static inline T apply(T l, T r) {return V::toType(l == r ? 0xFFFFFFFF: 0x0);}
};
template<typename T, typename V>
struct NotEqual {
    static inline T apply(T l, T r) {return V::toType(l != r ? 0xFFFFFFFF: 0x0);}
};
template<typename T, typename V>
struct Xor {
    static inline T apply(T l, T r) {return V::toType(l ^ r);}
};
template<typename T, typename V>
struct And {
    static inline T apply(T l, T r) {return V::toType(l & r);}
};
template<typename T, typename V>
struct Or {
    static inline T apply(T l, T r) {return V::toType(l | r);}
};
template<typename T, typename V>
struct Scale {
    static inline T apply(int32_t lane, T scalar, T x) {return V::toType(scalar * x);}
};
template<typename T, typename V>
struct WithX {
    static inline T apply(int32_t lane, T scalar, T x) {return V::toType(lane == 0 ? scalar : x);}
};
template<typename T, typename V>
struct WithY {
    static inline T apply(int32_t lane, T scalar, T x) {return V::toType(lane == 1 ? scalar : x);}
};
template<typename T, typename V>
struct WithZ {
    static inline T apply(int32_t lane, T scalar, T x) {return V::toType(lane == 2 ? scalar : x);}
};
template<typename T, typename V>
struct WithW {
    static inline T apply(int32_t lane, T scalar, T x) {return V::toType(lane == 3 ? scalar : x);}
};
template<typename T, typename V>
struct WithFlagX {
    static inline T apply(T l, T f, T x) { return V::toType(l == 0 ? (f ? 0xFFFFFFFF : 0x0) : x);}
};
template<typename T, typename V>
struct WithFlagY {
    static inline T apply(T l, T f, T x) { return V::toType(l == 1 ? (f ? 0xFFFFFFFF : 0x0) : x);}
};
template<typename T, typename V>
struct WithFlagZ {
    static inline T apply(T l, T f, T x) { return V::toType(l == 2 ? (f ? 0xFFFFFFFF : 0x0) : x);}
};
template<typename T, typename V>
struct WithFlagW {
    static inline T apply(T l, T f, T x) { return V::toType(l == 3 ? (f ? 0xFFFFFFFF : 0x0) : x);}
};
template<typename T, typename V>
struct Shuffle {
    static inline int32_t apply(int32_t l, int32_t mask) {return V::toType((mask >> l) & 0x3);}
};
}

template<typename V, typename Op, typename Vret>
static bool
Func(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc == 1) {
        if((!args[0].isObject() || !ObjectIsVector<V>(args[0].toObject()))) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }
        typename V::Elem *val =
            reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[0].toObject()).typedMem());
        typename Vret::Elem result[Vret::lanes];
        for (int32_t i = 0; i < Vret::lanes; i++)
            result[i] = Op::apply(val[i], 0);

        RootedObject obj(cx, Create<Vret>(cx, result));
        if (!obj)
            return false;

        args.rval().setObject(*obj);
        return true;

    } else if (argc == 2) {
        if((!args[0].isObject() || !ObjectIsVector<V>(args[0].toObject())) ||
           (!args[1].isObject() || !ObjectIsVector<V>(args[1].toObject())))
        {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        typename V::Elem *left =
            reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[0].toObject()).typedMem());
        typename V::Elem *right =
            reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[1].toObject()).typedMem());

        typename Vret::Elem result[Vret::lanes];
        for (int32_t i = 0; i < Vret::lanes; i++)
            result[i] = Op::apply(left[i], right[i]);

        RootedObject obj(cx, Create<Vret>(cx, result));
        if (!obj)
            return false;

        args.rval().setObject(*obj);
        return true;
    } else {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
}

template<typename V, typename OpWith, typename Vret>
static bool
FuncWith(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 2) ||
        (!args[0].isObject() || !ObjectIsVector<V>(args[0].toObject())) ||
        (!args[1].isNumber() && !args[1].isBoolean()))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }

    typename V::Elem *val =
        reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[0].toObject()).typedMem());

    typename Vret::Elem result[Vret::lanes];
    for (int32_t i = 0; i < Vret::lanes; i++) {
        if(args[1].isNumber())
            result[i] = OpWith::apply(i, args[1].toNumber(), val[i]);
        else if (args[1].isBoolean())
            result[i] = OpWith::apply(i, args[1].toBoolean(), val[i]);
    }
    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename V, typename OpShuffle, typename Vret>
static bool
FuncShuffle(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if(argc == 2){
        if ((!args[0].isObject() || !ObjectIsVector<V>(args[0].toObject())) ||
            (!args[1].isNumber()))
        {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }

        typename V::Elem *val =
            reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[0].toObject()).typedMem());
        typename Vret::Elem result[Vret::lanes];
        for (int32_t i = 0; i < Vret::lanes; i++) {
            result[i] = val[OpShuffle::apply(i * 2, args[1].toNumber())];
        }
        RootedObject obj(cx, Create<Vret>(cx, result));
        if (!obj)
            return false;

        args.rval().setObject(*obj);
        return true;
    } else if (argc == 3){
        if ((!args[0].isObject() || !ObjectIsVector<V>(args[0].toObject())) ||
            (!args[1].isObject() || !ObjectIsVector<V>(args[1].toObject())) ||
            (!args[2].isNumber()))
        {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
            return false;
        }
        typename V::Elem *val1 =
            reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[0].toObject()).typedMem());
        typename V::Elem *val2 =
            reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[1].toObject()).typedMem());
        typename Vret::Elem result[Vret::lanes];
        for (int32_t i = 0; i < Vret::lanes; i++) {
            if(i < Vret::lanes / 2)
                result[i] = val1[OpShuffle::apply(i * 2, args[2].toNumber())];
            else
                result[i] = val2[OpShuffle::apply(i * 2, args[2].toNumber())];
        }
        RootedObject obj(cx, Create<Vret>(cx, result));
        if (!obj)
            return false;

        args.rval().setObject(*obj);
        return true;
    } else {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
}

template<typename V, typename Vret>
static bool
FuncConvert(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 1) ||
       (!args[0].isObject() || !ObjectIsVector<V>(args[0].toObject())))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    typename V::Elem *val =
        reinterpret_cast<typename V::Elem *>(AsTypedDatum(args[0].toObject()).typedMem());
    typename Vret::Elem result[Vret::lanes];
    for (int32_t i = 0; i < Vret::lanes; i++)
        result[i] = static_cast<typename Vret::Elem>(val[i]);

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename V, typename Vret>
static bool
FuncConvertBits(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 1) ||
       (!args[0].isObject() || !ObjectIsVector<V>(args[0].toObject())))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    typename Vret::Elem *val =
        reinterpret_cast<typename Vret::Elem *>(AsTypedDatum(args[0].toObject()).typedMem());

    RootedObject obj(cx, Create<Vret>(cx, val));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename Vret>
static bool
FuncZero(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (argc != 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    typename Vret::Elem result[Vret::lanes];
    for (int32_t i = 0; i < Vret::lanes; i++)
        result[i] = static_cast<typename Vret::Elem>(0);

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

template<typename Vret>
static bool
FuncSplat(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 1) || (!args[0].isNumber())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    typename Vret::Elem result[Vret::lanes];
    for (int32_t i = 0; i < Vret::lanes; i++)
        result[i] = static_cast<typename Vret::Elem>(args[0].toNumber());

    RootedObject obj(cx, Create<Vret>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Float32x4Construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 4) ||
        (!args[0].isNumber()) || !args[1].isNumber() ||
        (!args[2].isNumber()) || !args[3].isNumber())
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    Float32x4::Elem result[Float32x4::lanes];
    for (int32_t i = 0; i < Float32x4::lanes; i++)
        result[i] = static_cast<Float32x4::Elem>(args[i].toNumber());

    RootedObject obj(cx, Create<Float32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Int32x4Construct(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 4) ||
        (!args[0].isNumber()) || !args[1].isNumber() ||
        (!args[2].isNumber()) || !args[3].isNumber())
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    Int32x4::Elem result[Int32x4::lanes];
    for (int32_t i = 0; i < Int32x4::lanes; i++)
        result[i] = static_cast<Int32x4::Elem>(args[i].toNumber());

    RootedObject obj(cx, Create<Int32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Int32x4Bool(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 4) ||
        (!args[0].isBoolean()) || !args[1].isBoolean() ||
        (!args[2].isBoolean()) || !args[3].isBoolean())
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    int32_t result[Int32x4::lanes];
    for (int32_t i = 0; i < Int32x4::lanes; i++)
        result[i] = args[i].toBoolean() ? 0xFFFFFFFF : 0x0;

    RootedObject obj(cx, Create<Int32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Float32x4Clamp(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 3) ||
        (!args[0].isObject() || !ObjectIsVector<Float32x4>(args[0].toObject())) ||
        (!args[1].isObject() || !ObjectIsVector<Float32x4>(args[1].toObject())) ||
        (!args[2].isObject() || !ObjectIsVector<Float32x4>(args[2].toObject())))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    float *val = reinterpret_cast<float *>(AsTypedDatum(args[0].toObject()).typedMem());
    float *lowerLimit = reinterpret_cast<float *>(AsTypedDatum(args[1].toObject()).typedMem());
    float *upperLimit = reinterpret_cast<float *>(AsTypedDatum(args[2].toObject()).typedMem());

    float result[Float32x4::lanes];
    result[0] = val[0] < lowerLimit[0] ? lowerLimit[0] : val[0];
    result[1] = val[1] < lowerLimit[1] ? lowerLimit[1] : val[1];
    result[2] = val[2] < lowerLimit[2] ? lowerLimit[2] : val[2];
    result[3] = val[3] < lowerLimit[3] ? lowerLimit[3] : val[3];
    result[0] = result[0] > upperLimit[0] ? upperLimit[0] : result[0];
    result[1] = result[1] > upperLimit[1] ? upperLimit[1] : result[1];
    result[2] = result[2] > upperLimit[2] ? upperLimit[2] : result[2];
    result[3] = result[3] > upperLimit[3] ? upperLimit[3] : result[3];
    RootedObject obj(cx, Create<Float32x4>(cx, result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
Int32x4Select(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if ((argc != 3) ||
        (!args[0].isObject() || !ObjectIsVector<Int32x4>(args[0].toObject())) ||
        (!args[1].isObject() || !ObjectIsVector<Float32x4>(args[1].toObject())) ||
        (!args[2].isObject() || !ObjectIsVector<Float32x4>(args[2].toObject())))
    {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_BAD_ARGS);
        return false;
    }
    int32_t *val = reinterpret_cast<int32_t *>(AsTypedDatum(args[0].toObject()).typedMem());
    int32_t *tv = reinterpret_cast<int32_t *>(AsTypedDatum(args[1].toObject()).typedMem());
    int32_t *fv = reinterpret_cast<int32_t *>(AsTypedDatum(args[2].toObject()).typedMem());
    int32_t tr[Int32x4::lanes];
    for (int32_t i = 0; i < Int32x4::lanes; i++)
        tr[i] = And<int32_t, Int32x4>::apply(val[i], tv[i]);
    int32_t fr[Int32x4::lanes];
    for (int32_t i = 0; i < Int32x4::lanes; i++)
        fr[i] = And<int32_t, Int32x4>::apply(Not<int32_t, Int32x4>::apply(val[i], 0), fv[i]);
    int32_t orInt[Int32x4::lanes];
    for (int32_t i = 0; i < Int32x4::lanes; i++)
        orInt[i] = Or<int32_t, Int32x4>::apply(tr[i], fr[i]);
    float *result[Float32x4::lanes];
    *result = reinterpret_cast<float *>(&orInt);
    RootedObject obj(cx, Create<Float32x4>(cx, *result));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

#define DEFINE_SIMD_FLOAT32X4_FUNCTION(Name, Func, Operands, Flags, MIRId)     \
bool                                                                           \
js::simd_float32x4_##Name(JSContext *cx, unsigned argc, Value *vp)             \
{                                                                              \
    return Func(cx, argc, vp);                                                 \
}
FLOAT32X4_FUNCTION_LIST(DEFINE_SIMD_FLOAT32X4_FUNCTION)
#undef DEFINE_SIMD_FLOAT32x4_FUNCTION

#define DEFINE_SIMD_INT32X4_FUNCTION(Name, Func, Operands, Flags, MIRId)       \
bool                                                                           \
js::simd_int32x4_##Name(JSContext *cx, unsigned argc, Value *vp)               \
{                                                                              \
    return Func(cx, argc, vp);                                                 \
}
INT32X4_FUNCTION_LIST(DEFINE_SIMD_INT32X4_FUNCTION)
#undef DEFINE_SIMD_INT32X4_FUNCTION

const JSFunctionSpec js::Float32x4Methods[] = {
#define SIMD_FLOAT32X4_FUNCTION_ITEM(Name, Func, Operands, Flags, MIRId)       \
        JS_FN(#Name, js::simd_float32x4_##Name, Operands, Flags),
        FLOAT32X4_FUNCTION_LIST(SIMD_FLOAT32X4_FUNCTION_ITEM)
#undef SIMD_FLOAT32x4_FUNCTION_ITEM
        JS_FS_END
};

const JSFunctionSpec js::Int32x4Methods[] = {
#define SIMD_INT32X4_FUNCTION_ITEM(Name, Func, Operands, Flags, MIRId)         \
        JS_FN(#Name, js::simd_int32x4_##Name, Operands, Flags),
        INT32X4_FUNCTION_LIST(SIMD_INT32X4_FUNCTION_ITEM)
#undef SIMD_INT32X4_FUNCTION_ITEM
        JS_FS_END
};
