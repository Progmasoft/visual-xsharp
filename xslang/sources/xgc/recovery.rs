/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::CollectionKind;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub enum CollectionAttempt
{
  #[default]
  Concurrent,
  EmergencyConcurrent,
  StopTheWorld,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CollectionFailure
{
  AllocationPressure,
  EvacuationReserveExhausted,
  RelocationConflict,
  MarkQueueOverflow,
  DeadlineExceeded,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RecoveryDecision
{
  RetryEmergencyConcurrent,
  RetryStopTheWorld,
  Exhausted,
}

/// Per-cycle fallback controller. A new collection always starts concurrently.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct RecoveryController
{
  kind: CollectionKind,
  attempt: CollectionAttempt,
  last_failure: Option<CollectionFailure>,
}

impl RecoveryController
{
  #[must_use]
  pub const fn new(kind: CollectionKind) -> Self
  {
    Self { kind,
           attempt: CollectionAttempt::Concurrent,
           last_failure: None }
  }

  pub fn fail(&mut self, failure: CollectionFailure) -> RecoveryDecision
  {
    self.last_failure = Some(failure);
    match self.attempt
    {
      CollectionAttempt::Concurrent =>
      {
        self.attempt = CollectionAttempt::EmergencyConcurrent;
        RecoveryDecision::RetryEmergencyConcurrent
      }
      CollectionAttempt::EmergencyConcurrent =>
      {
        self.attempt = CollectionAttempt::StopTheWorld;
        RecoveryDecision::RetryStopTheWorld
      }
      CollectionAttempt::StopTheWorld => RecoveryDecision::Exhausted,
    }
  }

  pub fn succeed(&mut self)
  {
    self.attempt = CollectionAttempt::Concurrent;
    self.last_failure = None;
  }

  #[must_use]
  pub const fn kind(self) -> CollectionKind
  {
    self.kind
  }

  #[must_use]
  pub const fn attempt(self) -> CollectionAttempt
  {
    self.attempt
  }

  #[must_use]
  pub const fn last_failure(self) -> Option<CollectionFailure>
  {
    self.last_failure
  }

  #[must_use]
  pub const fn mutators_run(self) -> bool
  {
    !matches!(self.attempt, CollectionAttempt::StopTheWorld)
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn recovery_retries_concurrently_before_stw()
  {
    let mut recovery = RecoveryController::new(CollectionKind::Old);
    assert_eq!(recovery.fail(CollectionFailure::EvacuationReserveExhausted),
               RecoveryDecision::RetryEmergencyConcurrent);
    assert_eq!(recovery.attempt(), CollectionAttempt::EmergencyConcurrent);
    assert!(recovery.mutators_run());
    assert_eq!(recovery.fail(CollectionFailure::AllocationPressure),
               RecoveryDecision::RetryStopTheWorld);
    assert_eq!(recovery.attempt(), CollectionAttempt::StopTheWorld);
    assert!(!recovery.mutators_run());
    assert_eq!(recovery.fail(CollectionFailure::DeadlineExceeded),
               RecoveryDecision::Exhausted);
  }

  #[test]
  fn successful_recovery_resets_the_next_cycle()
  {
    let mut recovery = RecoveryController::new(CollectionKind::LargeObject);
    recovery.fail(CollectionFailure::RelocationConflict);
    recovery.succeed();
    assert_eq!(recovery.attempt(), CollectionAttempt::Concurrent);
    assert_eq!(recovery.last_failure(), None);
  }
}
