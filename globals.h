#pragma once

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

typedef uintptr_t uword;
static const int kWordSize = sizeof(uword);
static const int kBitsPerByte = 8;
static const int64_t kMinInt64 = INT64_MIN;
static const int32_t kMinInt32 = INT32_MIN;
static const int kKiB = 1000;
static const int kMiB = 1000 * kKiB;

// A macro to disallow the copy constructor and operator= functions.
// This should be used in the private: declarations for a class.
#if !defined(DISALLOW_COPY_AND_ASSIGN)
#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                     \
private:                                                                       \
  TypeName(const TypeName &) = delete;                                         \
  void operator=(const TypeName &) = delete
#endif // !defined(DISALLOW_COPY_AND_ASSIGN)

// A macro to disallow all the implicit constructors, namely the default
// constructor, copy constructor and operator= functions. This should be
// used in the private: declarations for a class that wants to prevent
// anyone from instantiating it. This is especially useful for classes
// containing only static methods.
#if !defined(DISALLOW_IMPLICIT_CONSTRUCTORS)
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName)                               \
private:                                                                       \
  TypeName() = delete;                                                         \
  DISALLOW_COPY_AND_ASSIGN(TypeName)
#endif // !defined(DISALLOW_IMPLICIT_CONSTRUCTORS)

#define FATAL(msg)                                                             \
  fprintf(stderr, "fatal: %s\n", msg);                                         \
  abort();

#define UNREACHABLE()                                                          \
  fprintf(stderr, "unreachable code\n");                                       \
  abort();

#define UNIMPLEMENTED()                                                        \
  fprintf(stderr, "unimplemented code\n");                                     \
  abort();

// Macro to disallow allocation in the C++ heap. This should be used
// in the private section for a class. Don't use UNREACHABLE here to
// avoid circular dependencies between platform/globals.h and
// platform/assert.h.
#if !defined(DISALLOW_ALLOCATION)
#define DISALLOW_ALLOCATION()                                                  \
public:                                                                        \
  void operator delete(void *pointer) { UNREACHABLE() }                        \
                                                                               \
private:                                                                       \
  void *operator new(size_t size);
#endif // !defined(DISALLOW_ALLOCATION)

#define ASSERT assert

// Stack allocated objects subclass from this base class. Objects of this type
// cannot be allocated on either the C or object heaps. Destructors for objects
// of this type will not be run unless the stack is unwound through normal
// program control flow.
class ValueObject {
public:
  ValueObject() {}
  ~ValueObject() {}

private:
  DISALLOW_ALLOCATION();
  DISALLOW_COPY_AND_ASSIGN(ValueObject);
};

class Utils {
public:
  template <typename T> static inline T Minimum(T x, T y) {
    return x < y ? x : y;
  }

  // Check whether an N-bit two's-complement representation can hold value.
  template <typename T> static inline bool IsInt(int N, T value) {
    ASSERT((0 < N) &&
           (static_cast<unsigned int>(N) < (kBitsPerByte * sizeof(value))));
    T limit = static_cast<T>(1) << (N - 1);
    return (-limit <= value) && (value < limit);
  }

  template <typename T> static inline bool IsUint(int N, T value) {
    ASSERT((0 < N) &&
           (static_cast<unsigned int>(N) < (kBitsPerByte * sizeof(value))));
    const auto limit =
        (static_cast<typename std::make_unsigned<T>::type>(1) << N) - 1;
    return (0 <= value) &&
           (static_cast<typename std::make_unsigned<T>::type>(value) <= limit);
  }

  template <typename T> static inline bool IsPowerOfTwo(T x) {
    return ((x & (x - 1)) == 0) && (x != 0);
  }
};

// Similar to bit_cast, but allows copying from types of unrelated
// sizes. This method was introduced to enable the strict aliasing
// optimizations of GCC 4.4. Basically, GCC mindlessly relies on
// obscure details in the C++ standard that make reinterpret_cast
// virtually useless.
template <class D, class S> inline D bit_copy(const S &source) {
  D destination;
  // This use of memcpy is safe: source and destination cannot overlap.
  memcpy(&destination, reinterpret_cast<const void *>(&source),
         sizeof(destination));
  return destination;
}
