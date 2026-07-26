/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{
  CardTable, GenerationId, LARGE_OBJECT_THRESHOLD_BYTES, MarkBitmap, REGION_SIZE_BYTES, RegionId, RegionMetadata,
  RegionModelError, RegionState, RememberedSet,
};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RegionDirectoryError
{
  EmptyHeap,
  RegionCountOverflow,
  NoFreeRegion,
  NoContiguousRegions,
  NotLargeObject,
  InvalidRegion,
  RegionModel(RegionModelError),
}

impl From<RegionModelError> for RegionDirectoryError
{
  fn from(value: RegionModelError) -> Self
  {
    Self::RegionModel(value)
  }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RegionRecord
{
  pub metadata: RegionMetadata,
  pub marks: MarkBitmap,
  pub cards: CardTable,
  pub remembered: RememberedSet,
}

impl RegionRecord
{
  fn free(id: RegionId) -> Self
  {
    Self { metadata: RegionMetadata::free(id),
           marks: MarkBitmap::new(),
           cards: CardTable::new(),
           remembered: RememberedSet::new() }
  }

  fn reset_auxiliary_metadata(&mut self)
  {
    self.marks.clear();
    self.cards.clear();
    self.remembered.clear();
  }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct LargeObjectAllocation
{
  pub first_region: RegionId,
  pub region_count: u32,
  pub object_size: usize,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RegionDirectory
{
  regions: Vec<RegionRecord>,
}

impl RegionDirectory
{
  pub fn new(region_count: u32) -> Result<Self, RegionDirectoryError>
  {
    if region_count == 0
    {
      return Err(RegionDirectoryError::EmptyHeap);
    }
    let capacity = usize::try_from(region_count).map_err(|_| RegionDirectoryError::RegionCountOverflow)?;
    let mut regions = Vec::with_capacity(capacity);
    for index in 0..region_count
    {
      regions.push(RegionRecord::free(RegionId(index)));
    }
    Ok(Self { regions })
  }

  #[must_use]
  pub fn regions(&self) -> &[RegionRecord]
  {
    &self.regions
  }

  pub fn region(&self, id: RegionId) -> Result<&RegionRecord, RegionDirectoryError>
  {
    self.regions
        .get(id.0 as usize)
        .ok_or(RegionDirectoryError::InvalidRegion)
  }

  pub fn region_mut(&mut self, id: RegionId) -> Result<&mut RegionRecord, RegionDirectoryError>
  {
    self.regions
        .get_mut(id.0 as usize)
        .ok_or(RegionDirectoryError::InvalidRegion)
  }

  pub fn acquire_eden(&mut self, generation: GenerationId) -> Result<RegionId, RegionDirectoryError>
  {
    let record = self.regions
                     .iter_mut()
                     .find(|record| record.metadata.state == RegionState::Free)
                     .ok_or(RegionDirectoryError::NoFreeRegion)?;
    record.metadata.transition(RegionState::Eden, generation)?;
    record.reset_auxiliary_metadata();
    Ok(record.metadata.id)
  }

  pub fn acquire_large_object(&mut self,
                              object_size: usize,
                              generation: GenerationId)
                              -> Result<LargeObjectAllocation, RegionDirectoryError>
  {
    if object_size < LARGE_OBJECT_THRESHOLD_BYTES
    {
      return Err(RegionDirectoryError::NotLargeObject);
    }
    let region_count = object_size.div_ceil(REGION_SIZE_BYTES);
    let start = self.regions
                    .windows(region_count)
                    .position(|window| window.iter().all(|record| record.metadata.state == RegionState::Free))
                    .ok_or(RegionDirectoryError::NoContiguousRegions)?;
    for record in &mut self.regions[start..start + region_count]
    {
      record.metadata.transition(RegionState::LargeObject, generation)?;
      record.reset_auxiliary_metadata();
    }
    Ok(LargeObjectAllocation { first_region: RegionId(u32::try_from(start).map_err(|_| {
                                                                            RegionDirectoryError::RegionCountOverflow
                                                                          })?),
                               region_count: u32::try_from(region_count).map_err(|_| {
                                                                          RegionDirectoryError::RegionCountOverflow
                                                                        })?,
                               object_size })
  }

  pub fn acquire_pinned_object_region(&mut self, generation: GenerationId) -> Result<RegionId, RegionDirectoryError>
  {
    let record = self.regions
                     .iter_mut()
                     .find(|record| record.metadata.state == RegionState::Free)
                     .ok_or(RegionDirectoryError::NoFreeRegion)?;
    record.metadata.transition(RegionState::PinnedObject, generation)?;
    record.reset_auxiliary_metadata();
    Ok(record.metadata.id)
  }

  pub fn transition(&mut self,
                    id: RegionId,
                    state: RegionState,
                    generation: GenerationId)
                    -> Result<(), RegionDirectoryError>
  {
    let record = self.region_mut(id)?;
    record.metadata.transition(state, generation)?;
    if state == RegionState::Free
    {
      record.reset_auxiliary_metadata();
    }
    Ok(())
  }

  #[must_use]
  pub fn free_region_count(&self) -> usize
  {
    self.regions
        .iter()
        .filter(|region| region.metadata.state == RegionState::Free)
        .count()
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn directory_claims_eden_and_contiguous_large_object_regions()
  {
    let mut directory = RegionDirectory::new(5).unwrap();
    assert_eq!(directory.acquire_eden(GenerationId(1)), Ok(RegionId(0)));
    let allocation = directory.acquire_large_object(REGION_SIZE_BYTES + 1, GenerationId(2))
                              .unwrap();
    assert_eq!(allocation.first_region, RegionId(1));
    assert_eq!(allocation.region_count, 2);
    assert_eq!(directory.free_region_count(), 2);
    assert_eq!(directory.acquire_large_object(64, GenerationId(3)),
               Err(RegionDirectoryError::NotLargeObject));
    assert_eq!(directory.acquire_pinned_object_region(GenerationId(3)), Ok(RegionId(3)));
  }
}
