/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef GRPCPP_IMPL_CODEGEN_METADATA_MAP_H
#define GRPCPP_IMPL_CODEGEN_METADATA_MAP_H

#include <map>

#include <grpcpp/impl/codegen/slice.h>

namespace grpc {

namespace {
const char kBinaryErrorDetailsKey[] = "grpc-status-details-bin";
}

// TODO, split this up into MetadataSender and MetadataReciever?
//
// general purpose metadata container for both send and recv side. It acts a
// bit differently on both sides. On send the md is placed in as object, and is
// accessible as object until we convert it to the core representation of md.
//
// on the recv side the objects stay in the core format until they are
// requested by the application or interceptors. At that point they are
// lazily deserialized, cached, and put into the multimap.
class MetadataContainer {
 public:
  template <typename T>
  void AddMetadata(const grpc::string& key, T value) {
    metadata_.insert(std::make_pair(key, std::shared_ptr<SerializableConcept>(
                                             new SerializableModel<T>(value))));
  }
  template <typename T>
  bool GetMetadata(const grpc::string& key, const T** dst) {
    auto gotten = metadata_.find(key);
    if (gotten == metadata_.end()) {
      return false;
    }
    *dst = static_cast<T*>(gotten->second->object_);
    return true;
  }
  template <typename T>
  bool GetMutableMetadata(const grpc::string& key, T** dst) {
    auto gotten = metadata_.find(key);
    if (gotten == metadata_.end()) {
      return false;
    }
    *dst = static_cast<T*>(gotten->second->object_);
    return true;
  }

  // TODO(ncteisen) if the map is changed before we send, the pointers will be a
  // mess. Make sure it does not happen.
  grpc_metadata* CreateCoreMetadataArray(
      size_t* metadata_count,
      const grpc::string& optional_error_details) const {
    *metadata_count =
        metadata_.size() + (optional_error_details.empty() ? 0 : 1);
    if (*metadata_count == 0) {
      return nullptr;
    }
    grpc_metadata* metadata_array =
        (grpc_metadata*)(g_core_codegen_interface->gpr_malloc(
            (*metadata_count) * sizeof(grpc_metadata)));
    size_t i = 0;
    for (auto iter = metadata_.cbegin(); iter != metadata_.cend();
         ++iter, ++i) {
      metadata_array[i].key = SliceReferencingString(iter->first);
      Slice s;
      iter->second->Serialize(&s);
      metadata_array[i].value = s.c_slice();
    }
    if (!optional_error_details.empty()) {
      metadata_array[i].key =
          g_core_codegen_interface->grpc_slice_from_static_buffer(
              kBinaryErrorDetailsKey, sizeof(kBinaryErrorDetailsKey) - 1);
      metadata_array[i].value = SliceReferencingString(optional_error_details);
    }
    return metadata_array;
  }

 private:
  // Concept of something that is serializable.
  class SerializableConcept {
   public:
    SerializableConcept(void* object) : object_(object) {}
    virtual Status Serialize(Slice* slice) = 0;
    // TODO
    // virtual Status Extract();
    // on send side, non owning pointer to object.
    // on recv side, allocated, owned pointer to object.
    void* object_;
  };

  // Model to hold the actual medata value.
  template <typename T>
  class SerializableModel : public SerializableConcept {
   public:
    SerializableModel(T object)
        : SerializableConcept(static_cast<void*>(&object_)), object_(object) {}

    Status Serialize(Slice* slice) override { return object_.Serialize(slice); }

   private:
    // TODO, change back to shared ptr?
    T object_;
  };

 public:
  std::multimap<grpc::string, std::shared_ptr<SerializableConcept>> metadata_;
};

class StringMetadataValue {
 public:
  StringMetadataValue(grpc::string str) : str_(str) {}
  Status Serialize(Slice* slice) const {
    Slice s(str_);
    slice->Swap(&s);
    return Status::OK;
  }
  grpc::string str_;
};

namespace internal {
class MetadataMap {
 public:
  MetadataMap() { memset(&arr_, 0, sizeof(arr_)); }

  ~MetadataMap() {
    g_core_codegen_interface->grpc_metadata_array_destroy(&arr_);
  }

  void FillMap() {
    for (size_t i = 0; i < arr_.count; i++) {
      // TODO(yangg) handle duplicates?
      map_.insert(std::pair<grpc::string_ref, grpc::string_ref>(
          StringRefFromSlice(&arr_.metadata[i].key),
          StringRefFromSlice(&arr_.metadata[i].value)));
    }
  }

  std::multimap<grpc::string_ref, grpc::string_ref>* map() { return &map_; }
  const std::multimap<grpc::string_ref, grpc::string_ref>* map() const {
    return &map_;
  }
  grpc_metadata_array* arr() { return &arr_; }

 private:
  grpc_metadata_array arr_;
  std::multimap<grpc::string_ref, grpc::string_ref> map_;
};
}  // namespace internal

}  // namespace grpc

#endif  // GRPCPP_IMPL_CODEGEN_METADATA_MAP_H
