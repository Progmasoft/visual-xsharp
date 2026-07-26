/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CollectionKind
{
  Young,
  Old,
  LargeObject,
  PinnedObject,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CollectionPhase
{
  RootHandshake,
  ConcurrentMark,
  RemarkHandshake,
  SelectCollectionSet,
  ConcurrentCopy,
  ConcurrentRelocate,
  ConcurrentCompact,
  ConcurrentSweep,
  CoalesceFreeList,
  ReclaimRegions,
  RetireForwarding,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CollectionPlanError
{
  EmptyPlan,
  MissingMark,
  MovingWithoutReadBarrier,
  PinnedHeapMoved,
  SweepWithoutFreeListCoalescing,
}

/// Target-independent collection phase plan for one XGC heap class.
#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CollectionPlan
{
  kind: CollectionKind,
  phases: Vec<CollectionPhase>,
  read_barrier: bool,
  write_barrier: bool,
}

impl CollectionPlan
{
  #[must_use]
  pub fn young() -> Self
  {
    Self { kind: CollectionKind::Young,
           phases: vec![CollectionPhase::RootHandshake,
                        CollectionPhase::ConcurrentMark,
                        CollectionPhase::RemarkHandshake,
                        CollectionPhase::ConcurrentCopy,
                        CollectionPhase::ReclaimRegions,
                        CollectionPhase::RetireForwarding],
           read_barrier: true,
           write_barrier: true }
  }

  #[must_use]
  pub fn old() -> Self
  {
    Self { kind: CollectionKind::Old,
           phases: vec![CollectionPhase::RootHandshake,
                        CollectionPhase::ConcurrentMark,
                        CollectionPhase::RemarkHandshake,
                        CollectionPhase::SelectCollectionSet,
                        CollectionPhase::ConcurrentRelocate,
                        CollectionPhase::ReclaimRegions,
                        CollectionPhase::RetireForwarding],
           read_barrier: true,
           write_barrier: true }
  }

  #[must_use]
  pub fn large_object() -> Self
  {
    Self { kind: CollectionKind::LargeObject,
           phases: vec![CollectionPhase::RootHandshake,
                        CollectionPhase::ConcurrentMark,
                        CollectionPhase::RemarkHandshake,
                        CollectionPhase::SelectCollectionSet,
                        CollectionPhase::ConcurrentCompact,
                        CollectionPhase::ReclaimRegions,
                        CollectionPhase::RetireForwarding],
           read_barrier: true,
           write_barrier: true }
  }

  #[must_use]
  pub fn pinned_object() -> Self
  {
    Self { kind: CollectionKind::PinnedObject,
           phases: vec![CollectionPhase::RootHandshake,
                        CollectionPhase::ConcurrentMark,
                        CollectionPhase::RemarkHandshake,
                        CollectionPhase::ConcurrentSweep,
                        CollectionPhase::CoalesceFreeList],
           read_barrier: false,
           write_barrier: true }
  }

  pub fn verify(&self) -> Result<(), CollectionPlanError>
  {
    if self.phases.is_empty()
    {
      return Err(CollectionPlanError::EmptyPlan);
    }
    if !self.phases.contains(&CollectionPhase::ConcurrentMark)
    {
      return Err(CollectionPlanError::MissingMark);
    }
    let moves_objects = self.phases.iter().any(|phase| {
                                            matches!(phase,
                                                     CollectionPhase::ConcurrentCopy |
                                                     CollectionPhase::ConcurrentRelocate |
                                                     CollectionPhase::ConcurrentCompact)
                                          });
    if moves_objects && !self.read_barrier
    {
      return Err(CollectionPlanError::MovingWithoutReadBarrier);
    }
    if self.kind == CollectionKind::PinnedObject && moves_objects
    {
      return Err(CollectionPlanError::PinnedHeapMoved);
    }
    if self.phases.contains(&CollectionPhase::ConcurrentSweep) &&
       !self.phases.contains(&CollectionPhase::CoalesceFreeList)
    {
      return Err(CollectionPlanError::SweepWithoutFreeListCoalescing);
    }
    Ok(())
  }

  #[must_use]
  pub const fn kind(&self) -> CollectionKind
  {
    self.kind
  }

  #[must_use]
  pub fn phases(&self) -> &[CollectionPhase]
  {
    &self.phases
  }

  #[must_use]
  pub const fn uses_read_barrier(&self) -> bool
  {
    self.read_barrier
  }

  #[must_use]
  pub const fn uses_write_barrier(&self) -> bool
  {
    self.write_barrier
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn all_builtin_plans_preserve_barrier_and_movement_invariants()
  {
    for plan in [CollectionPlan::young(),
                 CollectionPlan::old(),
                 CollectionPlan::large_object(),
                 CollectionPlan::pinned_object()]
    {
      assert_eq!(plan.verify(), Ok(()));
      assert!(plan.uses_write_barrier());
    }
    assert!(CollectionPlan::young().uses_read_barrier());
    assert!(!CollectionPlan::pinned_object().uses_read_barrier());
  }

  #[test]
  fn pinned_plan_is_non_moving_mark_sweep()
  {
    let plan = CollectionPlan::pinned_object();
    assert!(plan.phases().contains(&CollectionPhase::ConcurrentSweep));
    assert!(plan.phases().contains(&CollectionPhase::CoalesceFreeList));
    assert!(!plan.phases().contains(&CollectionPhase::ConcurrentCompact));
  }
}
