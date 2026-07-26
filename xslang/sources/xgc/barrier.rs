/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::collections::{BTreeMap, BTreeSet};

use super::{
  CardIndex, CardTableError, ObjectReference, RegionDirectory, RegionDirectoryError, RememberedCard, SatbBuffer,
  SatbBufferError, SatbRecord,
};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum BarrierError
{
  Satb(SatbBufferError),
  Region(RegionDirectoryError),
  Card(CardTableError),
  ForwardingCycle,
}

impl From<RegionDirectoryError> for BarrierError
{
  fn from(value: RegionDirectoryError) -> Self
  {
    Self::Region(value)
  }
}

impl From<CardTableError> for BarrierError
{
  fn from(value: CardTableError) -> Self
  {
    Self::Card(value)
  }
}

/// Relocation records consulted by the concurrent read barrier.
#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct ForwardingTable
{
  entries: BTreeMap<ObjectReference, ObjectReference>,
}

impl ForwardingTable
{
  pub fn forward(&mut self, old: ObjectReference, new: ObjectReference)
  {
    self.entries.insert(old, new);
  }

  pub fn resolve(&self, reference: ObjectReference) -> Result<ObjectReference, BarrierError>
  {
    let mut current = reference;
    let mut visited = BTreeSet::new();
    while let Some(next) = self.entries.get(&current).copied()
    {
      if !visited.insert(current)
      {
        return Err(BarrierError::ForwardingCycle);
      }
      current = next;
    }
    Ok(current)
  }

  #[must_use]
  pub fn len(&self) -> usize
  {
    self.entries.len()
  }

  #[must_use]
  pub fn is_empty(&self) -> bool
  {
    self.entries.is_empty()
  }

  pub fn clear(&mut self)
  {
    self.entries.clear();
  }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct WriteBarrierResult
{
  pub card: CardIndex,
  pub card_became_dirty: bool,
  pub remembered_edge_added: bool,
  pub published_satb: Vec<ObjectReference>,
}

/// Combined SATB, card-table, remembered-set, and relocation barrier state.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BarrierSet
{
  satb: SatbBuffer,
  forwarding: ForwardingTable,
}

impl BarrierSet
{
  pub fn new(satb_capacity: usize) -> Result<Self, BarrierError>
  {
    let satb = SatbBuffer::new(satb_capacity).map_err(BarrierError::Satb)?;
    Ok(Self { satb,
              forwarding: ForwardingTable::default() })
  }

  pub fn install_forwarding(&mut self, old: ObjectReference, new: ObjectReference)
  {
    self.forwarding.forward(old, new);
  }

  pub fn read(&self, reference: ObjectReference) -> Result<ObjectReference, BarrierError>
  {
    self.forwarding.resolve(reference)
  }

  pub fn write(&mut self,
               directory: &mut RegionDirectory,
               source: ObjectReference,
               old_value: Option<ObjectReference>,
               new_value: Option<ObjectReference>)
               -> Result<WriteBarrierResult, BarrierError>
  {
    let published_satb = match old_value.map(|old| self.satb.record_old_reference(old))
    {
      Some(SatbRecord::Publish(entries)) => entries,
      Some(SatbRecord::Buffered) | None => Vec::new(),
    };

    let source_region = source.region().map_err(|_| RegionDirectoryError::InvalidRegion)?;
    let source_offset = usize::try_from(source.region_offset()).map_err(|_| RegionDirectoryError::InvalidRegion)?;
    let source_record = directory.region_mut(source_region)?;
    let previous_dirty_count = source_record.cards.dirty_count();
    let card = source_record.cards.mark_offset(source_offset)?;
    let card_became_dirty = source_record.cards.dirty_count() > previous_dirty_count;

    let mut remembered_edge_added = false;
    if let Some(target) = new_value
    {
      let target_region = target.region().map_err(|_| RegionDirectoryError::InvalidRegion)?;
      if target_region != source_region
      {
        remembered_edge_added = directory.region_mut(target_region)?
                                         .remembered
                                         .insert(RememberedCard { region: source_region,
                                                                  card });
      }
    }

    Ok(WriteBarrierResult { card,
                            card_became_dirty,
                            remembered_edge_added,
                            published_satb })
  }

  #[must_use]
  pub fn flush_satb(&mut self) -> Vec<ObjectReference>
  {
    self.satb.take()
  }

  pub fn finish_relocation(&mut self)
  {
    self.forwarding.clear();
  }
}

#[cfg(test)]
mod tests
{
  use super::*;
  use crate::xgc::{GenerationId, RegionId};

  #[test]
  fn read_barrier_follows_forwarding_chain()
  {
    let first = ObjectReference::from_region(RegionId(0), 8).unwrap();
    let second = ObjectReference::from_region(RegionId(1), 16).unwrap();
    let third = ObjectReference::from_region(RegionId(2), 24).unwrap();
    let mut barriers = BarrierSet::new(4).unwrap();
    barriers.install_forwarding(first, second);
    barriers.install_forwarding(second, third);
    assert_eq!(barriers.read(first), Ok(third));
    barriers.finish_relocation();
    assert_eq!(barriers.read(first), Ok(first));
  }

  #[test]
  fn write_barrier_records_satb_card_and_cross_region_edge()
  {
    let mut directory = RegionDirectory::new(3).unwrap();
    directory.acquire_eden(GenerationId(1)).unwrap();
    directory.acquire_eden(GenerationId(1)).unwrap();
    let source = ObjectReference::from_region(RegionId(0), 520).unwrap();
    let old = ObjectReference::from_region(RegionId(0), 16).unwrap();
    let new = ObjectReference::from_region(RegionId(1), 24).unwrap();
    let mut barriers = BarrierSet::new(1).unwrap();
    let result = barriers.write(&mut directory, source, Some(old), Some(new)).unwrap();
    assert_eq!(result.published_satb, vec![old]);
    assert!(result.remembered_edge_added);
    assert_eq!(directory.region(RegionId(0)).unwrap().cards.dirty_count(), 1);
    assert_eq!(directory.region(RegionId(1)).unwrap().remembered.len(), 1);
  }
}
