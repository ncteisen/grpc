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

// TODO, split this up into MetadataSender and MetadataReciever?
//
// general purpose metadata container for both send and recv side. It acts a
// bit differently on both sides. On send the md is placed in as object, and is
// accessible as object until we convert it to the core representation of md.
// 
class MetadataContainer {
 public:
  template <typename T>
  void AddMetadata(const grpc::string& key, T value) {
    metadata_.insert(std::make_pair(key, std::shared_ptr<SerializableConcept>(
                                             new SerializableModel<T>(value))));
  }
  template <typename T>
  bool GetMetadata(const grpc::string& key, const T* dst) {
    auto gotten = metadata_.find(key);
    if (gotten == metadata_.end()) {
      return false;
    }
    dst = gotten;
    return true;
  }
  template <typename T>
  bool GetMutableMetadata(const grpc::string& key, T* dst) {
    auto gotten = metadata_.find(key);
    if (gotten == metadata_.end()) {
      return false;
    }
    dst = gotten;
    return true;
  }

 private:
  // Concept of something that is serializable.
  class SerializableConcept {
   public:
    virtual Status Serialize(Slice* slice) = 0;
  };

  // Model to hold the actual medata value.
  template <typename T>
  class SerializableModel : public SerializableConcept {
   public:
    SerializableModel(T object) : object_(object) {}

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

 private:
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
