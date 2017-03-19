// -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-
// Copyright (c) 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
// Author: Sanjay Ghemawat <opensource@google.com>

#include <config.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>                   // for PRIuPTR
#endif
#include <errno.h>                      // for ENOMEM, errno
#include <gperftools/malloc_extension.h>      // for MallocRange, etc
#include "base/basictypes.h"
#include "base/commandlineflags.h"
#include "internal_logging.h"  // for ASSERT, TCMalloc_Printer, etc
#include "page_heap_allocator.h"  // for PageHeapAllocator
#include "static_vars.h"       // for Static
#include "system-alloc.h"      // for TCMalloc_SystemAlloc, etc

namespace tcmalloc {

static const size_t kForcedCoalesceInterval = 128*1024*1024;

Span* MetadataPageHeap::New(Length n, size_t ac) {
  ASSERT(n > 0);
  if (n > (METADATAOFFSET >> kPageShift)) {
    Log(kCrash, __FILE__, __LINE__,
        "tcmalloc-metadata: requested allocation too large", n, METADATAOFFSET >> kPageShift);
  } 

  Span* result = list_[ac].SearchFreeAndLargeLists(n);
  if (result != NULL) return result;

  PageHeap::Stats lStats = list_[ac].stats();

  if (lStats.free_bytes != 0 && lStats.unmapped_bytes != 0
      && lStats.free_bytes + lStats.unmapped_bytes >= lStats.system_bytes / 4
      && (lStats.system_bytes / kForcedCoalesceInterval
          != (lStats.system_bytes + (n << kPageShift)) / kForcedCoalesceInterval)) {
    // We're about to grow heap, but there are lots of free pages.
    // tcmalloc's design decision to keep unmapped and free spans
    // separately and never coalesce them means that sometimes there
    // can be free pages span of sufficient size, but it consists of
    // "segments" of different type so page heap search cannot find
    // it. In order to prevent growing heap and wasting memory in such
    // case we're going to unmap all free pages. So that all free
    // spans are maximally coalesced.
    //
    // We're also limiting 'rate' of going into this path to be at
    // most once per 128 megs of heap growth. Otherwise programs that
    // grow heap frequently (and that means by small amount) could be
    // penalized with higher count of minor page faults.
    //
    // See also large_heap_fragmentation_unittest.cc and
    // https://code.google.com/p/gperftools/issues/detail?id=368
    list_[ac].ReleaseAtLeastNPages(static_cast<Length>(0x7fffffff));

    // then try again. If we are forced to grow heap because of large
    // spans fragmentation and not because of problem described above,
    // then at the very least we've just unmapped free but
    // insufficiently big large spans back to OS. So in case of really
    // unlucky memory fragmentation we'll be consuming virtual address
    // space, but not real memory
    result = list_[ac].SearchFreeAndLargeLists(n);
    if (result != NULL) return result;
  }

  // Grow the heap and try again.
  if (!list_[ac].GrowHeap(ARENASIZE >> kPageShift)) {
    // underlying SysAllocator likely set ENOMEM but we can get here
    // due to EnsureLimit so we set it here too.
    //
    // Setting errno to ENOMEM here allows us to avoid dealing with it
    // in fast-path.
    errno = ENOMEM;
    return NULL;
  }
  return list_[ac].SearchFreeAndLargeLists(n);
}

void MetadataPageHeap::Delete(Span* span) {
  size_t ac = span->alignmentclass;
  list_[ac].Delete(span);
}

void MetadataPageHeap::RegisterSizeClass(Span* span, size_t ac, size_t sc) {
  list_[ac].RegisterSizeClass(span, sc);
  span->alignmentclass = ac;
}

static void RecordGrowth(size_t growth) {
  StackTrace* t = Static::stacktrace_allocator()->New();
  t->depth = GetStackTrace(t->stack, kMaxStackDepth-1, 3);
  t->size = growth;
  t->stack[kMaxStackDepth-1] = reinterpret_cast<void*>(Static::growth_stacks());
  Static::set_growth_stacks(t);
}

bool CoarsePageHeap::GrowHeap(Length n) {
  ASSERT(n == (ARENASIZE >> kPageShift));
  Length available_data_pages = (n / 2);
  // Bug in tcmalloc allows for 8-byte alignment for really small objects
  if (ac_ == 0)
    available_data_pages--;
  void* ptr = NULL;
  size_t actual_size;
  ptr = TCMalloc_SystemAlloc(n << kPageShift, &actual_size, ARENASIZE);
  if (ptr == NULL || actual_size != (n << kPageShift)) {
    Log(kLog, __FILE__, __LINE__,
        "tcmalloc-metadata: allocation of arena failed", n);
    return false;
  }
  RecordGrowth(available_data_pages << kPageShift);

  // Store metadata at the end of the arena
  size_t *end_ptr = ((size_t*)ptr) + (ARENASIZE / sizeof(size_t));
  *(end_ptr - 1) = Static::sizemap()->AlignmentBitsForAlignment(ac_) - 3;

  uint64_t old_system_bytes = stats_.system_bytes;
  stats_.system_bytes += (available_data_pages << kPageShift);
  stats_.committed_bytes += (available_data_pages << kPageShift);
  const PageID p = reinterpret_cast<uintptr_t>(ptr) >> kPageShift;
  ASSERT(p > 0);

  // If we have already a lot of pages allocated, just pre allocate a bunch of
  // memory for the page map. This prevents fragmentation by pagemap metadata
  // when a program keeps allocating and freeing large blocks.

  /*if (old_system_bytes < kPageMapBigAllocationThreshold
      && stats_.system_bytes >= kPageMapBigAllocationThreshold) {
    pagemap_.PreallocateMoreMemory();
  }*/

  // Make sure pagemap_ has entries for all of the new pages.
  // Plus ensure one before and one after so coalescing code
  // does not need bounds-checking.
  if (pagemap_.Ensure(p-1, available_data_pages +2)) {
    // Pretend the new area is allocated and then Delete() it to cause
    // any necessary coalescing to occur.
    Span* span = NewSpan(p, available_data_pages);
    RecordSpan(span);
    Delete(span);
    ASSERT(stats_.unmapped_bytes+ stats_.committed_bytes==stats_.system_bytes);
    ASSERT(Check());
    return true;
  } else {
    // We could not allocate memory within "pagemap_"
    // TODO: Once we can return memory to the system, return the new span
    return false;
  }
}

}  // namespace tcmalloc
