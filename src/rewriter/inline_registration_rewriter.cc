// Copyright 2010-2021, Google Inc.
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

#include "rewriter/inline_registration_rewriter.h"

#include <cstddef>
#include <string>
#include <utility>

#include "absl/strings/match.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "dictionary/inline_registration.h"
#include "request/conversion_request.h"

namespace mozc {

InlineRegistrationRewriter::InlineRegistrationRewriter()
    : words_(dictionary::LoadInlineRegisteredWords()) {}

bool InlineRegistrationRewriter::Reload() {
  words_ = dictionary::LoadInlineRegisteredWords();
  return true;
}

bool InlineRegistrationRewriter::Rewrite(const ConversionRequest& request,
                                         Segments* segments) const {
  if (words_.empty()) {
    return false;
  }
  bool modified = false;
  for (Segment& segment : segments->conversion_segments()) {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      converter::Candidate* candidate = segment.mutable_candidate(i);
      if (!(candidate->attributes & converter::Attribute::USER_DICTIONARY) ||
          !words_.contains({candidate->content_key, candidate->content_value}) ||
          absl::EndsWith(candidate->description,
                         dictionary::kInlineRegistrationName)) {
        continue;
      }
      if (!candidate->description.empty()) {
        candidate->description += " ";
      }
      candidate->description += dictionary::kInlineRegistrationName;
      modified = true;
    }
  }
  return modified;
}

bool InlineRegistrationRewriter::ClearHistoryEntry(const Segments& segments,
                                                   size_t segment_index,
                                                   int candidate_index) {
  const converter::Candidate& candidate =
      segments.segment(segment_index).candidate(candidate_index);
  if (!(candidate.attributes & converter::Attribute::USER_DICTIONARY) ||
      !words_.contains({candidate.content_key, candidate.content_value})) {
    return false;
  }
  if (!dictionary::RemoveInlineRegisteredWord(candidate.content_key,
                                              candidate.content_value)) {
    return false;
  }
  words_.erase({candidate.content_key, candidate.content_value});
  return true;
}

}  // namespace mozc
