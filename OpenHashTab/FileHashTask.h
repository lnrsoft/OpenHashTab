//    Copyright 2019 namazso <admin@namazso.eu>
//    This file is part of OpenHashTab.
//
//    OpenHashTab is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    OpenHashTab is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with OpenHashTab.  If not, see <https://www.gnu.org/licenses/>.
#pragma once
#include "Hasher.h"

class OpenHashTabPropPage;

class FileHashTask
{
  // Increasing this will make CPU use more efficent,
  // but also increase memory usage
  constexpr static intptr_t k_block_size = 2 << 20; // 2 MB

  // Increasing this will increase memory use and reduce
  // possibility of a slower disk clogging up the queue
  constexpr static intptr_t k_max_allocations = 512; // 1 GB

  static std::atomic<intptr_t> s_allocations_remaining;

  static uint8_t* BlockTryAllocate();
  static void BlockReset(uint8_t* p);
  static void BlockFree(uint8_t* p);

  static VOID NTAPI HashWorkCallback(
    _Inout_     PTP_CALLBACK_INSTANCE instance,
    _Inout_opt_ PVOID                 ctx,
    _Inout_     PTP_WORK              work
  );

  static VOID WINAPI IoCallback(
    _Inout_     PTP_CALLBACK_INSTANCE instance,
    _Inout_opt_ PVOID                 ctx,
    _Inout_opt_ PVOID                 overlapped,
    _In_        ULONG                 result,
    _In_        ULONG_PTR             bytes_transferred,
    _Inout_     PTP_IO                io
  );

  static void ProcessReadQueue(uint8_t* reuse_block = nullptr);

  uint8_t* _block{nullptr};

  PTP_WORK _threadpool_hash_work = nullptr;

  PTP_IO _threadpool_io = nullptr;
  
  mbedtls_md_context_t _hash_contexts[k_hashers_count];

  OVERLAPPED _overlapped{};

  using hash_results_t = std::array<std::vector<uint8_t>, k_hashers_count>;

  hash_results_t _hash_results;

  HANDLE _handle;

  OpenHashTabPropPage* _prop_page;

  tstring _display_name;
  std::vector<uint8_t> _expected_hash;

  uint64_t _file_size;
  uint64_t _current_offset{ 0 };

  uint64_t _file_index;
  uint32_t _volume_serial;

  DWORD _error{ ERROR_SUCCESS };

  std::atomic<unsigned> _hash_start_counter{ 0 };
  std::atomic<unsigned> _hash_finish_counter{ 0 };

  int _match_state{};
  bool _cancelled{};

  uint8_t _lparam_idx[k_hashers_count];

public:
  FileHashTask(const FileHashTask&) = delete;
  FileHashTask(FileHashTask&&) = delete;
  FileHashTask& operator=(const FileHashTask&) = delete;
  FileHashTask& operator=(FileHashTask&&) = delete;

  FileHashTask(const tstring& path, OpenHashTabPropPage* prop_page, tstring display_name, std::vector<uint8_t> expected_hash = {});

  // You should only ever delete this object after Finish() was called,
  // either by error state or natural finish. TODO: check this somehow
  ~FileHashTask();

  void StartProcessing() { ReadBlockAsync(); }

private:
  // Enqueue the next block for reading
  // Returns true if an async io was started, false if the file was enqueued
  bool ReadBlockAsync(uint8_t* reuse_block = nullptr);

  void OverlappedCompletionRoutine(ULONG error_code, ULONG_PTR bytes_transferred);

  void AddToHashQueue();

  void DoHashRound();

  void FinishedBlock();

  // Do NOT use "this" after calling Finish(), as it might be deleted
  // This may be the last reference to PropPage, which then deletes us in destructor.
  void Finish();

  uint64_t GetCurrentBlockSize() const
  {
    auto size = _file_size - _current_offset;
    if (size > k_block_size)
      size = k_block_size;
    return size;
  }

public:
  LPARAM ToLparam(size_t hasher) const { return (LPARAM)&_lparam_idx[hasher]; }
  static std::pair<FileHashTask*, size_t> FromLparam(LPARAM lparam)
  {
    const auto lp = (const std::uint8_t*)lparam;
    return { CONTAINING_RECORD(lp - *lp, FileHashTask, _lparam_idx), *lp };
  }

  DWORD GetError() const { return _error; }
  HANDLE GetHandle() const { return _handle; }
  const hash_results_t& GetHashResult() const { return _hash_results; }
  const tstring& GetDisplayName() const { return _display_name; }

  enum : int
  {
    MatchState_None = -1,
    MatchState_Mismatch = -2
  };
  int GetMatchState() const { return _match_state; }

  void SetCancelled() { _cancelled = true; }
};