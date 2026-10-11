#pragma once

#include "files.hpp"
#include "release.hpp"

#include <functional>

namespace point_zero::loader {
// Caller holds Transaction's exclusive update lock for this entire call and
// supplies parse_release(manifest) and the installed release. The exact manifest
// bytes remain unchanged for this call. Return nullopt when select_update declines.
// fetch supplies a requested member from the already checked in-memory ZIP,
// with its exact manifest byte limit. The caller owns the archive and its
// cancellation token; a callback cannot be interrupted here. Parsing bounds count, per-file and total
// bytes; staging checks stop during I/O, hashing, and immediately before publish.
//
// Only staging/<id> is written, then moved to versions/<id> after full on-disk
// revalidation. Partial staged artifacts survive failure for verified retry;
// bounded, recognizable atomic-write remnants in that subtree may be removed.
// Existing versions are immutable: parse/validate and reuse, or throw.
// Errors and cancellation throw; no active/managed/pending/journal/config/cache
// writes. The returned ID is an offer only: Transaction::offer revalidates
// and pins it before use. No activation or network retries happen here.
std::optional<std::string> stage_generation(
    Files& files, const Release& candidate, std::string_view manifest,
    const Release& installed, Channel requested,
    const std::function<std::string(const Artifact&, std::size_t)>& fetch,
    const std::atomic_bool* cancelled = nullptr, std::uint32_t game_major = 0);
} // namespace point_zero::loader
