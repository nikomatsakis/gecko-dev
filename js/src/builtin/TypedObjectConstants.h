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

// Slots on scalars, references, and x4s
#define JS_DESCR_SLOT_TYPE                 2  // Type code

// Slots on all array descriptors
#define JS_DESCR_SLOT_ARRAY_ELEM_TYPE      2

// Slots on sized array descriptors
#define JS_DESCR_SLOT_SIZED_ARRAY_LENGTH   3

// Slots on struct type objects
#define JS_DESCR_SLOT_STRUCT_FIELD_NAMES   2
#define JS_DESCR_SLOT_STRUCT_FIELD_TYPES   3
#define JS_DESCR_SLOT_STRUCT_FIELD_OFFSETS 4

// Maximum number of slots for any descriptor
#define JS_DESCR_SLOTS                     5

// These constants are for use exclusively in JS code. In C++ code,
// prefer TypeRepresentation::Scalar etc, which allows you to
// write a switch which will receive a warning if you omit a case.
#define JS_TYPEREPR_UNSIZED_ARRAY_KIND  0
#define JS_TYPEREPR_MAX_UNSIZED_KIND    0    // Unsized kinds go above here
#define JS_TYPEREPR_SCALAR_KIND         1
#define JS_TYPEREPR_REFERENCE_KIND      2
#define JS_TYPEREPR_STRUCT_KIND         3
#define JS_TYPEREPR_SIZED_ARRAY_KIND    4
#define JS_TYPEREPR_X4_KIND             5

// These constants are for use exclusively in JS code. In C++ code,
// prefer ScalarTypeRepresentation::TYPE_INT8 etc, which allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_SCALARTYPEREPR_INT8          0
#define JS_SCALARTYPEREPR_UINT8         1
#define JS_SCALARTYPEREPR_INT16         2
#define JS_SCALARTYPEREPR_UINT16        3
#define JS_SCALARTYPEREPR_INT32         4
#define JS_SCALARTYPEREPR_UINT32        5
#define JS_SCALARTYPEREPR_FLOAT32       6
#define JS_SCALARTYPEREPR_FLOAT64       7
#define JS_SCALARTYPEREPR_UINT8_CLAMPED 8

// These constants are for use exclusively in JS code. In C++ code,
// prefer ReferenceTypeRepresentation::TYPE_ANY etc, which allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_REFERENCETYPEREPR_ANY        0
#define JS_REFERENCETYPEREPR_OBJECT     1
#define JS_REFERENCETYPEREPR_STRING     2

// These constants are for use exclusively in JS code.  In C++ code,
// prefer X4TypeRepresentation::TYPE_INT32 etc, since that allows
// you to write a switch which will receive a warning if you omit a
// case.
#define JS_X4TYPEREPR_INT32         0
#define JS_X4TYPEREPR_FLOAT32       1

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
#define JS_TYPEDOBJ_SLOTS                5 // Number of slots for typed objs

// (*) The interpretation of the JS_BUFVIEW_SLOT_LENGTH slot depends on
// the kind of view:
// - DataView: stores the length in bytes
// - TypedArray: stores the array length
// - TypedObject: for arrays, stores the array length, else 0

#endif
