/*
 *
 * Copyright 2017 gRPC authors.
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

#ifndef GRPC_CORE_LIB_SUPPORT_FUNCTION_H
#define GRPC_CORE_LIB_SUPPORT_FUNCTION_H

#include <stddef.h>
#include <memory>
#include <type_traits>
#include <utility>

namespace grpc_core {

namespace function_impl {
static constexpr const size_t kDefaultInplaceStorage = 3 * sizeof(void*);

template <typename R, typename... Args>
struct VTable {
  void (*copy_construct)(const void* from, void* to);
  void (*move_construct)(void* from, void* to);
  void (*destruct)(void* storage);
  R (*invoke)(void* storage, Args&&... args);
};

template <typename T, typename F, bool kIsLarge>
class Traits;

template <typename R, typename... Args, typename F>
class Traits<R(Args...), F, false> {
 public:
  static const VTable<R, Args...>* Construct(F&& f, void* storage) {
    new (storage) F(std::move(f));
    return &vtable_;
  }

 private:
  static void CopyConstruct(const void* from, void* to) {
    new (to) F(*static_cast<const F*>(from));
  }
  static void MoveConstruct(void* from, void* to) {
    new (to) F(std::move(*static_cast<F*>(from)));
  }
  static void Destruct(void* storage) { static_cast<F*>(storage)->~F(); }
  static R Invoke(void* storage, Args&&... args) {
    return (*static_cast<F*>(storage))(std::forward<Args>(args)...);
  }

  static const VTable<R, Args...> vtable_;
};

template <typename R, typename... Args, typename F>
const VTable<R, Args...> Traits<R(Args...), F, false>::vtable_ = {
    CopyConstruct, MoveConstruct, Destruct, Invoke};

template <typename R, typename... Args, typename F>
class Traits<R(Args...), F, true> {
 private:
  typedef std::unique_ptr<F> Ptr;

 public:
  static const VTable<R, Args...>* Construct(F&& f, void* storage) {
    new (storage) Ptr(new F(std::move(f)));
    return &vtable_;
  }

 private:
  static void CopyConstruct(const void* from, void* to) {
    new (to) Ptr(new F(*static_cast<Ptr*>(from)->get()));
  }
  static void MoveConstruct(void* from, void* to) {
    new (to) Ptr(std::move(*static_cast<Ptr*>(from)));
  }
  static void Destruct(void* storage) { static_cast<Ptr*>(storage)->~Ptr(); }
  static R Invoke(void* storage, Args&&... args) {
    return (**static_cast<Ptr*>(storage))(std::forward<Args>(args)...);
  }

  static const VTable<R, Args...> vtable_;
};

template <typename R, typename... Args, typename F>
const VTable<R, Args...> Traits<R(Args...), F, true>::vtable_ = {
    CopyConstruct, MoveConstruct, Destruct, Invoke};
}

template <typename T,
          size_t kInplaceStorage = function_impl::kDefaultInplaceStorage>
class Function;

template <typename T,
          size_t kInplaceStorage = function_impl::kDefaultInplaceStorage>
class InplaceFunction;

template <typename T,
          size_t kInplaceStorage = function_impl::kDefaultInplaceStorage>
class TrivialInplaceFunction;

// Function<> is roughly a std::function work-alike
// Differences:
// - configurable inplace storage quantity... functors smaller than this incur
//   no allocation overhead, functors bigger will cause an allocation
template <typename R, typename... Args, size_t kInplaceStorage>
class Function<R(Args...), kInplaceStorage> {
 public:
  template <class F>
  Function(F&& f) {
    vtable_ =
        function_impl::Traits<R(Args...), F, (sizeof(F) > kInplaceStorage)>::
            Construct(std::forward<F>(f), &storage_);
  }

  Function(const Function& rhs) : vtable_(rhs.vtable_) {
    vtable_->copy_construct(&rhs.storage_, &storage_);
  }

  Function(Function& rhs) : vtable_(rhs.vtable_) {
    vtable_->copy_construct(&rhs.storage_, &storage_);
  }

  Function(Function&& rhs) : vtable_(rhs.vtable_) {
    vtable_->move_construct(&rhs.storage_, &storage_);
  }

  ~Function() { vtable_->destruct(&storage_); }

  Function& operator=(const Function& rhs) {
    if (this == &rhs) return *this;
    vtable_->destruct(&storage_);
    vtable_ = rhs.vtable_;
    vtable_->copy_construct(&rhs.storage_, &storage_);
    return *this;
  }

  Function& operator=(Function&& rhs) {
    if (this == &rhs) return *this;
    vtable_->destruct(&storage_);
    vtable_ = rhs.vtable_;
    vtable_->move_construct(&rhs.storage_, &storage_);
    return *this;
  }

  R operator()(Args&&... args) {
    return vtable_->invoke(&storage_, std::forward<Args>(args)...);
  }

 private:
  const function_impl::VTable<R, Args...>* vtable_;
  std::aligned_storage<kInplaceStorage> storage_;
};

// InplaceFunction<> is like Function<>, but causes a compile time error if the
// contained functor is bigger than kInplaceStorage
// This class is guaranteed never to allocate
template <typename R, typename... Args, size_t kInplaceStorage>
class InplaceFunction<R(Args...), kInplaceStorage> {
 public:
  template <class F>
  InplaceFunction(F&& f) {
    static_assert(
        sizeof(F) <= kInplaceStorage,
        "InplaceFunction functor must be smaller than kInplaceStorage");
    vtable_ = function_impl::Traits<R(Args...), F, false>::Construct(
        std::forward<F>(f), &storage_);
  }

  InplaceFunction(const InplaceFunction& rhs) : vtable_(rhs.vtable_) {
    vtable_->copy_construct(&rhs.storage_, &storage_);
  }

  InplaceFunction(InplaceFunction& rhs) : vtable_(rhs.vtable_) {
    vtable_->copy_construct(&rhs.storage_, &storage_);
  }

  InplaceFunction(InplaceFunction&& rhs) : vtable_(rhs.vtable_) {
    vtable_->move_construct(&rhs.storage_, &storage_);
  }

  ~InplaceFunction() { vtable_->destruct(&storage_); }

  InplaceFunction& operator=(const InplaceFunction& rhs) {
    if (this == &rhs) return *this;
    vtable_->destruct(&storage_);
    vtable_ = rhs.vtable_;
    vtable_->copy_construct(&rhs.storage_, &storage_);
    return *this;
  }

  InplaceFunction& operator=(InplaceFunction&& rhs) {
    if (this == &rhs) return *this;
    vtable_->destruct(&storage_);
    vtable_ = rhs.vtable_;
    vtable_->move_construct(&rhs.storage_, &storage_);
    return *this;
  }

  R operator()(Args&&... args) {
    return vtable_->invoke(&storage_, std::forward<Args>(args)...);
  }

 private:
  const function_impl::VTable<R, Args...>* vtable_;
  std::aligned_storage<kInplaceStorage> storage_;
};

// TrivialInplaceFunction<> is like InplaceFunction<>, but causes a compile time
// error if the contained functor is not trivial (meaning memcpy-able members)
// Given the functor is trivial, take advantage of this to provide a more
// efficient implementation
template <typename R, typename... Args, size_t kInplaceStorage>
class TrivialInplaceFunction<R(Args...), kInplaceStorage> {
 public:
  template <class F>
  TrivialInplaceFunction(F&& f) {
    static_assert(
        sizeof(F) <= kInplaceStorage,
        "TrivialInplaceFunction functor must be smaller than kInplaceStorage");
    static_assert(std::is_trivial<F>::value,
                  "TrivialInplaceFunction functor must be trivial");
    invoke_ = function_impl::Traits<R(Args...), F, false>::Construct(
                  std::forward<F>(f), &storage_)
                  ->invoke;
  }

  TrivialInplaceFunction(const TrivialInplaceFunction&) = default;
  TrivialInplaceFunction(TrivialInplaceFunction&) = default;
  TrivialInplaceFunction(TrivialInplaceFunction&&) = default;

  R operator()(Args&&... args) {
    return invoke_(&storage_, std::forward<Args>(args)...);
  }

 private:
  R (*invoke_)(void* storage, Args... args);
  std::aligned_storage<kInplaceStorage> storage_;
};

}  // namespace grpc_core

#endif  // GRPC_CORE_LIB_SUPPORT_FUNCTION_H
