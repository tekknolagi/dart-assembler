// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "assembler.h"
#include "globals.h"

static uword NewContents(intptr_t capacity) {
  uword result = reinterpret_cast<uword>(malloc(capacity));
#if defined(DEBUG)
  // Initialize the buffer with kBreakPointInstruction to force a break
  // point if we ever execute an uninitialized part of the code buffer.
  Assembler::InitializeMemoryWithBreakpoints(result, capacity);
#endif
  return result;
}

AssemblerBuffer::AssemblerBuffer() {
  static const intptr_t kInitialBufferCapacity = 4 * kKiB;
  contents_ = NewContents(kInitialBufferCapacity);
  cursor_ = contents_;
  limit_ = ComputeLimit(contents_, kInitialBufferCapacity);
#if defined(DEBUG)
  has_ensured_capacity_ = false;
  fixups_processed_ = false;
#endif

  // Verify internal state.
  ASSERT(Capacity() == kInitialBufferCapacity);
  ASSERT(Size() == 0);
}

AssemblerBuffer::~AssemblerBuffer() {}

void AssemblerBuffer::ExtendCapacity() {
  intptr_t old_size = Size();
  intptr_t old_capacity = Capacity();
  intptr_t new_capacity =
      Utils::Minimum(old_capacity * 2, old_capacity + 1 * kMiB);
  if (new_capacity < old_capacity) {
    FATAL("Unexpected overflow in AssemblerBuffer::ExtendCapacity");
  }

  // Allocate the new data area and copy contents of the old one to it.
  uword new_contents = NewContents(new_capacity);
  memmove(reinterpret_cast<void *>(new_contents),
          reinterpret_cast<void *>(contents_), old_size);

  // Compute the relocation delta and switch to the new contents area.
  intptr_t delta = new_contents - contents_;
  contents_ = new_contents;

  // Update the cursor and recompute the limit.
  cursor_ += delta;
  limit_ = ComputeLimit(new_contents, new_capacity);

  // Verify internal state.
  ASSERT(Capacity() == new_capacity);
  ASSERT(Size() == old_size);
}

// Shared macros are implemented here.
void AssemblerBase::Unimplemented(const char *message) {
  const char *format = "Unimplemented: %s";
  const intptr_t len = snprintf(NULL, 0, format, message);
  char *buffer = reinterpret_cast<char *>(malloc(len + 1));
  snprintf(buffer, len + 1, format, message);
  Stop(buffer);
}

void AssemblerBase::Untested(const char *message) {
  const char *format = "Untested: %s";
  const intptr_t len = snprintf(NULL, 0, format, message);
  char *buffer = reinterpret_cast<char *>(malloc(len + 1));
  snprintf(buffer, len + 1, format, message);
  Stop(buffer);
}

void AssemblerBase::Unreachable(const char *message) {
  const char *format = "Unreachable: %s";
  const intptr_t len = snprintf(NULL, 0, format, message);
  char *buffer = reinterpret_cast<char *>(malloc(len + 1));
  snprintf(buffer, len + 1, format, message);
  Stop(buffer);
}

void Assembler::Stop(const char *message) {
  (void)message;
  Breakpoint();
}
