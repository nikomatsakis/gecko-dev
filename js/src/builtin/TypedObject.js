#include "TypedObjectConstants.h"

///////////////////////////////////////////////////////////////////////////
// Getters and setters for various slots.

// Type object slots

#define DESCR_SIZE(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_SIZE)
#define DESCR_TYPE(obj)   \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_TYPE)
#define DESCR_STRUCT_FIELD_NAMES(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_NAMES)
#define DESCR_STRUCT_FIELD_TYPES(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_TYPES)
#define DESCR_STRUCT_FIELD_OFFSETS(obj) \
    UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS)

// Typed object slots

#define TYPEDOBJ_BYTEOFFSET(obj) \
    TO_INT32(UnsafeGetReservedSlot(obj, JS_BUFVIEW_SLOT_BYTEOFFSET))
#define TYPEDOBJ_OWNER(obj) \
    UnsafeGetReservedSlot(obj, JS_BUFVIEW_SLOT_OWNER)
#define TYPEDOBJ_LENGTH(obj) \
    TO_INT32(UnsafeGetReservedSlot(obj, JS_BUFVIEW_SLOT_LENGTH))

#define HAS_PROPERTY(obj, prop) \
    callFunction(std_Object_hasOwnProperty, obj, prop)

function _StringReprEq(a, b) {
  // Potential optimization: a and b here are known to be atoms
  // (though I know of no way to assert that). We could convert this
  // to an intrinsic and have ion use pointer comparison.

  assert(typeof a === "string", "a is not a string");
  assert(typeof b === "string", "b is not a string");
  return a === b;
}

///////////////////////////////////////////////////////////////////////////
// Type assertions
//
// These check the types of various values. Note that these checks are
// NOT disabled in optimized builds. Instead, they are carefully
// written so that ion can optimize them away to nothing if the
// predicted types match the expected types. This offers us full
// memory protection in optimized builds at zero expected cost. Note
// that we (intentionally) do not protect against null dereferences.
//
// The general structure of each test is:
//
//    typeof obj === "object" && Predicate(obj)
//
// TI is generally able to predict and constant propagate `typeof`
// calls, so that portion should be optimized away. Similarly the
// predicates are all inlined and constant propagated. Note that I did
// not use `IsObject` -- that utility also checks that `obj` is not
// null, which TI cannot predict and which we don't need for memory
// safety.

function _EnforceIsTypeDescr(obj) {
  if ((typeof obj === "object" || typeof obj === "function") && ObjectIsTypeDescr(obj))
    return obj;
  AssertionFailed("Internal SpiderMonkey Error: Expected a type object.");
  return null;
}
SetScriptHints(_EnforceIsTypeDescr,          { inline: true });

function _EnforceIsArrayTypeDescr(obj) {
  if ((typeof obj === "object" || typeof obj === "function") && TypeDescrIsArrayType(obj))
    return obj;
  AssertionFailed("Internal SpiderMonkey Error: Expected an array type object.");
  return null;
}
SetScriptHints(_EnforceIsArrayTypeDescr,     { inline: true });

function _EnforceIsTypedObject(obj) {
  if (typeof obj === "object" && ObjectIsTypedObject(obj))
    return obj;
  AssertionFailed("Internal SpiderMonkey Error: Expected a typed object.");
  return null;
}
SetScriptHints(_EnforceIsTypedObject,          { inline: true });

function _EnforceIsTypedProto(obj) {
  if (typeof obj === "object" && ObjectIsTypedProto(obj))
    return obj;
  AssertionFailed("Internal SpiderMonkey Error: Expected a typed object prototype.");
  return null;
}
SetScriptHints(_EnforceIsTypedProto,          { inline: true });

function _EnforceIsShape(obj) {
  if (obj === null || IsShapeObject(obj))
    return obj;
  AssertionFailed("Internal SpiderMonkey Error: Expected a shape object.");
  return null;
}
SetScriptHints(_EnforceIsShape,          { inline: true });

function _EnforceIsInt(obj) {
  if (typeof obj === "number" && TO_INT32(obj) === obj)
    return obj;
  AssertionFailed("Internal SpiderMonkey Error: Expected an integer");
  return 0;
}
SetScriptHints(_EnforceIsInt,                 { inline: true });

function _EnforceIsAtom(obj) {
  if (typeof obj === "string") // not technically sufficient
    return obj;
  AssertionFailed("Internal SpiderMonkey Error: Expected an atomized string");
  return "";
}
SetScriptHints(_EnforceIsAtom,                { inline: true });

///////////////////////////////////////////////////////////////////////////
// Accessors for primitive slots
//
// In optimized builds, these should optimize to just a raw
// `UnsafeGetReservedSlot()`. Ensure that every call to
// `UnsafeGetReservedSlot` is protected by an appropriate
// `_EnforceIsThisOrThat()` call to guarantee memory safety under all
// conditions. Additional conditions that TI cannot enforce can be
// checked with assertions so that they are optimized away -- these
// are not typically required for memory safety (though violating them
// will likely lead to some kind of exception later on).

function _DescrTypedProto(obj) {
  _EnforceIsTypeDescr(obj);
  return _EnforceIsTypedProto(UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_TYPROTO));
}
SetScriptHints(_DescrTypedProto, { inline: true });

function _DescrLength(obj) {
  _EnforceIsTypeDescr(obj);
  return _EnforceIsInt(UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_LENGTH));
}
SetScriptHints(_DescrLength, { inline: true });

function _DescrInnerShape(obj) {
  _EnforceIsTypeDescr(obj);
  return _EnforceIsShape(UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_INNER_SHAPE));
}
SetScriptHints(_DescrInnerShape, { inline: true });

function _DescrKind(obj) {
  return _TypedProtoKind(_DescrTypedProto(obj));
}
SetScriptHints(_DescrKind, { inline: true });

function _DescrAlignment(obj) {
  return _TypedProtoAlignment(_DescrTypedProto(obj));
}
SetScriptHints(_DescrAlignment, { inline: true });

function _DescrStringRepr(obj) {
  return _TypedProtoStringRepr(_DescrTypedProto(obj));
}
SetScriptHints(_DescrStringRepr, { inline: true });

function _DescrOpaque(obj) {
  return _TypedProtoOpaque(_DescrTypedProto(obj));
}
SetScriptHints(_DescrOpaque, { inline: true });

function _DescrArrayElementType(obj) {
  _EnforceIsArrayTypeDescr(obj);
  return _EnforceIsTypeDescr(UnsafeGetReservedSlot(obj, JS_DESCR_SLOT_ARRAY_ELEM_TYPE));
}
SetScriptHints(_DescrArrayElementType, { inline: true });

function _TypedObjectProto(obj) {
  _EnforceIsTypedObject(obj);
  return _EnforceIsTypedProto(obj.__proto__);
}
SetScriptHints(_TypedObjectProto, { inline: true });

function _TypedObjectDescr(typedObj) {
  return _TypedProtoDescr(_TypedObjectProto(typedObj));
}
SetScriptHints(_TypedObjectDescr, { inline: true });

function _TypedObjectLength(typedObj) {
  return _EnforceIsInt(UnsafeGetReservedSlot(typedObj, JS_BUFVIEW_SLOT_LENGTH));
}
SetScriptHints(_TypedObjectLength, { inline: true });

function _TypedObjectInnerShape(typedObj) {
  return _EnforceIsShape(UnsafeGetReservedSlot(typedObj, JS_TYPEDOBJ_SLOT_INNER_SHAPE));
}
SetScriptHints(_TypedObjectInnerShape, { inline: true });

function _TypedProtoDescr(obj) {
  _EnforceIsTypedProto(obj);
  return _EnforceIsTypeDescr(UnsafeGetReservedSlot(obj, JS_TYPROTO_SLOT_DESCR));
}
SetScriptHints(_TypedProtoDescr, { inline: true });

function _TypedProtoStringRepr(obj) {
  _EnforceIsTypedProto(obj);
  return _EnforceIsAtom(UnsafeGetReservedSlot(obj, JS_TYPROTO_SLOT_STRING_REPR));
}
SetScriptHints(_TypedProtoStringRepr, { inline: true });

function _TypedProtoKind(obj) {
  _EnforceIsTypedProto(obj);
  return _EnforceIsInt(UnsafeGetReservedSlot(obj, JS_TYPROTO_SLOT_KIND));
}
SetScriptHints(_TypedProtoKind, { inline: true });

function _TypedProtoAlignment(obj) {
  _EnforceIsTypedProto(obj);
  return _EnforceIsInt(UnsafeGetReservedSlot(obj, JS_TYPROTO_SLOT_ALIGNMENT));
}
SetScriptHints(_TypedProtoAlignment, { inline: true });

function _TypedProtoOpaque(obj) {
  _EnforceIsTypedProto(obj);
  return _EnforceIsInt(UnsafeGetReservedSlot(obj, JS_TYPROTO_SLOT_OPAQUE));
}
SetScriptHints(_TypedProtoOpaque, { inline: true });

// Given a typed object, returns an object of form `{descr, shape}`.
// The field `descr` will be a type descriptor and `shape` will either
// be null or a non-empty array. If `shape` is an array, then this
// typed object is a array of `descr` elements with that shape.
// Otherwise, this typed object is not an array and has type
// descriptor `descr`.
function _TypedObjectDescrAndShape(typedObj) {
  _EnforceIsTypedObject(typedObj);

  var proto = _TypedObjectProto(typedObj);
  var kind = _TypedProtoKind(proto);
  var descr, shape;
  switch (kind) {
  case JS_TYPE_SCALAR_KIND:
  case JS_TYPE_REFERENCE_KIND:
  case JS_TYPE_X4_KIND:
  case JS_TYPE_STRUCT_KIND:
    descr = _TypedProtoDescr(proto);
    shape = null;
    break;

  case JS_TYPE_ARRAY_KIND:
    // FIXME -- In future patches, the shape will all be derived from
    // typedObj and not from the proto at all. This may affect the
    // return type of this function, depending on what we choose to
    // do.

    descr = _DescrArrayElementType(_TypedProtoDescr(proto));
    shape = [typedObj.length];
    while (_DescrKind(descr) == JS_TYPE_ARRAY_KIND) {
      ARRAY_PUSH(shape, _DescrLength(descr));
      descr = _DescrArrayElementType(descr);
    }
    break;

  default:
    ThrowError("Internal error: Unhandled kind " + kind);
  }

  return {descr: descr, shape: shape};
}

// Given a type object, returns an object of form `{descr, shape}`.
// The field `descr` will be a type descriptor and `shape` will either
// be null or a non-empty array. If `shape` is an array, then this
// typed object is a array of `descr` elements with that shape.
// Otherwise, this typed object is not an array and has type
// descriptor `descr`.
function _ArrayDescrAndShape(arrayDescr) {
  _EnforceIsTypeDescr(arrayDescr);
  assert(_DescrKind(arrayDescr) == JS_TYPE_ARRAY_KIND,
         "_ArrayDescrAndShape invoked on non-sized-array");

  var shape = [_DescrLength(arrayDescr)];
  var elemDescr = _DescrArrayElementType(arrayDescr);
  while (_DescrKind(elemDescr) == JS_TYPE_ARRAY_KIND) {
    ARRAY_PUSH(shape, _DescrLength(elemDescr));
    elemDescr = _DescrArrayElementType(elemDescr);
  }
  return { descr: elemDescr, shape: shape };
}

///////////////////////////////////////////////////////////////////////////
// Getting values
//
// The methods in this section read from the memory pointed at
// by `this` and produce JS values. This process is called *reification*
// in the spec.

// Reifies the value referenced by the pointer, meaning that it
// returns a new object pointing at the value. If the value is
// a scalar, it will return a JS number, but otherwise the reified
// result will be a typedObj of the same class as the ptr's typedObj.
function TypedObjectGet(typedObj, offset, proto, length, innerShape) {
  _EnforceIsTypedObject(typedObj);
  _EnforceIsTypedProto(proto);
  _EnforceIsInt(offset);
  _EnforceIsInt(length);
  _EnforceIsShape(innerShape);
  assert(TypedObjectIsAttached(typedObj), "TypedObjectGet: unattached");

  switch (_TypedProtoKind(proto)) {
  case JS_TYPE_SCALAR_KIND:
    return TypedObjectGetScalar(typedObj, offset, proto);

  case JS_TYPE_REFERENCE_KIND:
    return TypedObjectGetReference(typedObj, offset, proto);

  case JS_TYPE_X4_KIND:
    return TypedObjectGetX4(typedObj, offset, proto);

  case JS_TYPE_ARRAY_KIND:
  case JS_TYPE_STRUCT_KIND:
    return NewDerivedTypedObject(typedObj, offset, proto, length, innerShape);
  }

  assert(false, "Unhandled kind: " + _TypedProtoKind(proto));
  return undefined;
}

function TypedObjectGetScalar(typedObj, offset, proto) {
  var descr = _TypedProtoDescr(proto);
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_SCALARTYPE_INT8:
    return Load_int8(typedObj, offset);

  case JS_SCALARTYPE_UINT8:
  case JS_SCALARTYPE_UINT8_CLAMPED:
    return Load_uint8(typedObj, offset);

  case JS_SCALARTYPE_INT16:
    return Load_int16(typedObj, offset);

  case JS_SCALARTYPE_UINT16:
    return Load_uint16(typedObj, offset);

  case JS_SCALARTYPE_INT32:
    return Load_int32(typedObj, offset);

  case JS_SCALARTYPE_UINT32:
    return Load_uint32(typedObj, offset);

  case JS_SCALARTYPE_FLOAT32:
    return Load_float32(typedObj, offset);

  case JS_SCALARTYPE_FLOAT64:
    return Load_float64(typedObj, offset);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectGetReference(typedObj, offset, proto) {
  var descr = _TypedProtoDescr(proto);
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_REFERENCETYPE_ANY:
    return Load_Any(typedObj, offset);

  case JS_REFERENCETYPE_OBJECT:
    return Load_Object(typedObj, offset);

  case JS_REFERENCETYPE_STRING:
    return Load_string(typedObj, offset);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectGetX4(typedObj, offset, proto) {
  var descr = _TypedProtoDescr(proto);
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_X4TYPE_FLOAT32:
    var x = Load_float32(typedObj, offset + 0);
    var y = Load_float32(typedObj, offset + 4);
    var z = Load_float32(typedObj, offset + 8);
    var w = Load_float32(typedObj, offset + 12);
    return GetFloat32x4TypeDescr()(x, y, z, w);

  case JS_X4TYPE_INT32:
    var x = Load_int32(typedObj, offset + 0);
    var y = Load_int32(typedObj, offset + 4);
    var z = Load_int32(typedObj, offset + 8);
    var w = Load_int32(typedObj, offset + 12);
    return GetInt32x4TypeDescr()(x, y, z, w);
  }

  assert(false, "Unhandled x4 type: " + type);
  return undefined;
}

function NewDerivedTypedObjectIf(cond, typedObj, offset,
                                 proto, length, innerShape) {
  return (cond
          ? NewDerivedTypedObject(typedObj, offset,
                                  proto, length, innerShape)
          : undefined);
}

function NewDerivedOpaqueTypedObjectIf(cond, typedObj, offset,
                                       proto, length, innerShape) {
  return (cond
          ? NewDerivedOpaqueTypedObject(typedObj, offset,
                                        proto, length, innerShape)
          : undefined);
}

///////////////////////////////////////////////////////////////////////////
// Setting values
//
// The methods in this section modify the data pointed at by `this`.

// Writes `fromValue` into the `typedObj` at offset `offset`, adapting
// it to `descr` as needed. This is the most general entry point
// and works for any type.
function TypedObjectSet(descr, typedObj, offset, fromValue) {
  assert(TypedObjectIsAttached(typedObj), "set() called with unattached typedObj");

  // Fast path: `fromValue` is a typed object with same type
  // representation as the destination. In that case, we can just do a
  // memcpy.
  if (IsObject(fromValue) && ObjectIsTypedObject(fromValue) && !descr.variable) {
    var descrStringRepr = _DescrStringRepr(descr);
    var tyProtoStringRepr = _TypedProtoStringRepr(_TypedObjectProto(fromValue));
    if (_StringReprEq(descrStringRepr, tyProtoStringRepr)) {
      if (!TypedObjectIsAttached(fromValue))
        ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

      var size = DESCR_SIZE(descr);
      Memcpy(typedObj, offset, fromValue, 0, size);
      return;
    }
  }

  switch (_DescrKind(descr)) {
  case JS_TYPE_SCALAR_KIND:
    TypedObjectSetScalar(descr, typedObj, offset, fromValue);
    return;

  case JS_TYPE_REFERENCE_KIND:
    TypedObjectSetReference(descr, typedObj, offset, fromValue);
    return;

  case JS_TYPE_X4_KIND:
    TypedObjectSetX4(descr, typedObj, offset, fromValue);
    return;

  case JS_TYPE_ARRAY_KIND:
    if (!IsObject(fromValue))
      break;

    var length = _DescrLength(descr);

    // Check that "array-like" fromValue has an appropriate length.
    if (fromValue.length !== length)
      break;

    // Adapt each element.
    if (length > 0) {
      var elemDescr = _DescrArrayElementType(descr);
      var elemSize = DESCR_SIZE(elemDescr);
      var elemOffset = offset;
      for (var i = 0; i < length; i++) {
        TypedObjectSet(elemDescr, typedObj, elemOffset, fromValue[i]);
        elemOffset += elemSize;
      }
    }
    return;

  case JS_TYPE_STRUCT_KIND:
    if (!IsObject(fromValue))
      break;

    // Adapt each field.
    var fieldNames = DESCR_STRUCT_FIELD_NAMES(descr);
    var fieldDescrs = DESCR_STRUCT_FIELD_TYPES(descr);
    var fieldOffsets = DESCR_STRUCT_FIELD_OFFSETS(descr);
    for (var i = 0; i < fieldNames.length; i++) {
      var fieldName = fieldNames[i];
      var fieldDescr = fieldDescrs[i];
      var fieldOffset = fieldOffsets[i];
      var fieldValue = fromValue[fieldName];
      TypedObjectSet(fieldDescr, typedObj, offset + fieldOffset, fieldValue);
    }
    return;
  }

  ThrowError(JSMSG_CANT_CONVERT_TO,
             typeof(fromValue),
             _DescrStringRepr(descr));
}

// Sets `fromValue` to `this` assuming that `this` is a scalar type.
function TypedObjectSetScalar(descr, typedObj, offset, fromValue) {
  assert(_DescrKind(descr) === JS_TYPE_SCALAR_KIND,
         "Expected scalar type descriptor");
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_SCALARTYPE_INT8:
    return Store_int8(typedObj, offset,
                     TO_INT32(fromValue) & 0xFF);

  case JS_SCALARTYPE_UINT8:
    return Store_uint8(typedObj, offset,
                      TO_UINT32(fromValue) & 0xFF);

  case JS_SCALARTYPE_UINT8_CLAMPED:
    var v = ClampToUint8(+fromValue);
    return Store_int8(typedObj, offset, v);

  case JS_SCALARTYPE_INT16:
    return Store_int16(typedObj, offset,
                      TO_INT32(fromValue) & 0xFFFF);

  case JS_SCALARTYPE_UINT16:
    return Store_uint16(typedObj, offset,
                       TO_UINT32(fromValue) & 0xFFFF);

  case JS_SCALARTYPE_INT32:
    return Store_int32(typedObj, offset,
                      TO_INT32(fromValue));

  case JS_SCALARTYPE_UINT32:
    return Store_uint32(typedObj, offset,
                       TO_UINT32(fromValue));

  case JS_SCALARTYPE_FLOAT32:
    return Store_float32(typedObj, offset, +fromValue);

  case JS_SCALARTYPE_FLOAT64:
    return Store_float64(typedObj, offset, +fromValue);
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

function TypedObjectSetReference(descr, typedObj, offset, fromValue) {
  var type = DESCR_TYPE(descr);
  switch (type) {
  case JS_REFERENCETYPE_ANY:
    return Store_Any(typedObj, offset, fromValue);

  case JS_REFERENCETYPE_OBJECT:
    var value = (fromValue === null ? fromValue : ToObject(fromValue));
    return Store_Object(typedObj, offset, value);

  case JS_REFERENCETYPE_STRING:
    return Store_string(typedObj, offset, ToString(fromValue));
  }

  assert(false, "Unhandled scalar type: " + type);
  return undefined;
}

// Sets `fromValue` to `this` assuming that `this` is a scalar type.
function TypedObjectSetX4(descr, typedObj, offset, fromValue) {
  // It is only permitted to set a float32x4/int32x4 value from another
  // float32x4/int32x4; in that case, the "fast path" that uses memcopy will
  // have already matched. So if we get to this point, we're supposed
  // to "adapt" fromValue, but there are no legal adaptions.
  ThrowError(JSMSG_CANT_CONVERT_TO,
             typeof(fromValue),
             _DescrStringRepr(descr));
}

///////////////////////////////////////////////////////////////////////////
// C++ Wrappers
//
// These helpers are invoked by C++ code or used as method bodies.

// Wrapper for use from C++ code.
function ConvertAndCopyTo(destDescr,
                          destTypedObj,
                          destOffset,
                          fromValue)
{
  assert(IsObject(destDescr) && ObjectIsTypeDescr(destDescr),
         "ConvertAndCopyTo: not type obj");
  assert(IsObject(destTypedObj) && ObjectIsTypedObject(destTypedObj),
         "ConvertAndCopyTo: not type typedObj");

  if (!TypedObjectIsAttached(destTypedObj))
    ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

  TypedObjectSet(destDescr, destTypedObj, destOffset, fromValue);
}

// Wrapper for use from C++ code.
function Reify(sourceDescr,
               sourceTypedObj,
               sourceOffset) {
  assert(IsObject(sourceDescr) && ObjectIsTypeDescr(sourceDescr),
         "Reify: not type obj");
  assert(IsObject(sourceTypedObj) && ObjectIsTypedObject(sourceTypedObj),
         "Reify: not type typedObj");

  if (!TypedObjectIsAttached(sourceTypedObj))
    ThrowError(JSMSG_TYPEDOBJECT_HANDLE_UNATTACHED);

  var proto = _DescrTypedProto(sourceDescr);
  var length = _DescrLength(sourceDescr);
  var innerShape = _DescrInnerShape(sourceDescr);

  return TypedObjectGet(sourceTypedObj, sourceOffset,
                        proto, length, innerShape);
}

// Warning: user exposed!
function TypeDescrEquivalent(otherDescr) {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  if (!IsObject(otherDescr) || !ObjectIsTypeDescr(otherDescr))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  return _StringReprEq(_DescrStringRepr(this), _DescrStringRepr(otherDescr));
}

// TypedArray.redimension(newArrayType)
//
// Method that "repackages" the data from this array into a new typed
// object whose type is `newArrayType`. Once you strip away all the
// outer array dimensions, the type of `this` array and `newArrayType`
// must share the same innermost element type. Moreover, those
// stripped away dimensions must amount to the same total number of
// elements.
//
// For example, given two equivalent types `T` and `U`, it is legal to
// interconvert between arrays types like:
//     T[32]
//     U[2][16]
//     U[2][2][8]
// Because they all share the same total number (32) of equivalent elements.
// But it would be illegal to convert `T[32]` to `U[31]` or `U[2][17]`, since
// the number of elements differs. And it's just plain incompatible to convert
// if the base element types are not equivalent.
//
// Warning: user exposed!
function TypedArrayRedimension(newArrayType) {
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (!IsObject(newArrayType) || !ObjectIsTypeDescr(newArrayType))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var {descr: oldElementType, shape: oldShape} = _TypedObjectDescrAndShape(this);

  // Count the total number of elements in the shape.
  var oldElementCount = 1;
  if (oldShape) {
    for (var i = 0; i < oldShape.length; i++)
      oldElementCount *= oldShape[i];
  }

  // Peel away the outermost array layers from `newArrayType`. In the
  // process, count the number of elements.
  var newElementType = newArrayType;
  var newElementCount = 1;
  while (_DescrKind(newElementType) == JS_TYPE_ARRAY_KIND) {
    newElementCount *= newElementType.length;
    newElementType = newElementType.elementType;
  }

  // Check that the total number of elements does not change.
  if (oldElementCount !== newElementCount)
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Check that the element types are equivalent.
  //
  // FIXME We should really require that the two types be the *same*,
  // not equivalent. See
  // <https://github.com/dslomov-chromium/typed-objects-es7/issues/6>.
  var oldElementStringRepr = _DescrStringRepr(oldElementType);
  var newElementStringRepr = _DescrStringRepr(newElementType);
  if (!_StringReprEq(oldElementStringRepr, newElementStringRepr))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Rewrap the data from `this` in a new type.
  return NewDerivedTypedObject(this, 0,
                               _DescrTypedProto(newArrayType),
                               _DescrLength(newArrayType),
                               _DescrInnerShape(newArrayType));
}

///////////////////////////////////////////////////////////////////////////
// X4

function X4ProtoString(type) {
  switch (type) {
  case JS_X4TYPE_INT32:
    return "int32x4";
  case JS_X4TYPE_FLOAT32:
    return "float32x4";
  }

  assert(false, "Unhandled type constant");
  return undefined;
}

function X4ToSource() {
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    ThrowError(JSMSG_INCOMPATIBLE_PROTO, "X4", "toSource", typeof this);

  var typedProto = _TypedObjectProto(this);
  if (_TypedProtoKind(typedProto) != JS_TYPE_X4_KIND)
    ThrowError(JSMSG_INCOMPATIBLE_PROTO, "X4", "toSource", typeof this);

  var type = DESCR_TYPE(_TypedProtoDescr(typedProto));
  return X4ProtoString(type)+"("+this.x+", "+this.y+", "+this.z+", "+this.w+")";
}

///////////////////////////////////////////////////////////////////////////
// Miscellaneous

// toSource() for type descriptors.
//
// Warning: user exposed!
function DescrToSource() {
  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_INCOMPATIBLE_PROTO, "Type", "toSource", "value");

  return _DescrStringRepr(this);
}

// Warning: user exposed!
function ArrayShorthand(...dims) {
  if (!IsObject(this) || !ObjectIsTypeDescr(this) || dims.length == 0)
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var T = GetTypedObjectModule();

  var accum = this;
  for (var i = dims.length - 1; i >= 0; i--)
    accum = new T.ArrayType(accum, dims[i]);
  return accum;
}

// This is the `storage()` function defined in the spec.  When
// provided with a *transparent* typed object, it returns an object
// containing buffer, byteOffset, byteLength. When given an opaque
// typed object, it returns null. Otherwise it throws.
//
// Warning: user exposed!
function StorageOfTypedObject(obj) {
  if (IsObject(obj)) {
    if (ObjectIsOpaqueTypedObject(obj))
      return null;

    if (ObjectIsTransparentTypedObject(obj)) {
      var descrAndShape = _TypedObjectDescrAndShape(obj);
      var byteLength = DESCR_SIZE(descrAndShape.descr);
      if (descrAndShape.shape) {
        for (var i = 0; i < descrAndShape.shape.length; i++)
          byteLength *= descrAndShape.shape[i];
      }

      return { buffer: TYPEDOBJ_OWNER(obj),
               byteLength: byteLength,
               byteOffset: TYPEDOBJ_BYTEOFFSET(obj) };
    }
  }

  ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  return null; // pacify silly "always returns a value" lint
}

// This is the `objectType()` function defined in the spec.
// It returns the type of its argument.
//
// Warning: user exposed!
function TypeOfTypedObject(obj) {
  if (IsObject(obj) && ObjectIsTypedObject(obj)) {
    var proto = _TypedObjectProto(obj);
    switch (_TypedProtoKind(proto)) {
    case JS_TYPE_SCALAR_KIND:
    case JS_TYPE_REFERENCE_KIND:
      // Scalars and references can't be "instantiated", so there should
      // never be a typed object with this kind.
      break;

    case JS_TYPE_X4_KIND:
    case JS_TYPE_STRUCT_KIND:
      return _TypedProtoDescr(proto);

    case JS_TYPE_ARRAY_KIND:
      var length = TYPEDOBJ_LENGTH(obj);
      var elemDescr = _DescrArrayElementType(_TypedProtoDescr(proto));
      return callFunction(ArrayShorthand, elemDescr, length);
    }

    assert(false, "Invalid kind for typed object prototype: "+TYPROTO_KIND(proto));
  }

  // Note: Do not create bindings for `Any`, `String`, etc in
  // Utilities.js, but rather access them through
  // `GetTypedObjectModule()`. The reason is that bindings
  // you create in Utilities.js are part of the self-hosted global,
  // vs the user-accessible global, and hence should not escape to
  // user script.
  var T = GetTypedObjectModule();
  switch (typeof obj) {
    case "object": return T.Object;
    case "function": return T.Object;
    case "string": return T.String;
    case "number": return T.float64;
    case "undefined": return T.Any;
    default: return T.Any;
  }
}

///////////////////////////////////////////////////////////////////////////
// TypedObject surface API methods (sequential implementations).

function ObjectIsArrayTypeDescr(obj) {
  // FIXME this is goofy, we ought to make these predicates just accept
  // arbitrary values.
  return ObjectIsTypeDescr(obj) && TypeDescrIsArrayType(obj);
}

// Warning: user exposed!
function TypedObjectArrayTypeBuild(a,b,c) {
  // Arguments (this sized) : [depth], func
  // Arguments (this unsized) : length, [depth], func

  if (!IsObject(this) || !ObjectIsArrayTypeDescr(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var kind = _DescrKind(this);
  switch (kind) {
  case JS_TYPE_ARRAY_KIND:
    var descrAndShape = _ArrayDescrAndShape(this);
    if (typeof a === "function") // XXX here and elsewhere: these type dispatches are fragile at best.
      return BuildTypedSeqImpl(descrAndShape, 1, a);
    else if (typeof a === "number" && typeof b === "function")
      return BuildTypedSeqImpl(descrAndShape, a, b);
    else if (typeof a === "number")
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
    else
      return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  default:
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }
}

// Warning: user exposed!
function TypedObjectArrayTypeFrom(a, b, c) {
  // Arguments: arrayLike, [depth], func

  if (!IsObject(this) || !ObjectIsTypeDescr(this))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var untypedInput = !IsObject(a) || !ObjectIsTypedObject(a);

  // for untyped input array, the expectation (in terms of error
  // reporting for invalid parameters) is no-depth, despite
  // supporting an explicit depth of 1; while for typed input array,
  // the expectation is explicit depth.

  if (untypedInput) {
    var explicitDepth = (b === 1);
    if (explicitDepth && IsCallable(c))
      return MapUntypedSeqImpl(a, this, c);
    else if (IsCallable(b))
      return MapUntypedSeqImpl(a, this, b);
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  }

  if (_DescrKind(this) != JS_TYPE_ARRAY_KIND)
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var thisDescrAndShape = _ArrayDescrAndShape(this);

  var aDescrAndShape = _TypedObjectDescrAndShape(a);
  if (!aDescrAndShape.shape) // Argument is not an array
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var explicitDepth = (typeof b === "number");
  if (explicitDepth && IsCallable(c))
    return MapTypedSeqImpl(a, b, aDescrAndShape, thisDescrAndShape, c);
  else if (IsCallable(b))
    return MapTypedSeqImpl(a, 1, aDescrAndShape, thisDescrAndShape, b);
  else if (explicitDepth)
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
}

// Warning: user exposed!
function TypedArrayMap(a, b) {
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var thisDescrAndShape = _TypedObjectDescrAndShape(this);
  if (!thisDescrAndShape.shape) // Receiver is not an array
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Arguments: [depth], func
  if (typeof a === "number" && typeof b === "function")
    return MapTypedSeqImpl(this, a, thisDescrAndShape, thisDescrAndShape, b);
  else if (typeof a === "function")
    return MapTypedSeqImpl(this, 1, thisDescrAndShape, thisDescrAndShape, a);
  return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
}

// Warning: user exposed!
function TypedArrayMapPar(a, b) {
  // Arguments: [depth], func

  // FIXME -- for now, just defer to seq code, bring this back near end of
  // patch series
  return callFunction(TypedArrayMap, this, a, b);
}

// Warning: user exposed!
function TypedArrayReduce(a, b) {
  // Arguments: func, [initial]
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var thisType = _TypedObjectDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (a !== undefined && typeof a !== "function")
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var outputType = thisType.elementType;
  return ReduceTypedSeqImpl(this, outputType, a, b);
}

// Warning: user exposed!
function TypedArrayScatter(a, b, c, d) {
  // Arguments: outputArrayType, indices, defaultValue, conflictFunction
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var thisType = _TypedObjectDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (!IsObject(a) || !ObjectIsTypeDescr(a) || !TypeDescrIsArrayType(a))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (d !== undefined && typeof d !== "function")
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  return ScatterTypedSeqImpl(this, a, b, c, d);
}

// Warning: user exposed!
function TypedArrayFilter(func) {
  // Arguments: predicate
  if (!IsObject(this) || !ObjectIsTypedObject(this))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);
  var thisType = _TypedObjectDescr(this);
  if (!TypeDescrIsArrayType(thisType))
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  if (typeof func !== "function")
    return ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  return FilterTypedSeqImpl(this, func);
}

// placeholders

// Warning: user exposed!
function TypedObjectArrayTypeBuildPar(a,b,c) {
  return callFunction(TypedObjectArrayTypeBuild, this, a, b, c);
}

// Warning: user exposed!
function TypedObjectArrayTypeFromPar(a,b,c) {
  // Arguments: arrayLike, [depth], func

  // FIXME -- for now, just defer to seq code, bring this back near end of
  // patch series
  return callFunction(TypedObjectArrayTypeFrom, this, a, b, c);
}

// Warning: user exposed!
function TypedArrayReducePar(a, b) {
  return callFunction(TypedArrayReduce, this, a, b);
}

// Warning: user exposed!
function TypedArrayScatterPar(a, b, c, d) {
  return callFunction(TypedArrayScatter, this, a, b, c, d);
}

// Warning: user exposed!
function TypedArrayFilterPar(func) {
  return callFunction(TypedArrayFilter, this, func);
}

// should eventually become macros
function NUM_BYTES(bits) {
  return (bits + 7) >> 3;
}
function SET_BIT(data, index) {
  var word = index >> 3;
  var mask = 1 << (index & 0x7);
  data[word] |= mask;
}
function GET_BIT(data, index) {
  var word = index >> 3;
  var mask = 1 << (index & 0x7);
  return (data[word] & mask) != 0;
}

// Bug 956914: make performance-tuned variants tailored to 1, 2, and 3 dimensions.
function BuildTypedSeqImpl(descrAndShape, depth, func) {
  _EnforceIsTypeDescr(descrAndShape.descr);
  assert(descrAndShape.shape, "Build called on non-array");

  if (depth <= 0 || TO_INT32(depth) !== depth)
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // For example, if we have as input
  //    ArrayType(ArrayType(T, 4), 5)
  // and a depth of 2, we get
  //    grainType = T
  //    iterationSpace = [5, 4]
  var [iterationSpace, grainType, totalLength] =
    _ComputeIterationSpace(descrAndShape, depth);

  // Create a zeroed instance with no data
  var result = _CreateArray(descrAndShape);

  var indices = NewDenseArray(depth);
  for (var i = 0; i < depth; i++) {
    indices[i] = 0;
  }

  var grainTypeProto = _DescrTypedProto(grainType);
  var grainTypeLength = _DescrLength(grainType);
  var grainTypeInnerShape = _DescrInnerShape(grainType);

  var grainTypeIsComplex = !TypeDescrIsSimpleType(grainType);
  var size = DESCR_SIZE(grainType);
  var outOffset = 0;
  for (i = 0; i < totalLength; i++) {
    // Position out-pointer to point at &result[...indices], if appropriate.
    var userOutPointer = NewDerivedOpaqueTypedObjectIf(
      grainTypeIsComplex, result, outOffset,
      grainTypeProto, grainTypeLength, grainTypeInnerShape);

    // Invoke func(...indices, userOutPointer) and store the result
    callFunction(std_Array_push, indices, userOutPointer);
    var r = callFunction(std_Function_apply, func, undefined, indices);
    callFunction(std_Array_pop, indices);
    if (r !== undefined)
      TypedObjectSet(grainType, result, outOffset, r); // result[...indices] = r;

    // Increment indices.
    IncrementIterationSpace(indices, iterationSpace);
    outOffset += size;
  }

  return result;
}

function _ComputeIterationSpace(descrAndShape, depth) {
  _EnforceIsTypeDescr(descrAndShape.descr);
  _EnforceIsInt(depth);
  assert(depth > 0, "_ComputeIterationSpace called on non-positive depth");

  if (depth > descrAndShape.shape.length) // depth too high
    ThrowError(JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);

  // Extract the outermost `depth` dimensions into `iterationSpace`
  // and compute total number of elements we're iterating over.
  var iterationSpace = NewDenseArray(depth);
  var totalLength = 1;
  for (var i = 0; i < depth; i++) {
    var l = descrAndShape.shape[i];
    iterationSpace[i] = l
    totalLength *= l;
  }

  // Construct a type descriptor that wraps the grain type
  // in whatever remaining dimensions there are.
  //
  // FIXME This will change in later patches and not be so slow.
  var grainType = _CreateArrayDescr(descrAndShape.descr, descrAndShape.shape.slice(depth));

  return [iterationSpace, grainType, totalLength];
}

function _CreateArrayDescr(descr, shape) {
  // FIXME This will change or go away in later patches.
  var T = GetTypedObjectModule();
  for (var i = shape.length - 1; i >= 0; i--)
    descr = new T.ArrayType(descr, shape[i]);
  return descr;
}

function _CreateArray(descrAndShape) {
  // FIXME This will change in later patches and not be so slow.
  var d = _CreateArrayDescr(descrAndShape.descr, descrAndShape.shape);
  return new d();
}

function IncrementIterationSpace(indices, iterationSpace) {
  // Increment something like
  //     [5, 5, 7, 8]
  // in an iteration space of
  //     [9, 9, 9, 9]
  // to
  //     [5, 5, 8, 0]

  assert(indices.length === iterationSpace.length,
         "indices dimension must equal iterationSpace dimension.");
  var n = indices.length - 1;
  while (true) {
    indices[n] += 1;
    if (indices[n] < iterationSpace[n])
      return;

    assert(indices[n] === iterationSpace[n],
         "Components of indices must match those of iterationSpace.");
    indices[n] = 0;
    if (n == 0)
      return;

    n -= 1;
  }
}

// Implements |from| method for untyped |inArray|.  (Depth is implicitly 1 for untyped input.)
function MapUntypedSeqImpl(inArray, outputType, maybeFunc) {
  assert(IsObject(outputType), "1. Map/From called on non-object outputType");
  assert(ObjectIsTypeDescr(outputType), "1. Map/From called on non-type-object outputType");
  inArray = ToObject(inArray);
  assert(TypeDescrIsArrayType(outputType), "Map/From called on non array-type outputType");

  if (!IsCallable(maybeFunc))
    ThrowError(JSMSG_NOT_FUNCTION, DecompileArg(0, maybeFunc));
  var func = maybeFunc;

  // Skip check for compatible iteration spaces; any normal JS array
  // is trivially compatible with any iteration space of depth 1.

  var outLength = outputType.variable ? inArray.length : outputType.length;
  var outGrainType = outputType.elementType;

  // Create a zeroed instance with no data
  var result = outputType.variable ? new outputType(inArray.length) : new outputType();

  var outUnitSize = DESCR_SIZE(outGrainType);
  var outGrainTypeIsComplex = !TypeDescrIsSimpleType(outGrainType);
  var outOffset = 0;

  var outGrainTypeProto = _DescrTypedProto(outGrainType);
  var outGrainTypeLength = _DescrLength(outGrainType);
  var outGrainTypeInnerShape = _DescrInnerShape(outGrainType);

  // Core of map computation starts here (comparable to
  // DoMapTypedSeqDepth1 and DoMapTypedSeqDepthN below).

  for (var i = 0; i < outLength; i++) {
    // In this loop, since depth is 1, "indices" denotes singleton array [i].

    if (i in inArray) { // Check for holes (only needed for untyped case).
      // Extract element value.
      var element = inArray[i];

      // Create out pointer to point at &array[...indices] for result array.
      var out = NewDerivedOpaqueTypedObjectIf(
        outGrainTypeIsComplex, result, outOffset,
        outGrainTypeProto, outGrainTypeLength, outGrainTypeInnerShape);

      // Invoke: var r = func(element, ...indices, collection, out);
      var r = func(element, i, inArray, out);

      if (r !== undefined)
        TypedObjectSet(outGrainType, result, outOffset, r); // result[i] = r
    }

    // Update offset and (implicitly) increment indices.
    outOffset += outUnitSize;
  }

  return result;
}

// Implements |map| and |from| methods for typed |inArray|.
function MapTypedSeqImpl(inArray, depth, inputDescrAndShape,
                         outputDescrAndShape, func) {
  _EnforceIsTypeDescr(inputDescrAndShape.descr);
  _EnforceIsTypeDescr(outputDescrAndShape.descr);
  assert(inputDescrAndShape.shape, "MapTypedSeqImpl: Empty input shape");
  assert(outputDescrAndShape.shape, "MapTypedSeqImpl: Empty input shape");
  _EnforceIsTypedObject(inArray);

  if (depth <= 0 || TO_INT32(depth) !== depth)
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  // Compute iteration space for input and output and check for compatibility.
  var inputType = _TypedObjectDescr(inArray);
  var [inIterationSpace, inGrainType, _] = _ComputeIterationSpace(inputDescrAndShape, depth);
  var [iterationSpace, outGrainType, totalLength] = _ComputeIterationSpace(outputDescrAndShape, depth);

  for (var i = 0; i < depth; i++)
    if (inIterationSpace[i] !== iterationSpace[i])
      // TypeError("Incompatible iteration space in input and output type");
      ThrowError(JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);

  // Create a zeroed instance with no data
  var result = _CreateArray(outputDescrAndShape);

  var inGrainTypeIsComplex = !TypeDescrIsSimpleType(inGrainType);
  var outGrainTypeIsComplex = !TypeDescrIsSimpleType(outGrainType);

  var inOffset = 0;
  var outOffset = 0;

  var isDepth1Simple = depth == 1 && !(inGrainTypeIsComplex || outGrainTypeIsComplex);

  var inUnitSize = isDepth1Simple ? 0 : DESCR_SIZE(inGrainType);
  var outUnitSize = isDepth1Simple ? 0 : DESCR_SIZE(outGrainType);

  // Bug 956914: add additional variants for depth = 2, 3, etc.

  var inGrainTypeProto = _DescrTypedProto(inGrainType);
  var inGrainTypeLength = _DescrLength(inGrainType);
  var inGrainTypeInnerShape = _DescrInnerShape(inGrainType);

  var outGrainTypeProto = _DescrTypedProto(outGrainType);
  var outGrainTypeLength = _DescrLength(outGrainType);
  var outGrainTypeInnerShape = _DescrInnerShape(outGrainType);

  function DoMapTypedSeqDepth1() {
    for (var i = 0; i < totalLength; i++) {
      // In this loop, since depth is 1, "indices" denotes singleton array [i].

      // Prepare input element/handle and out pointer
      var element = TypedObjectGet(inArray, inOffset,
                                   inGrainTypeProto, inGrainTypeLength,
                                   inGrainTypeInnerShape);
      var out = NewDerivedOpaqueTypedObjectIf(outGrainTypeIsComplex,
                                              result, outOffset,
                                              outGrainTypeProto,
                                              outGrainTypeLength,
                                              outGrainTypeInnerShape);

      // Invoke: var r = func(element, ...indices, collection, out);
      var r = func(element, i, inArray, out);
      if (r !== undefined)
        TypedObjectSet(outGrainType, result, outOffset, r); // result[i] = r

      // Update offsets and (implicitly) increment indices.
      inOffset += inUnitSize;
      outOffset += outUnitSize;
    }

    return result;
  }

  function DoMapTypedSeqDepth1Simple(inArray, totalLength, func, result) {
    for (var i = 0; i < totalLength; i++) {
      var r = func(inArray[i], i, inArray, undefined);
      if (r !== undefined)
        result[i] = r;
    }

    return result;
  }

  function DoMapTypedSeqDepthN() {
    var indices = new Uint32Array(depth);

    var inGrainTypeProto = _DescrTypedProto(inGrainType);
    var inGrainTypeLength = _DescrLength(inGrainType);
    var inGrainTypeInnerShape = _DescrInnerShape(inGrainType);

    var outGrainTypeProto = _DescrTypedProto(outGrainType);
    var outGrainTypeLength = _DescrLength(outGrainType);
    var outGrainTypeInnerShape = _DescrInnerShape(outGrainType);

    for (var i = 0; i < totalLength; i++) {
      // Prepare input element and out pointer
      var element = TypedObjectGet(inArray, inOffset,
                                   inGrainTypeProto, inGrainTypeLength,
                                   inGrainTypeInnerShape);
      var out = NewDerivedOpaqueTypedObjectIf(outGrainTypeIsComplex,
                                              result, outOffset,
                                              outGrainTypeProto,
                                              outGrainTypeLength,
                                              outGrainTypeInnerShape);

      // Invoke: var r = func(element, ...indices, collection, out);
      var args = [element];
      callFunction(std_Function_apply, std_Array_push, args, indices);
      callFunction(std_Array_push, args, inArray, out);
      var r = callFunction(std_Function_apply, func, void 0, args);
      if (r !== undefined)
        TypedObjectSet(outGrainType, result, outOffset, r); // result[...indices] = r

      // Update offsets and explicitly increment indices.
      inOffset += inUnitSize;
      outOffset += outUnitSize;
      IncrementIterationSpace(indices, iterationSpace);
    }

    return result;
  }

  if (isDepth1Simple)
    return DoMapTypedSeqDepth1Simple(inArray, totalLength, func, result);

  if (depth == 1)
    return DoMapTypedSeqDepth1();

  return DoMapTypedSeqDepthN();
}

// Implements |map| and |from| methods for typed |inArray|.
function MapTypedParImpl(inArray, depth, outputType, func) {
  assert(IsObject(outputType) && ObjectIsTypeDescr(outputType),
         "Map/From called on non-type-object outputType");
  assert(IsObject(inArray) && ObjectIsTypedObject(inArray),
         "Map/From called on non-object or untyped input array.");
  assert(TypeDescrIsArrayType(outputType),
         "Map/From called on non array-type outputType");
  assert(typeof depth === "number",
         "Map/From called with non-numeric depth");
  assert(IsCallable(func),
         "Map/From called on something not callable");

  var inArrayType = _TypedObjectDescr(inArray);

  if (ShouldForceSequential() ||
      depth <= 0 ||
      TO_INT32(depth) !== depth ||
      !TypeDescrIsArrayType(inArrayType))
  {
    // defer error cases to seq implementation:
    return MapTypedSeqImpl(inArray, depth, outputType, func);
  }

  switch (depth) {
  case 1:
    return MapTypedParImplDepth1(inArray, inArrayType, outputType, func);
  default:
    return MapTypedSeqImpl(inArray, depth, outputType, func);
  }
}

function RedirectPointer(typedObj, offset, outputIsScalar) {
  if (!outputIsScalar || !InParallelSection()) {
    // ^ Subtle note: always check InParallelSection() last, because
    // otherwise the other if conditions will not execute during
    // sequential mode and we will not gather enough type
    // information.

    // Here `typedObj` represents the input or output pointer we will
    // pass to the user function. Ideally, we will just update the
    // offset of `typedObj` in place so that it moves along the
    // input/output buffer without incurring any allocation costs. But
    // we can only do this if these changes are invisible to the user.
    //
    // Under normal uses, such changes *should* be invisible -- the
    // in/out pointers are only intended to be used during the
    // callback and then discarded, but of course in the general case
    // nothing prevents them from escaping.
    //
    // However, if we are in parallel mode, we know that the pointers
    // will not escape into global state. They could still escape by
    // being returned into the resulting array, but even that avenue
    // is impossible if the result array cannot contain objects.
    //
    // Therefore, we reuse a pointer if we are both in parallel mode
    // and we have a transparent output type.  It'd be nice to loosen
    // this condition later by using fancy ion optimizations that
    // assume the value won't escape and copy it if it does. But those
    // don't exist yet. Moreover, checking if the type is transparent
    // is an overapproximation: users can manually declare opaque
    // types that nonetheless only contain scalar data.

    var proto = _TypedObjectProto(typedObj);
    var length = _TypedObjectLength(typedObj);
    var innerShape = _TypedObjectInnerShape(typedObj);
    typedObj = NewDerivedTypedObject(typedObj, 0, proto, length, innerShape);
  }

  SetTypedObjectOffset(typedObj, offset);
  return typedObj;
}
SetScriptHints(RedirectPointer,         { inline: true });

function ReduceTypedSeqImpl(array, outputType, func, initial) {
  assert(IsObject(array) && ObjectIsTypedObject(array), "Reduce called on non-object or untyped input array.");
  assert(IsObject(outputType) && ObjectIsTypeDescr(outputType), "Reduce called on non-type-object outputType");

  var start, value;

  if (initial === undefined && array.length < 1)
    // RangeError("reduce requires array of length > 0")
    ThrowError(JSMSG_TYPEDOBJECT_ARRAYTYPE_BAD_ARGS);

  // FIXME bug 950106 Should reduce method supply an outptr handle?
  // For now, reduce never supplies an outptr, regardless of outputType.

  if (TypeDescrIsSimpleType(outputType)) {
    if (initial === undefined) {
      start = 1;
      value = array[0];
    } else {
      start = 0;
      value = outputType(initial);
    }

    for (var i = start; i < array.length; i++)
      value = outputType(func(value, array[i]));

  } else {
    if (initial === undefined) {
      start = 1;
      value = new outputType(array[0]);
    } else {
      start = 0;
      value = initial;
    }

    for (var i = start; i < array.length; i++)
      value = func(value, array[i]);
  }

  return value;
}

function ScatterTypedSeqImpl(array, outputType, indices, defaultValue, conflictFunc) {
  assert(IsObject(array) && ObjectIsTypedObject(array), "Scatter called on non-object or untyped input array.");
  assert(IsObject(outputType) && ObjectIsTypeDescr(outputType), "Scatter called on non-type-object outputType");
  assert(TypeDescrIsArrayType(outputType), "Scatter called on non-sized array type");
  assert(conflictFunc === undefined || typeof conflictFunc === "function", "Scatter called with invalid conflictFunc");

  var result = new outputType();
  var bitvec = new Uint8Array(result.length);
  var elemType = outputType.elementType;
  var i, j;
  if (defaultValue !== elemType(undefined)) {
    for (i = 0; i < result.length; i++) {
      result[i] = defaultValue;
    }
  }

  for (i = 0; i < indices.length; i++) {
    j = indices[i];
    if (!GET_BIT(bitvec, j)) {
      result[j] = array[i];
      SET_BIT(bitvec, j);
    } else if (conflictFunc === undefined) {
      ThrowError(JSMSG_PAR_ARRAY_SCATTER_CONFLICT);
    } else {
      result[j] = conflictFunc(result[j], elemType(array[i]));
    }
  }
  return result;
}

function FilterTypedSeqImpl(array, func) {
  assert(IsObject(array) && ObjectIsTypedObject(array), "Filter called on non-object or untyped input array.");
  assert(typeof func === "function", "Filter called with non-function predicate");

  var arrayType = _TypedObjectDescr(array);
  if (!TypeDescrIsArrayType(arrayType))
    ThrowError(JSMSG_TYPEDOBJECT_BAD_ARGS);

  var inGrainType = arrayType.inGrainType;
  var inGrainTypeProto = _DescrTypedProto(inGrainType);
  var inGrainTypeLength = _DescrLength(inGrainType);
  var inGrainTypeInnerShape = _DescrInnerShape(inGrainType);

  var flags = new Uint8Array(NUM_BYTES(array.length));
  var count = 0;
  var size = DESCR_SIZE(inGrainType);
  var inOffset = 0;
  for (var i = 0; i < array.length; i++) {
    var v = TypedObjectGet(array, inOffset,
                           inGrainTypeProto, inGrainTypeLength,
                           inGrainTypeInnerShape);
    if (func(v, i, array)) {
      SET_BIT(flags, i);
      count++;
    }
    inOffset += size;
  }

  var resultType = (arrayType.variable ? arrayType : arrayType.unsized);
  var result = new resultType(count);
  for (var i = 0, j = 0; i < array.length; i++) {
    if (GET_BIT(flags, i))
      result[j++] = array[i];
  }
  return result;
}
