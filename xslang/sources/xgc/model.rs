/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

/// XGC regions have one fixed size in runtime ABI version 0.
pub const REGION_SIZE_BYTES: usize = 2 * 1024 * 1024;

/// Objects at or above half a region use the large-object heap.
pub const LARGE_OBJECT_THRESHOLD_BYTES: usize = REGION_SIZE_BYTES / 2;

/// Runtime-selected allocation class.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum AllocationClass
{
  #[default]
  Young,
  Large,
  Pinned,
}

/// Physically separate managed heap containing an allocation class.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum HeapKind
{
  Main,
  LargeObject,
  PinnedObject,
}

impl HeapKind
{
  #[must_use]
  pub const fn accepts(self, state: RegionState) -> bool
  {
    match self
    {
      Self::Main => matches!(state,
                             RegionState::Free | RegionState::Eden | RegionState::Survivor | RegionState::Old),
      Self::LargeObject => matches!(state, RegionState::Free | RegionState::LargeObject),
      Self::PinnedObject => matches!(state, RegionState::Free | RegionState::PinnedObject),
    }
  }
}

/// The role currently assigned to one region.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum RegionState
{
  #[default]
  Free,
  Eden,
  Survivor,
  Old,
  LargeObject,
  PinnedObject,
}

impl RegionState
{
  const fn can_transition_to(self, next: Self) -> bool
  {
    matches!((self, next),
             (Self::Free, Self::Eden | Self::LargeObject | Self::PinnedObject) |
             (Self::Eden, Self::Survivor | Self::Old | Self::Free) |
             (Self::Survivor, Self::Survivor | Self::Old | Self::Free) |
             (Self::Old, Self::Old | Self::Free) |
             (Self::LargeObject, Self::Free) |
             (Self::PinnedObject, Self::Free))
  }
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, Ord, PartialOrd)]
pub struct RegionId(pub u32);

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Ord, PartialOrd)]
pub struct GenerationId(pub u64);

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RegionModelError
{
  InvalidStateTransition,
  UsageExceedsRegion,
}

/// Metadata lives outside the managed heap and never moves with objects.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RegionMetadata
{
  pub id: RegionId,
  pub state: RegionState,
  pub generation: GenerationId,
  pub live_bytes: usize,
  pub garbage_bytes: usize,
  pub allocation_top: usize,
}

impl RegionMetadata
{
  #[must_use]
  pub const fn free(id: RegionId) -> Self
  {
    Self { id,
           state: RegionState::Free,
           generation: GenerationId(0),
           live_bytes: 0,
           garbage_bytes: 0,
           allocation_top: 0 }
  }

  pub fn transition(&mut self, next: RegionState, generation: GenerationId) -> Result<(), RegionModelError>
  {
    if !self.state.can_transition_to(next)
    {
      return Err(RegionModelError::InvalidStateTransition);
    }
    self.state = next;
    self.generation = generation;
    self.live_bytes = 0;
    self.garbage_bytes = 0;
    self.allocation_top = 0;
    Ok(())
  }

  pub fn update_usage(&mut self,
                      live_bytes: usize,
                      garbage_bytes: usize,
                      allocation_top: usize)
                      -> Result<(), RegionModelError>
  {
    let Some(classified_bytes) = live_bytes.checked_add(garbage_bytes)
    else
    {
      return Err(RegionModelError::UsageExceedsRegion);
    };
    if allocation_top > REGION_SIZE_BYTES || classified_bytes > allocation_top
    {
      return Err(RegionModelError::UsageExceedsRegion);
    }
    self.live_bytes = live_bytes;
    self.garbage_bytes = garbage_bytes;
    self.allocation_top = allocation_top;
    Ok(())
  }
}

/// Whole-program XGC selection. RAII remains the default mode.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct XgcConfiguration
{
  pub enabled: bool,
}

impl XgcConfiguration
{
  #[must_use]
  pub const fn disabled() -> Self
  {
    Self { enabled: false }
  }

  #[must_use]
  pub const fn enabled() -> Self
  {
    Self { enabled: true }
  }

  #[must_use]
  pub const fn is_large(object_size: usize) -> bool
  {
    object_size >= LARGE_OBJECT_THRESHOLD_BYTES
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn xgc_is_disabled_by_default()
  {
    assert_eq!(XgcConfiguration::default(), XgcConfiguration::disabled());
  }

  #[test]
  fn region_and_large_object_boundaries_are_stable()
  {
    assert_eq!(REGION_SIZE_BYTES, 2_097_152);
    assert!(!XgcConfiguration::is_large(LARGE_OBJECT_THRESHOLD_BYTES - 1));
    assert!(XgcConfiguration::is_large(LARGE_OBJECT_THRESHOLD_BYTES));
  }

  #[test]
  fn region_metadata_enforces_lifecycle_and_usage_bounds()
  {
    let mut region = RegionMetadata::free(RegionId(7));
    assert_eq!(region.transition(RegionState::Eden, GenerationId(11)), Ok(()));
    assert_eq!(region.update_usage(512, 128, 640), Ok(()));
    assert_eq!(region.live_bytes, 512);
    assert_eq!(region.transition(RegionState::LargeObject, GenerationId(12)),
               Err(RegionModelError::InvalidStateTransition));
    assert_eq!(region.update_usage(REGION_SIZE_BYTES, 1, REGION_SIZE_BYTES),
               Err(RegionModelError::UsageExceedsRegion));
    assert_eq!(region.transition(RegionState::Survivor, GenerationId(12)), Ok(()));
    assert_eq!(region.live_bytes, 0);
    assert_eq!(region.transition(RegionState::Free, GenerationId(12)), Ok(()));
  }
}
