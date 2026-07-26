/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{OBJECT_ALIGNMENT_BYTES, REGION_SIZE_BYTES, RegionId};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum FreeListError
{
  ZeroSize,
  InvalidAlignment,
  ArithmeticOverflow,
  OutsideRegion,
  OverlappingFree,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct FreeSpan
{
  pub offset: usize,
  pub size: usize,
}

impl FreeSpan
{
  fn end(self) -> Result<usize, FreeListError>
  {
    self.offset
        .checked_add(self.size)
        .ok_or(FreeListError::ArithmeticOverflow)
  }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PinnedAllocation
{
  pub region: RegionId,
  pub offset: usize,
  pub size: usize,
}

/// Coalescing free-list used by one non-moving POH region.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct FreeList
{
  spans: Vec<FreeSpan>,
  free_bytes: usize,
}

impl Default for FreeList
{
  fn default() -> Self
  {
    Self::for_region()
  }
}

impl FreeList
{
  #[must_use]
  pub fn for_region() -> Self
  {
    Self { spans: vec![FreeSpan { offset: 0,
                                  size: REGION_SIZE_BYTES }],
           free_bytes: REGION_SIZE_BYTES }
  }

  pub fn allocate(&mut self, size: usize, alignment: usize) -> Result<Option<FreeSpan>, FreeListError>
  {
    if size == 0
    {
      return Err(FreeListError::ZeroSize);
    }
    if alignment < OBJECT_ALIGNMENT_BYTES as usize || !alignment.is_power_of_two()
    {
      return Err(FreeListError::InvalidAlignment);
    }

    for index in 0..self.spans.len()
    {
      let span = self.spans[index];
      let aligned = align_up(span.offset, alignment)?;
      let end = aligned.checked_add(size).ok_or(FreeListError::ArithmeticOverflow)?;
      if end > span.end()?
      {
        continue;
      }
      self.consume(index, span, aligned, end);
      self.free_bytes -= size;
      return Ok(Some(FreeSpan { offset: aligned,
                                size }));
    }
    Ok(None)
  }

  fn consume(&mut self, index: usize, original: FreeSpan, start: usize, end: usize)
  {
    let original_end = original.offset + original.size;
    self.spans.remove(index);
    if end < original_end
    {
      self.spans.insert(index, FreeSpan { offset: end,
                                          size: original_end - end });
    }
    if original.offset < start
    {
      self.spans.insert(index, FreeSpan { offset: original.offset,
                                          size: start - original.offset });
    }
  }

  pub fn release(&mut self, span: FreeSpan) -> Result<(), FreeListError>
  {
    if span.size == 0
    {
      return Err(FreeListError::ZeroSize);
    }
    let end = span.end()?;
    if end > REGION_SIZE_BYTES
    {
      return Err(FreeListError::OutsideRegion);
    }
    let insertion = self.spans.partition_point(|candidate| candidate.offset < span.offset);
    if insertion > 0 && self.spans[insertion - 1].end()? > span.offset
    {
      return Err(FreeListError::OverlappingFree);
    }
    if insertion < self.spans.len() && end > self.spans[insertion].offset
    {
      return Err(FreeListError::OverlappingFree);
    }
    self.spans.insert(insertion, span);
    self.free_bytes = self.free_bytes
                          .checked_add(span.size)
                          .ok_or(FreeListError::ArithmeticOverflow)?;
    self.coalesce_around(insertion);
    Ok(())
  }

  fn coalesce_around(&mut self, mut index: usize)
  {
    if index > 0 && self.spans[index - 1].offset + self.spans[index - 1].size == self.spans[index].offset
    {
      let right = self.spans.remove(index);
      self.spans[index - 1].size += right.size;
      index -= 1;
    }
    if index + 1 < self.spans.len() && self.spans[index].offset + self.spans[index].size == self.spans[index + 1].offset
    {
      let right = self.spans.remove(index + 1);
      self.spans[index].size += right.size;
    }
  }

  #[must_use]
  pub const fn free_bytes(&self) -> usize
  {
    self.free_bytes
  }

  #[must_use]
  pub fn largest_span(&self) -> usize
  {
    self.spans.iter().map(|span| span.size).max().unwrap_or(0)
  }

  #[must_use]
  pub fn spans(&self) -> &[FreeSpan]
  {
    &self.spans
  }
}

fn align_up(value: usize, alignment: usize) -> Result<usize, FreeListError>
{
  value.checked_add(alignment - 1)
       .map(|sum| sum & !(alignment - 1))
       .ok_or(FreeListError::ArithmeticOverflow)
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn allocation_respects_alignment_and_split_spans()
  {
    let mut list = FreeList::for_region();
    let first = list.allocate(24, 8).unwrap().unwrap();
    let second = list.allocate(32, 64).unwrap().unwrap();
    assert_eq!(first.offset, 0);
    assert_eq!(second.offset, 64);
    assert_eq!(list.free_bytes(), REGION_SIZE_BYTES - 56);
  }

  #[test]
  fn sweep_release_coalesces_adjacent_spans()
  {
    let mut list = FreeList::for_region();
    let first = list.allocate(64, 8).unwrap().unwrap();
    let second = list.allocate(64, 8).unwrap().unwrap();
    list.release(first).unwrap();
    list.release(second).unwrap();
    assert_eq!(list.spans(), &[FreeSpan { offset: 0,
                                          size: REGION_SIZE_BYTES }]);
    assert_eq!(list.free_bytes(), REGION_SIZE_BYTES);
  }

  #[test]
  fn double_free_is_rejected()
  {
    let mut list = FreeList::for_region();
    let allocation = list.allocate(64, 8).unwrap().unwrap();
    list.release(allocation).unwrap();
    assert_eq!(list.release(allocation), Err(FreeListError::OverlappingFree));
  }
}
