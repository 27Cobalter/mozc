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

#include "dictionary/inline_registration.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {
namespace dictionary {
namespace {

// Loads the storage and locks it for an update. Returns the inline dictionary
// (creating it if |create|), or nullptr. The caller must UnLock() if the
// storage was locked (|*locked|).
user_dictionary::UserDictionary* LockInlineDictionary(
    UserDictionaryStorage& storage, bool create, bool* locked) {
  *locked = false;
  // The lock is held by the dictionary tool or the word register dialog.
  if (!storage.Lock()) {
    LOG(WARNING) << "Cannot lock the user dictionary";
    return nullptr;
  }
  *locked = true;
  // Load after the lock so that the edits of the others are not overwritten.
  // Do not save over a file that exists but cannot be read.
  if (!storage.Load().ok() && storage.Exists().ok()) {
    LOG(ERROR) << "Cannot load the user dictionary";
    return nullptr;
  }
  absl::StatusOr<uint64_t> id =
      storage.GetUserDictionaryId(kInlineRegistrationDictionaryName);
  if (!id.ok() && create) {
    id = storage.CreateDictionary(kInlineRegistrationDictionaryName);
  }
  return id.ok() ? storage.GetUserDictionary(*id) : nullptr;
}

}  // namespace

bool AddInlineRegisteredWord(const std::string& key, const std::string& value) {
  UserDictionaryStorage storage;
  bool locked = false;
  user_dictionary::UserDictionary* dic =
      LockInlineDictionary(storage, /*create=*/true, &locked);
  bool ok = false;
  if (dic != nullptr) {
    for (const auto& e : dic->entries()) {
      if (e.key() == key && e.value() == value) {
        ok = true;  // Already registered.
        break;
      }
    }
    if (!ok) {
      user_dictionary::UserDictionary::Entry entry;
      entry.set_key(key);
      entry.set_value(value);
      entry.set_pos(user_dictionary::UserDictionary::NOUN);
      if (!UserDictionaryStorage::IsDictionaryFull(*dic) &&
          user_dictionary::ValidateEntry(entry).ok()) {
        *dic->add_entries() = std::move(entry);
        ok = storage.Save().ok();
      }
    }
  }
  if (locked) {
    storage.UnLock();
  }
  return ok;
}

bool RemoveInlineRegisteredWord(const std::string& key,
                                const std::string& value) {
  UserDictionaryStorage storage;
  bool locked = false;
  user_dictionary::UserDictionary* dic =
      LockInlineDictionary(storage, /*create=*/false, &locked);
  bool removed = false;
  if (dic != nullptr) {
    auto* entries = dic->mutable_entries();
    for (int i = entries->size() - 1; i >= 0; --i) {
      if (entries->Get(i).key() == key && entries->Get(i).value() == value) {
        entries->DeleteSubrange(i, 1);
        removed = true;
      }
    }
    if (removed) {
      removed = storage.Save().ok();
    }
  }
  if (locked) {
    storage.UnLock();
  }
  return removed;
}

InlineRegisteredWords LoadInlineRegisteredWords() {
  InlineRegisteredWords words;
  UserDictionaryStorage storage;
  if (!storage.Load().ok()) {
    return words;
  }
  const absl::StatusOr<uint64_t> id =
      storage.GetUserDictionaryId(kInlineRegistrationDictionaryName);
  if (!id.ok()) {
    return words;
  }
  if (const user_dictionary::UserDictionary* dic =
          storage.GetUserDictionary(*id)) {
    for (const auto& e : dic->entries()) {
      words.emplace(e.key(), e.value());
    }
  }
  return words;
}

}  // namespace dictionary
}  // namespace mozc
