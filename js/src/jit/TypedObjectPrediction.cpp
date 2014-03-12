/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TypedObjectPrediction.h"

void
TypedObjectPrediction::findPrefix(const StructTypeDescr &descrA,
                                  const StructTypeDescr &descrB,
                                  size_t max)
{
    JS_ASSERT(predictionKind() == Prefix);

    // count is the number of fields in common. It begins as the min
    // of the number of fields from descrA, descrB, and max, and then
    // is decremented as we find uncommon fields.
    if (max > descrA.fieldCount())
        max = descrA.fieldCount();
    if (max > descrB.fieldCount())
        max = descrB.fieldCount();

    size_t i = 0;
    for (; i < max; i++) {
        if (&descrA.fieldName(i) != &descrB.fieldName(i))
            break;
        if (&descrA.fieldDescr(i) != &descrB.fieldDescr(i))
            break;
        JS_ASSERT(descrA.fieldOffset(i) == descrB.fieldOffset(i));
    }

    setPrefix(descrA, i);
}

void
TypedObjectPrediction::addProto(const TypedProto &proto)
{
    switch (predictionKind()) {
      case Empty:
        return setProto(proto);

      case Inconsistent:
        return; // keep same state

      case Proto:
        if (&proto == data_.proto)
            return; // keep same state

        if (proto.typeDescr().kind() != data_.proto->typeDescr().kind())
            return inconsistent();

        if (proto.typeDescr().kind() != TypeDescr::Struct)
            return inconsistent();

        findPrefix(data_.proto->typeDescr().as<StructTypeDescr>(),
                   proto.typeDescr().as<StructTypeDescr>(),
                   SIZE_MAX);
        return;

      case Descr:
        // First downgrade from descr to proto, which is less precise,
        // and then recurse.
        setProto(data_.descr->typedProto());
        return addProto(proto);

      case Prefix:
        if (proto.typeDescr().kind() != TypeDescr::Struct)
            return inconsistent();

        findPrefix(*data_.prefix.descr,
                   proto.typeDescr().as<StructTypeDescr>(),
                   data_.prefix.fields);
        return;
    }

    MOZ_ASSUME_UNREACHABLE("Bad predictionKind");
}

TypeDescr::Kind
TypedObjectPrediction::kind() const
{
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        return proto().typeDescr().kind();

      case TypedObjectPrediction::Descr:
        return descr().kind();

      case TypedObjectPrediction::Prefix:
        return prefix().descr->kind();
    }

    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

bool
TypedObjectPrediction::ofArrayKind() const
{
    switch (kind()) {
      case TypeDescr::Scalar:
      case TypeDescr::Reference:
      case TypeDescr::X4:
      case TypeDescr::Struct:
        return false;

      case TypeDescr::SizedArray:
      case TypeDescr::UnsizedArray:
        return true;
    }

    MOZ_ASSUME_UNREACHABLE("Bad kind");
}

static bool
DescrHasKnownSize(const TypeDescr &descr, int32_t *out)
{
    if (!descr.is<SizedTypeDescr>())
        return false;

    *out = descr.as<SizedTypeDescr>().size();
    return true;
}

bool
TypedObjectPrediction::hasKnownSize(int32_t *out) const
{
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        // In later patches, this will be different, since prototypes
        // will never track the length.
        return DescrHasKnownSize(proto().typeDescr(), out);

      case TypedObjectPrediction::Descr:
        // In later patches, this will be different, since descriptors
        // will always track the length.
        return DescrHasKnownSize(descr(), out);

      case TypedObjectPrediction::Prefix:
        // We only know a prefix of the struct fields, hence we do not
        // know its complete size.
        return false;
    }

    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

template<typename T>
typename T::Type
TypedObjectPrediction::extractType() const
{
    JS_ASSERT(kind() == T::Kind);
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        return proto().typeDescr().as<T>().type();

      case TypedObjectPrediction::Descr:
        return descr().as<T>().type();

      case TypedObjectPrediction::Prefix:
        break; // Prefixes are always structs, never scalars etc
    }

    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

ScalarTypeDescr::Type
TypedObjectPrediction::scalarType() const
{
    return extractType<ScalarTypeDescr>();
}

ReferenceTypeDescr::Type
TypedObjectPrediction::referenceType() const
{
    return extractType<ReferenceTypeDescr>();
}

X4TypeDescr::Type
TypedObjectPrediction::x4Type() const
{
    return extractType<X4TypeDescr>();
}

bool
TypedObjectPrediction::hasKnownArrayLength(int32_t *length) const
{
    JS_ASSERT(ofArrayKind());
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        // In later patches, it will be the case that prototypes never
        // track the length, so let's just make our lives easy and
        // start this code out the right way. (I'll remove this
        // comment later, I swear!)
        return false;

      case TypedObjectPrediction::Descr:
        // In later patches, this condition will always be true
        // so long as this represents an array
        if (descr().is<SizedArrayTypeDescr>()) {
            *length = descr().as<SizedArrayTypeDescr>().length();
            return true;
        }
        return false;

      case TypedObjectPrediction::Prefix:
        break; // Prefixes are always structs, never arrays
    }
    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

static TypedObjectPrediction
DescrElementType(const TypeDescr &descr)
{
    if (!descr.is<SizedArrayTypeDescr>()) {
        return TypedObjectPrediction(descr.as<SizedArrayTypeDescr>().elementType());
    } else {
        return TypedObjectPrediction(descr.as<UnsizedArrayTypeDescr>().elementType());
    }
}

TypedObjectPrediction
TypedObjectPrediction::arrayElementType() const
{
    JS_ASSERT(ofArrayKind());
    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        // In later patches, this will be different from Descr below.
        return DescrElementType(proto().typeDescr());

      case TypedObjectPrediction::Descr:
        // In later patches, this will be different from Proto above.
        return DescrElementType(descr());

      case TypedObjectPrediction::Prefix:
        break; // Prefixes are always structs, never arrays
    }
    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}

bool
TypedObjectPrediction::hasFieldNamedPrefix(const StructTypeDescr &descr,
                                           size_t fieldCount,
                                           jsid id,
                                           int32_t *offset,
                                           TypedObjectPrediction *out,
                                           size_t *index) const
{
    // Initialize `*offset` and `*out` for the case where incompatible
    // or absent fields are found.
    *offset = SIZE_MAX;
    *index = SIZE_MAX;
    *out = TypedObjectPrediction();

    // Find the index of the field `id` if any.
    if (!descr.fieldIndex(id, index))
        return false;

    // Check whether the index falls within our known safe prefix.
    if (*index >= fieldCount)
        return false;

    // Load the offset and type.
    *offset = descr.fieldOffset(*index);
    *out = TypedObjectPrediction(descr.fieldDescr(*index));
    return true;
}

bool
TypedObjectPrediction::hasFieldNamed(jsid id,
                                     int32_t *fieldOffset,
                                     TypedObjectPrediction *fieldType,
                                     size_t *fieldIndex) const
{
    JS_ASSERT(kind() == TypeDescr::Struct);

    switch (predictionKind()) {
      case TypedObjectPrediction::Empty:
      case TypedObjectPrediction::Inconsistent:
        break;

      case TypedObjectPrediction::Proto:
        return hasFieldNamedPrefix(
            proto().typeDescr().as<StructTypeDescr>(), SIZE_MAX,
            id, fieldOffset, fieldType, fieldIndex);

      case TypedObjectPrediction::Descr:
        return hasFieldNamedPrefix(
            descr().as<StructTypeDescr>(), SIZE_MAX,
            id, fieldOffset, fieldType, fieldIndex);

      case TypedObjectPrediction::Prefix:
        return hasFieldNamedPrefix(
            *prefix().descr, prefix().fields,
            id, fieldOffset, fieldType, fieldIndex);
    }
    MOZ_ASSUME_UNREACHABLE("Bad prediction kind");
}
