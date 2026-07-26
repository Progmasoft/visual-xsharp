/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

mod address;
mod barrier;
mod binding;
mod bitmap;
mod card_table;
mod collection;
mod compaction;
mod free_list;
mod heap;
mod model;
mod recovery;
mod remembered_set;
mod roots;
mod satb;
mod selection;
mod telemetry;
mod topology;

pub use address::{GcAddressError, HeapOffset, OBJECT_ALIGNMENT_BYTES, ObjectReference};
pub use barrier::{BarrierError, BarrierSet, ForwardingTable, WriteBarrierResult};
pub use binding::{BoundXgc, ObjectLayout, ObjectLayoutError, TypeMetadataId, XgcRuntimeBinding};
pub use bitmap::{MarkBitmap, MarkBitmapError};
pub use card_table::{CARD_COUNT, CARD_SIZE_BYTES, CardIndex, CardTable, CardTableError};
pub use collection::{CollectionKind, CollectionPhase, CollectionPlan, CollectionPlanError};
pub use compaction::{
  CompactionCandidate, CompactionPolicy, LOH_MAX_FRAGMENTATION_RATIO, LohFragmentation, measure_loh_fragmentation,
  select_loh_compaction_set,
};
pub use free_list::{FreeList, FreeListError, FreeSpan, PinnedAllocation};
pub use heap::{LargeObjectAllocation, RegionDirectory, RegionDirectoryError, RegionRecord};
pub use model::{
  AllocationClass, GenerationId, HeapKind, LARGE_OBJECT_THRESHOLD_BYTES, REGION_SIZE_BYTES, RegionId, RegionMetadata,
  RegionModelError, RegionState, XgcConfiguration,
};
pub use recovery::{CollectionAttempt, CollectionFailure, RecoveryController, RecoveryDecision};
pub use remembered_set::{RememberedCard, RememberedSet, RememberedSetKind};
pub use roots::{RootKind, RootLocation, SafepointId, StackMap, StackMapError, StackMapRegistry};
pub use satb::{SatbBuffer, SatbBufferError, SatbRecord};
pub use selection::{CollectionCandidate, CollectionPolicy, select_collection_set};
pub use telemetry::{Telemetry, TelemetrySnapshot};
pub use topology::{HeapSpace, HeapTopology, HeapTopologyError};
