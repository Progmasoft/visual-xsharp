/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{REGION_SIZE_BYTES, RegionId, RegionMetadata, RegionState};

/// A successful LOH collection leaves at most five percent free holes.
pub const LOH_MAX_FRAGMENTATION_RATIO: f64 = 0.05;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CompactionPolicy
{
  pub maximum_regions: usize,
  pub maximum_live_bytes: usize,
}

impl Default for CompactionPolicy
{
  fn default() -> Self
  {
    Self { maximum_regions: 4,
           maximum_live_bytes: REGION_SIZE_BYTES * 3 / 4 }
  }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct CompactionCandidate
{
  pub region: RegionId,
  pub fragmentation: f64,
  pub reclaimable_bytes: usize,
  pub relocation_bytes: usize,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct LohFragmentation
{
  pub live_bytes: usize,
  pub fragmented_bytes: usize,
  pub ratio: f64,
  pub within_required_limit: bool,
}

#[must_use]
pub fn measure_loh_fragmentation(regions: &[RegionMetadata]) -> LohFragmentation
{
  let (live_bytes, fragmented_bytes) = regions.iter()
                                              .filter(|region| region.state == RegionState::LargeObject)
                                              .fold((0usize, 0usize), |(live, fragmented), region| {
                                                (live.saturating_add(region.live_bytes),
                                                 fragmented.saturating_add(region.garbage_bytes))
                                              });
  let classified = live_bytes.saturating_add(fragmented_bytes);
  let ratio = if classified == 0
  {
    0.0
  }
  else
  {
    fragmented_bytes as f64 / classified as f64
  };
  LohFragmentation { live_bytes,
                     fragmented_bytes,
                     ratio,
                     within_required_limit: ratio <= LOH_MAX_FRAGMENTATION_RATIO }
}

/// Chooses fragmented LOH regions until the mandatory fragmentation limit is met.
pub fn select_loh_compaction_set(regions: &[RegionMetadata], policy: CompactionPolicy) -> Vec<CompactionCandidate>
{
  if policy.maximum_regions == 0 || measure_loh_fragmentation(regions).within_required_limit
  {
    return Vec::new();
  }
  let mut candidates: Vec<_> = regions.iter()
                                      .filter(|region| region.state == RegionState::LargeObject)
                                      .filter(|region| region.garbage_bytes > 0)
                                      .filter(|region| region.live_bytes <= policy.maximum_live_bytes)
                                      .map(|region| CompactionCandidate { region: region.id,
                                                                          fragmentation: region.garbage_bytes as f64 /
                                                                                         REGION_SIZE_BYTES as f64,
                                                                          reclaimable_bytes: region.garbage_bytes,
                                                                          relocation_bytes: region.live_bytes })
                                      .collect();
  candidates.sort_by(|left, right| {
              right.fragmentation
                   .total_cmp(&left.fragmentation)
                   .then_with(|| left.relocation_bytes.cmp(&right.relocation_bytes))
                   .then_with(|| left.region.cmp(&right.region))
            });
  candidates.truncate(policy.maximum_regions);
  candidates
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::xgc::GenerationId;

  fn loh_region(id: u32, live: usize, garbage: usize) -> RegionMetadata
  {
    let mut region = RegionMetadata::free(RegionId(id));
    region.transition(RegionState::LargeObject, GenerationId(1)).unwrap();
    region.update_usage(live, garbage, live + garbage).unwrap();
    region
  }

  #[test]
  fn loh_selection_prioritizes_fragmentation_without_unbounded_moves()
  {
    let regions = vec![loh_region(0, 128, REGION_SIZE_BYTES / 2),
                       loh_region(1, REGION_SIZE_BYTES - 128, 128),
                       loh_region(2, 256, REGION_SIZE_BYTES / 3)];
    let selected = select_loh_compaction_set(&regions, CompactionPolicy { maximum_regions: 1,
                                                                          ..CompactionPolicy::default() });
    assert_eq!(selected.len(), 1);
    assert_eq!(selected[0].region, RegionId(0));
  }

  #[test]
  fn five_percent_fragmentation_is_a_success_invariant()
  {
    let clean = vec![loh_region(0, 950, 50)];
    let fragmented = vec![loh_region(0, 949, 51)];
    assert!(measure_loh_fragmentation(&clean).within_required_limit);
    assert!(!measure_loh_fragmentation(&fragmented).within_required_limit);
    assert!(select_loh_compaction_set(&clean, CompactionPolicy::default()).is_empty());
    assert!(!select_loh_compaction_set(&fragmented, CompactionPolicy::default()).is_empty());
  }
}
