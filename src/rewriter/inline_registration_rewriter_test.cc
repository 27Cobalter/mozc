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

#include <string>

#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "dictionary/inline_registration.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

void AddCandidate(const std::string& value, bool user_dictionary,
                  Segments* segments) {
  Segment* segment = segments->segments_size() == 0
                         ? segments->push_back_segment()
                         : segments->mutable_segment(0);
  segment->set_key("あ");
  converter::Candidate* candidate = segment->add_candidate();
  candidate->key = "あ";
  candidate->content_key = "あ";
  candidate->value = value;
  candidate->content_value = value;
  if (user_dictionary) {
    candidate->attributes |= converter::Attribute::USER_DICTIONARY;
  }
}

class InlineRegistrationRewriterTest : public testing::TestWithTempUserProfile {
};

TEST_F(InlineRegistrationRewriterTest, MarksAndDeletesTheRegisteredWord) {
  ASSERT_TRUE(dictionary::AddInlineRegisteredWord("あ", "亜"));
  InlineRegistrationRewriter rewriter;
  const ConversionRequest request;

  Segments segments;
  AddCandidate("亜", /*user_dictionary=*/true, &segments);
  // Another word of the user dictionary, and a word of the system dictionary.
  AddCandidate("阿", /*user_dictionary=*/true, &segments);
  AddCandidate("亜", /*user_dictionary=*/false, &segments);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  const Segment& segment = segments.segment(0);
  EXPECT_EQ(segment.candidate(0).description,
            dictionary::kInlineRegistrationName);
  EXPECT_TRUE(segment.candidate(1).description.empty());
  EXPECT_TRUE(segment.candidate(2).description.empty());

  // The description is not added twice.
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  // Only the registered word can be deleted, and it is gone from the file.
  EXPECT_FALSE(rewriter.ClearHistoryEntry(segments, 0, 1));
  EXPECT_FALSE(rewriter.ClearHistoryEntry(segments, 0, 2));
  EXPECT_TRUE(rewriter.ClearHistoryEntry(segments, 0, 0));
  EXPECT_TRUE(dictionary::LoadInlineRegisteredWords().empty());
  EXPECT_FALSE(rewriter.ClearHistoryEntry(segments, 0, 0));
}

}  // namespace
}  // namespace mozc
