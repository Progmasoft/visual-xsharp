/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TelemetrySnapshot
{
  pub minor_collections: u64,
  pub major_collections: u64,
  pub loh_collections: u64,
  pub poh_collections: u64,
  pub allocated_bytes: u64,
  pub promoted_bytes: u64,
  pub evacuated_bytes: u64,
  pub evacuation_failures: u64,
  pub published_satb_entries: u64,
  pub dirty_cards: u64,
  pub compacted_bytes: u64,
  pub swept_bytes: u64,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct Telemetry
{
  snapshot: TelemetrySnapshot,
}

impl Telemetry
{
  pub fn record_minor_collection(&mut self)
  {
    self.snapshot.minor_collections = self.snapshot.minor_collections.saturating_add(1);
  }

  pub fn record_major_collection(&mut self)
  {
    self.snapshot.major_collections = self.snapshot.major_collections.saturating_add(1);
  }

  pub fn record_loh_collection(&mut self)
  {
    self.snapshot.loh_collections = self.snapshot.loh_collections.saturating_add(1);
  }

  pub fn record_poh_collection(&mut self)
  {
    self.snapshot.poh_collections = self.snapshot.poh_collections.saturating_add(1);
  }

  pub fn record_allocation(&mut self, bytes: u64)
  {
    self.snapshot.allocated_bytes = self.snapshot.allocated_bytes.saturating_add(bytes);
  }

  pub fn record_promotion(&mut self, bytes: u64)
  {
    self.snapshot.promoted_bytes = self.snapshot.promoted_bytes.saturating_add(bytes);
  }

  pub fn record_evacuation(&mut self, bytes: u64)
  {
    self.snapshot.evacuated_bytes = self.snapshot.evacuated_bytes.saturating_add(bytes);
  }

  pub fn record_evacuation_failure(&mut self)
  {
    self.snapshot.evacuation_failures = self.snapshot.evacuation_failures.saturating_add(1);
  }

  pub fn record_satb_publish(&mut self, entries: usize)
  {
    self.snapshot.published_satb_entries = self.snapshot.published_satb_entries.saturating_add(entries as u64);
  }

  pub fn record_dirty_card(&mut self)
  {
    self.snapshot.dirty_cards = self.snapshot.dirty_cards.saturating_add(1);
  }

  pub fn record_compaction(&mut self, bytes: u64)
  {
    self.snapshot.compacted_bytes = self.snapshot.compacted_bytes.saturating_add(bytes);
  }

  pub fn record_sweep(&mut self, bytes: u64)
  {
    self.snapshot.swept_bytes = self.snapshot.swept_bytes.saturating_add(bytes);
  }

  #[must_use]
  pub const fn snapshot(&self) -> TelemetrySnapshot
  {
    self.snapshot
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  #[test]
  fn telemetry_accumulates_without_affecting_semantics()
  {
    let mut telemetry = Telemetry::default();
    telemetry.record_minor_collection();
    telemetry.record_allocation(128);
    telemetry.record_allocation(64);
    telemetry.record_satb_publish(3);
    assert_eq!(telemetry.snapshot().minor_collections, 1);
    assert_eq!(telemetry.snapshot().allocated_bytes, 192);
    assert_eq!(telemetry.snapshot().published_satb_entries, 3);
  }
}
