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

#ifndef TCMALLOC_METADATA_PAGE_HEAP_H_
#define TCMALLOC_METADATA_PAGE_HEAP_H_

#include <config.h>
#include <stddef.h>                     // for size_t
#ifdef HAVE_STDINT_H
#include <stdint.h>                     // for uint64_t, int64_t, uint16_t
#endif
#include <gperftools/malloc_extension.h>
#include "base/basictypes.h"
#include "common.h"
#include "packed-cache-inl.h"
#include "pagemap.h"
#include "span.h"

// We need to dllexport PageHeap just for the unittest.  MSVC complains
// that we don't dllexport the PageHeap members, but we don't need to
// test those, so I just suppress this warning.
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

// This #ifdef should almost never be set.  Set NO_TCMALLOC_SAMPLES if
// you're porting to a system where you really can't get a stacktrace.
// Because we control the definition of GetStackTrace, all clients of
// GetStackTrace should #include us rather than stacktrace.h.
#ifdef NO_TCMALLOC_SAMPLES
  // We use #define so code compiles even if you #include stacktrace.h somehow.
# define GetStackTrace(stack, depth, skip)  (0)
#else
# include <gperftools/stacktrace.h>
#endif

namespace tcmalloc {

// -------------------------------------------------------------------------
// Page-level allocator which groups allocations based on size-class and
// allows for metadata
//  * Eager coalescing
//
// Collection of heap for page-level allocation. Redirects to the
// indivudual PageHeap responsible for each size-class.
// -------------------------------------------------------------------------

class PERFTOOLS_DLL_DECL CoarsePageHeap : public PageHeap {
  size_t ac_;
 public:
  bool GrowHeap(Length n);
  void SetAlignmentClass(size_t ac) { ac_ = ac; }
};

// -------------------------------------------------------------------------
// Page-level allocator which groups allocations based on size-class and
// allows for metadata
//  * Eager coalescing
//
// Collection of heap for page-level allocation. Redirects to the
// indivudual PageHeap responsible for each size-class.
// -------------------------------------------------------------------------

class PERFTOOLS_DLL_DECL MetadataPageHeap {
 public:
  MetadataPageHeap() : aggressive_decommit_(false) {
    for (int i = 0; i < kNumAlignmentClasses; ++i) {
      list_[i].SetAlignmentClass(i);
    }
  }

  // Allocate a run of "n" pages.  Returns zero if out of memory.
  // Caller should not pass "n == 0" -- instead, n should have
  // been rounded up already.
  Span* New(Length n, size_t ac);

  // Delete the span "[p, p+n-1]".
  // REQUIRES: span was returned by earlier call to New() and
  //           has not yet been deleted.
  void Delete(Span* span);

  // Mark an allocated span as being used for small objects of the
  // specified size-class.
  // REQUIRES: span was returned by an earlier call to New()
  //           and has not yet been deleted.
  void RegisterSizeClass(Span* span, size_t ac, size_t sc);

  // Split an allocated span into two spans: one of length "n" pages
  // followed by another span of length "span->length - n" pages.
  // Modifies "*span" to point to the first span of length "n" pages.
  // Returns a pointer to the second span.
  //
  // REQUIRES: "0 < n < span->length"
  // REQUIRES: span->location == IN_USE
  // REQUIRES: span->sizeclass == 0
  Span* Split(Span* span, Length n) { return list_[0].Split(span, n); }

  // Return the descriptor for the specified page.  Returns NULL if
  // this PageID was not allocated previously.
  inline Span* GetDescriptor(PageID p) const {
    Span *result;
    for (int i = 0; i < kNumAlignmentClasses; ++i) {
      result = reinterpret_cast<Span*>(list_[i].GetDescriptor(p));
      if (result != NULL)
        return result;
    }
    return NULL;
  }

  // If this page heap is managing a range with starting page # >= start,
  // store info about the range in *r and return true.  Else return false.
  bool GetNextRange(PageID start, base::MallocRange* r) {
    return list_[0].GetNextRange(start, r);
  }

  inline PageHeap::Stats stats() const { return list_[0].stats(); }

  void GetSmallSpanStats(PageHeap::SmallSpanStats* result) {
    return list_[0].GetSmallSpanStats(result);
  }

  void GetLargeSpanStats(PageHeap::LargeSpanStats* result) {
    return list_[0].GetLargeSpanStats(result);
  }

  bool Check() { return list_[0].Check(); }
  // Like Check() but does some more comprehensive checking.
  bool CheckExpensive() { return list_[0].CheckExpensive(); }
  bool CheckList(Span* list, Length min_pages, Length max_pages,
                 int freelist) {  // ON_NORMAL_FREELIST or ON_RETURNED_FREELIST
    return list_[0].CheckList(list, min_pages, max_pages, freelist);
  }

  // Try to release at least num_pages for reuse by the OS.  Returns
  // the actual number of pages released, which may be less than
  // num_pages if there weren't enough pages to release. The result
  // may also be larger than num_pages since page_heap might decide to
  // release one large range instead of fragmenting it into two
  // smaller released and unreleased ranges.
  Length ReleaseAtLeastNPages(Length num_pages) {
    Length result = 0;
    for (int i = 0; i < kNumAlignmentClasses; ++i) {
      result += list_[i].ReleaseAtLeastNPages(num_pages);
    }
    return result;
  }

  // Return 0 if we have no information, or else the correct sizeclass for p.
  // Reads and writes to pagemap_cache_ do not require locking.
  // The entries are 64 bits on 64-bit hardware and 16 bits on
  // 32-bit hardware, and we don't mind raciness as long as each read of
  // an entry yields a valid entry, not a partially updated entry.
  size_t GetSizeClassIfCached(PageID p) const {
    size_t result;
    for (int i = 0; i < kNumAlignmentClasses; ++i) {
      result = list_[i].GetSizeClassIfCached(p);
      if (result != 0)
        return result;
    }
    return 0;
  }

  void CacheSizeClass(PageID p, size_t ac, size_t cl) const {
    list_[ac].CacheSizeClass(p, cl);
  }

  bool GetAggressiveDecommit(void) {return aggressive_decommit_;}
  void SetAggressiveDecommit(bool aggressive_decommit) {
    aggressive_decommit_ = aggressive_decommit;
    for (int i = 0; i < kNumAlignmentClasses; ++i) {
      list_[i].SetAggressiveDecommit(aggressive_decommit_);
    }
  }

 private:
  CoarsePageHeap list_[kNumAlignmentClasses];     // Array indexed by size-class

  bool aggressive_decommit_;
};

}  // namespace tcmalloc

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif  // TCMALLOC_METADATA_PAGE_HEAP_H_
