/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::BTreeMap;

use super::{
  FreeList, FreeListError, GenerationId, HeapKind, LargeObjectAllocation, PinnedAllocation, RegionDirectory,
  RegionDirectoryError, RegionId, RegionState,
};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum HeapTopologyError
{
  Directory(RegionDirectoryError),
  FreeList(FreeListError),
  WrongHeap,
  AllocationTooLarge,
  RegionIdOverflow,
}

impl From<RegionDirectoryError> for HeapTopologyError
{
  fn from(value: RegionDirectoryError) -> Self
  {
    Self::Directory(value)
  }
}

impl From<FreeListError> for HeapTopologyError
{
  fn from(value: FreeListError) -> Self
  {
    Self::FreeList(value)
  }
}

/// One independently reserved heap address space.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct HeapSpace
{
  kind: HeapKind,
  directory: RegionDirectory,
}

impl HeapSpace
{
  fn new(kind: HeapKind, region_count: u32) -> Result<Self, HeapTopologyError>
  {
    Ok(Self { kind,
              directory: RegionDirectory::new(region_count)? })
  }

  #[must_use]
  pub const fn kind(&self) -> HeapKind
  {
    self.kind
  }

  #[must_use]
  pub const fn directory(&self) -> &RegionDirectory
  {
    &self.directory
  }

  fn directory_mut(&mut self) -> &mut RegionDirectory
  {
    &mut self.directory
  }

  fn verify_roles(&self) -> Result<(), HeapTopologyError>
  {
    self.directory
        .regions()
        .iter()
        .all(|region| self.kind.accepts(region.metadata.state))
        .then_some(())
        .ok_or(HeapTopologyError::WrongHeap)
  }
}

/// Three-heap XGC topology: main (young + old), LOH, and POH.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct HeapTopology
{
  main: HeapSpace,
  large: HeapSpace,
  pinned: HeapSpace,
  pinned_free_lists: BTreeMap<RegionId, FreeList>,
}

impl HeapTopology
{
  pub fn new(main_regions: u32, large_regions: u32, pinned_regions: u32) -> Result<Self, HeapTopologyError>
  {
    Ok(Self { main: HeapSpace::new(HeapKind::Main, main_regions)?,
              large: HeapSpace::new(HeapKind::LargeObject, large_regions)?,
              pinned: HeapSpace::new(HeapKind::PinnedObject, pinned_regions)?,
              pinned_free_lists: BTreeMap::new() })
  }

  #[must_use]
  pub const fn main(&self) -> &HeapSpace
  {
    &self.main
  }

  #[must_use]
  pub const fn large(&self) -> &HeapSpace
  {
    &self.large
  }

  #[must_use]
  pub const fn pinned(&self) -> &HeapSpace
  {
    &self.pinned
  }

  pub fn acquire_eden(&mut self, generation: GenerationId) -> Result<RegionId, HeapTopologyError>
  {
    Ok(self.main.directory_mut().acquire_eden(generation)?)
  }

  pub fn acquire_large_object(&mut self,
                              object_size: usize,
                              generation: GenerationId)
                              -> Result<LargeObjectAllocation, HeapTopologyError>
  {
    Ok(self.large
           .directory_mut()
           .acquire_large_object(object_size, generation)?)
  }

  pub fn allocate_pinned(&mut self,
                         size: usize,
                         alignment: usize,
                         generation: GenerationId)
                         -> Result<PinnedAllocation, HeapTopologyError>
  {
    for (region, free_list) in &mut self.pinned_free_lists
    {
      if let Some(span) = free_list.allocate(size, alignment)?
      {
        return Ok(PinnedAllocation { region: *region,
                                     offset: span.offset,
                                     size: span.size });
      }
    }

    let region = self.pinned.directory_mut().acquire_pinned_object_region(generation)?;
    let mut free_list = FreeList::for_region();
    let Some(span) = free_list.allocate(size, alignment)?
    else
    {
      return Err(HeapTopologyError::AllocationTooLarge);
    };
    self.pinned_free_lists.insert(region, free_list);
    Ok(PinnedAllocation { region,
                          offset: span.offset,
                          size: span.size })
  }

  pub fn sweep_pinned(&mut self, allocation: PinnedAllocation) -> Result<(), HeapTopologyError>
  {
    let free_list = self.pinned_free_lists
                        .get_mut(&allocation.region)
                        .ok_or(HeapTopologyError::WrongHeap)?;
    free_list.release(super::FreeSpan { offset: allocation.offset,
                                        size: allocation.size })?;
    Ok(())
  }

  pub fn transition_main(&mut self,
                         id: RegionId,
                         state: RegionState,
                         generation: GenerationId)
                         -> Result<(), HeapTopologyError>
  {
    if !HeapKind::Main.accepts(state)
    {
      return Err(HeapTopologyError::WrongHeap);
    }
    self.main.directory_mut().transition(id, state, generation)?;
    Ok(())
  }

  pub fn verify(&self) -> Result<(), HeapTopologyError>
  {
    self.main.verify_roles()?;
    self.large.verify_roles()?;
    self.pinned.verify_roles()?;
    Ok(())
  }
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::xgc::{LARGE_OBJECT_THRESHOLD_BYTES, REGION_SIZE_BYTES};

  #[test]
  fn allocation_classes_are_physically_separated()
  {
    let mut heaps = HeapTopology::new(4, 4, 2).unwrap();
    assert_eq!(heaps.acquire_eden(GenerationId(1)), Ok(RegionId(0)));
    let large = heaps.acquire_large_object(LARGE_OBJECT_THRESHOLD_BYTES, GenerationId(1))
                     .unwrap();
    let pinned = heaps.allocate_pinned(128, 16, GenerationId(1)).unwrap();
    assert_eq!(large.first_region, RegionId(0));
    assert_eq!(pinned.region, RegionId(0));
    assert_eq!(heaps.main().directory().regions()[0].metadata.state, RegionState::Eden);
    assert_eq!(heaps.large().directory().regions()[0].metadata.state,
               RegionState::LargeObject);
    assert_eq!(heaps.pinned().directory().regions()[0].metadata.state,
               RegionState::PinnedObject);
    assert_eq!(heaps.verify(), Ok(()));
  }

  #[test]
  fn pinned_sweep_reuses_and_coalesces_non_moving_storage()
  {
    let mut heaps = HeapTopology::new(1, 1, 1).unwrap();
    let first = heaps.allocate_pinned(64, 8, GenerationId(1)).unwrap();
    let second = heaps.allocate_pinned(64, 8, GenerationId(1)).unwrap();
    heaps.sweep_pinned(first).unwrap();
    heaps.sweep_pinned(second).unwrap();
    let reused = heaps.allocate_pinned(REGION_SIZE_BYTES, 8, GenerationId(2)).unwrap();
    assert_eq!(reused.offset, 0);
  }

  #[test]
  fn main_heap_rejects_large_and_pinned_roles()
  {
    let mut heaps = HeapTopology::new(1, 1, 1).unwrap();
    assert_eq!(heaps.transition_main(RegionId(0), RegionState::LargeObject, GenerationId(1)),
               Err(HeapTopologyError::WrongHeap));
  }
}
