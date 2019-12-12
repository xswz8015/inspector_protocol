// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRDTP_SERIALIZER_TRAITS_H_
#define CRDTP_SERIALIZER_TRAITS_H_

#include <memory>
#include <string>
#include <vector>
#include "cbor.h"
#include "serializable.h"
#include "span.h"

namespace crdtp {
// =============================================================================
// SerializerTraits - Helps encode field values of protocol objects in CBOR.
// =============================================================================
//
// A family of serialization functions which are used by the code generator
// for the inspector protocol bindings to encode field values in CBOR.
// Conceptually, it's this:
//
// Serialize(bool value, std::vector<uint8_t>* out);
// Serialize(int32_t value, std::vector<uint8_t>* out);
// Serialize(double value, std::vector<uint8_t>* out);
// ...
//
// However, if this was to use straight-forward overloading, implicit
// type conversions would lead to ambiguity - e.g., a bool could be
// represented as an int32_t, but it should really be encoded as a bool.
// The template parameterized / specialized structs accomplish this.
//
// SerializerTraits<bool>::Serialize(bool value, std::vector<uint8_t>* out);
// SerializerTraits<int32>::Serialize(int32_t value, std::vector<uint8_t>* out);
// SerializerTraits<double>::Serialize(double value, std::vector<uint8_t>* out);
//
// Includes convenience methods for dereferencing raw and unique_ptr's.
template <typename T>
struct SerializerTraits {
  // |Serializable| (defined in serializable.h) already knows how to serialize
  // to CBOR, so we can just delegate. This covers domain specific types,
  // protocol::Binary, etc.
  static void Serialize(const Serializable& value, std::vector<uint8_t>* out) {
    value.AppendSerialized(out);
  }

  // Convenience.
  static void Serialize(const T* value, std::vector<uint8_t>* out) {
    SerializerTraits<T>::Serialize(*value, out);
  }

  // Convenience.
  static void Serialize(const std::unique_ptr<T>& value,
                        std::vector<uint8_t>* out) {
    SerializerTraits<T>::Serialize(*value, out);
  }

  // This method covers the Exported types, e.g. from V8 into Chromium.
  // TODO(johannes): Change Exported signature to AppendSerialized
  // for consistency with Serializable; this is why we explicitly
  // disable this template for Serializable here.
  template <typename Exported,
            typename std::enable_if_t<
                std::is_member_pointer<decltype(&Exported::writeBinary)>{} &&
                    !std::is_same<Serializable, T>{},
                int> = 0>
  static void Serialize(const Exported& value, std::vector<uint8_t>* out) {
    value.writeBinary(out);
  }
};

// This covers std::string, which is assumed to be UTF-8.
// The two other string implementations that are used in the protocol bindings:
// - WTF::String, for which the SerializerTraits specialization is located
//   in third_party/blink/renderer/core/inspector/v8-inspector-string.h.
// - v8_inspector::String16, implemented in v8/src/inspector/string-16.h
//   along with its SerializerTraits specialization.
template <>
struct SerializerTraits<std::string> {
  static void Serialize(const std::string& str, std::vector<uint8_t>* out) {
    cbor::EncodeString8(SpanFrom(str), out);
  }
};

template <>
struct SerializerTraits<bool> {
  static void Serialize(bool value, std::vector<uint8_t>* out) {
    out->push_back(value ? cbor::EncodeTrue() : cbor::EncodeFalse());
  }
};

template <>
struct SerializerTraits<int32_t> {
  static void Serialize(int32_t value, std::vector<uint8_t>* out) {
    cbor::EncodeInt32(value, out);
  }
};

template <>
struct SerializerTraits<double> {
  static void Serialize(double value, std::vector<uint8_t>* out) {
    cbor::EncodeDouble(value, out);
  }
};

template <typename T>
struct SerializerTraits<std::vector<T>> {
  static void Serialize(const std::vector<T>& value,
                        std::vector<uint8_t>* out) {
    out->push_back(cbor::EncodeIndefiniteLengthArrayStart());
    for (const T& element : value)
      SerializerTraits<T>::Serialize(element, out);
    out->push_back(cbor::EncodeStop());
  }

  // Convenience.
  static void Serialize(const std::vector<T>* value,
                        std::vector<uint8_t>* out) {
    SerializerTraits<std::vector<T>>::Serialize(*value, out);
  }

  // Convenience.
  static void Serialize(const std::unique_ptr<std::vector<T>>& value,
                        std::vector<uint8_t>* out) {
    SerializerTraits<std::vector<T>>::Serialize(*value, out);
  }
};

// Convenience, esp. for elements of std::vector<std::unique_ptr<T>>.
template <typename T>
struct SerializerTraits<std::unique_ptr<T>> {
  static void Serialize(const std::unique_ptr<T>& value,
                        std::vector<uint8_t>* out) {
    SerializerTraits<T>::Serialize(*value, out);
  }
};
}  // namespace crdtp

#endif  // CRDTP_SERIALIZER_TRAITS_H_
