/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

//! Public XGC runtime-binding API integration test.

use xslang::xgc::*;

#[derive(Default)]
struct RuntimeBinding
{
  rewritten_roots: Vec<(RootLocation, Option<ObjectReference>)>,
}

impl XgcRuntimeBinding for RuntimeBinding
{
  fn object_layout(&self, metadata: TypeMetadataId) -> Option<ObjectLayout>
  {
    (metadata == TypeMetadataId(7)).then(|| ObjectLayout::new(24, 8).unwrap())
  }

  fn allocation_class(&self, metadata: TypeMetadataId) -> AllocationClass
  {
    if metadata == TypeMetadataId(7)
    {
      AllocationClass::Pinned
    }
    else
    {
      AllocationClass::Young
    }
  }

  fn trace_references(&self, _: TypeMetadataId, _: ObjectReference, _: &mut dyn FnMut(ObjectReference)) {}

  fn rewrite_root(&mut self, location: RootLocation, reference: Option<ObjectReference>)
  {
    self.rewritten_roots.push((location, reference));
  }
}

#[test]
fn public_xgc_api_requires_an_explicit_runtime_binding()
{
  let binding = RuntimeBinding::default();
  let mut xgc = BoundXgc::new(XgcConfiguration::enabled(), binding);
  let layout = xgc.binding().object_layout(TypeMetadataId(7)).unwrap();
  assert_eq!(layout.size_bytes(), 24);
  assert_eq!(xgc.binding().allocation_class(TypeMetadataId(7)),
             AllocationClass::Pinned);

  let root = RootLocation { kind: RootKind::StackSlot,
                            index: 0 };
  xgc.binding_mut().rewrite_root(root, None);
  assert_eq!(xgc.into_binding().rewritten_roots, vec![(root, None)]);
}

#[test]
fn public_heap_topology_keeps_main_loh_and_poh_separate()
{
  let mut heaps = HeapTopology::new(2, 2, 1).unwrap();
  heaps.acquire_eden(GenerationId(1)).unwrap();
  heaps.acquire_large_object(LARGE_OBJECT_THRESHOLD_BYTES, GenerationId(1))
       .unwrap();
  let pinned = heaps.allocate_pinned(32, 8, GenerationId(1)).unwrap();
  assert_eq!(heaps.main().directory().regions()[0].metadata.state, RegionState::Eden);
  assert_eq!(heaps.large().directory().regions()[0].metadata.state,
             RegionState::LargeObject);
  assert_eq!(heaps.pinned().directory().regions()[pinned.region.0 as usize].metadata
                                                                           .state,
             RegionState::PinnedObject);
}
