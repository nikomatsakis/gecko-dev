/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Specialized .h file to be used by both JS and C++ code.

#ifndef builtin_TypedObjectConstants_h
#define builtin_TypedObjectConstants_h

///////////////////////////////////////////////////////////////////////////
// Slots for typed prototypes

#define JS_TYPROTO_SLOT_DESCR            0
#define JS_TYPROTO_SLOT_STRING_REPR      1  // Atomized string representation
#define JS_TYPROTO_SLOT_KIND             2 // type::Kind constant
#define JS_TYPROTO_SLOT_ALIGNMENT        3 // Alignment in bytes
#define JS_TYPROTO_SLOT_OPAQUE           4 // Atomized string representation

// Slots on sized typed prototypes
#define JS_TYPROTO_SIZED_SLOTS           5

// Slots on array typed prototypes
#define JS_TYPROTO_ARRAY_SLOTS           5

///////////////////////////////////////////////////////////////////////////
// Slots for type objects
//
// Some slots apply to all type objects and some are specific to
// particular kinds of type objects. For simplicity we use the same
// number of slots no matter what kind of type descriptor we are
// working with, even though this is mildly wasteful.

// Slots on all type objects
#define JS_DESCR_SLOT_SIZE                 0  // Prototype for instances, if any
#define JS_DESCR_SLOT_TYPROTO              1  // Prototype for instances, if any
#define JS_DESCR_SLOT_LENGTH               2  // See (*) and (**) below
#define JS_DESCR_SLOT_INNER_SHAPE          3

// Slots on scalars, references, and x4s
#define JS_DESCR_SLOT_TYPE                 4  // Type code

// Slots on array descriptors
#define JS_DESCR_SLOT_ARRAY_ELEM_TYPE      4

// Slots on struct type objects
#define JS_DESCR_SLOT_STRUCT_FIELD_NAMES   4
#define JS_DESCR_SLOT_STRUCT_FIELD_TYPES   5
#define JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS 6

// Maximum number of slots for any descriptor
#define JS_DESCR_SLOTS                     7

// These constants are for use exclusively in JS code. In C++ code,
// prefer TypeRepresentation::Scalar etc, which allows you to
// write a switch which will receive a warning if you omit a case.
#define JS_TYPE_SCALAR_KIND    0
#define JS_TYPE_REFERENCE_KIND 1
#define JS_TYPE_STRUCT_KIND    2
#define JS_TYPE_ARRAY_KIND     3
#define JS_TYPE_X4_KIND        4

// These constants are for use exclusively in JS code. In C++ code,
// prefer ScalarTypeRepresentation::TYPE_INT8 etc, which allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_SCALARTYPE_INT8          0
#define JS_SCALARTYPE_UINT8         1
#define JS_SCALARTYPE_INT16         2
#define JS_SCALARTYPE_UINT16        3
#define JS_SCALARTYPE_INT32         4
#define JS_SCALARTYPE_UINT32        5
#define JS_SCALARTYPE_FLOAT32       6
#define JS_SCALARTYPE_FLOAT64       7
#define JS_SCALARTYPE_UINT8_CLAMPED 8

// These constants are for use exclusively in JS code. In C++ code,
// prefer ReferenceTypeRepresentation::TYPE_ANY etc, which allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_REFERENCETYPE_ANY        0
#define JS_REFERENCETYPE_OBJECT     1
#define JS_REFERENCETYPE_STRING     2

// These constants are for use exclusively in JS code.  In C++ code,
// prefer X4TypeRepresentation::TYPE_INT32 etc, since that allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_X4TYPE_INT32         0
#define JS_X4TYPE_FLOAT32       1

///////////////////////////////////////////////////////////////////////////
// Slots for shapes

#define JS_SHAPE_SLOT_LENGTH       0
#define JS_SHAPE_SLOT_DIMS         1
#define JS_SHAPE_SLOT_TOTAL_ELEMS  2
#define JS_SHAPE_SLOT_INNER_SHAPE  3
#define JS_SHAPE_SLOTS             4

///////////////////////////////////////////////////////////////////////////
// Slots for typed objects

// Common to data view, typed arrays, and typed objects:
#define JS_BUFVIEW_SLOT_BYTEOFFSET       0
#define JS_BUFVIEW_SLOT_LENGTH           1 // see (*) below
#define JS_BUFVIEW_SLOT_OWNER            2
#define JS_BUFVIEW_SLOT_NEXT_VIEW        3

// Specific to data view:
#define JS_DATAVIEW_SLOTS                4 // Number of slots for data views

// Specific to typed arrays:
#define JS_TYPEDARR_SLOT_TYPE            5 // A ScalarTypeDescr::Type constant
#define JS_TYPEDARR_SLOTS                6 // Number of slots for typed arrays

// Specific to typed objects:
#define JS_TYPEDOBJ_SLOT_INNER_SHAPE     5 // see (**) below
#define JS_TYPEDOBJ_SLOTS                6 // Number of slots for typed objs

// Footnotes
//
// (*) The interpretation of the JS_BUFVIEW_SLOT_LENGTH slot depends on
// the kind of view:
// - DataView: stores the length in bytes
// - TypedArray: stores the array length
// - TypedObject: for arrays, stores the array length, else 0
//
// (**) In general, all type objects and typed objects store their
// dimensions (if any) using the slots LENGTH and INNER_SHAPE. These
// slots are uniformly included on all objects and descriptors, even
// non-arrays, so as to simplify various code paths which work across
// both arrays and normal objects.
//
// To understand how the slots work, let me begin by describing the
// most complex case (multidim arrays) and then proceed to the simpler
// cases (single dimensional arrays and finally non-arrays).
//
// In a multidim array, the LENGTH slot stores the outermost dimension
// and the INNER_SHAPE slot stores a pointer to a ShapeObject
// describing any higher dimensions. Shape objects are effectively a
// fancy cons list, which is convenient since it makes it cheap and
// easy to peel off an outer dimension.
//
// For example, if we had a type object like
// uint8.arrayType(Z).arrayType(Y).arrayType(X), it (and its
// instances) would have a length of X (the outermost dimension0 and a
// shape of [Y [Z]]. The outermost dimension is always stored in
// length so as to optimize the representation for single-dimensional
// arrays. It is also the most frequently used dimension for things
// like bounds checks and so forth, and hence it is important for it
// to be easily accessible.
//
// In a single dimensional array, there is no outer dimensions, so the
// INNER_SHAPE slot is simply null. This ensures that there is no need
// to allocate a shape object when creating single-dimensional arrays.
// So if the user does `uint8.array(N)`, this can create an array and
// just set LENGTH to N and INNER_SHAPE to null.
//
// For non-arrays, the LENGTH field is always 1 and INNER_SHAPE is
// null. Using 1 as the LENGTH for non-arrays is convenient because of
// how the link between typed prototypes and descriptors works.

#endif
