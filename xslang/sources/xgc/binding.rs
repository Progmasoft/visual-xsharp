/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <xs-lang.chess031@slmails.com>
 * SPDX-License-Identifier: MPL-2.0
 */

use super::{AllocationClass, ObjectReference, RootLocation, XgcConfiguration};

/// Stable identity assigned to X# runtime type metadata.
#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
pub struct TypeMetadataId(pub u32);

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ObjectLayoutError
{
  ZeroAlignment,
  NonPowerOfTwoAlignment,
  MisalignedSize,
}

/// Target-specific object size and alignment supplied by a runtime binding.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ObjectLayout
{
  size_bytes: u64,
  alignment_bytes: u32,
}

impl ObjectLayout
{
  pub fn new(size_bytes: u64, alignment_bytes: u32) -> Result<Self, ObjectLayoutError>
  {
    if alignment_bytes == 0
    {
      return Err(ObjectLayoutError::ZeroAlignment);
    }
    if !alignment_bytes.is_power_of_two()
    {
      return Err(ObjectLayoutError::NonPowerOfTwoAlignment);
    }
    if !size_bytes.is_multiple_of(u64::from(alignment_bytes))
    {
      return Err(ObjectLayoutError::MisalignedSize);
    }
    Ok(Self { size_bytes,
              alignment_bytes })
  }

  #[must_use]
  pub const fn size_bytes(self) -> u64
  {
    self.size_bytes
  }

  #[must_use]
  pub const fn alignment_bytes(self) -> u32
  {
    self.alignment_bytes
  }
}

/// Connects target-independent XGC policy to X# runtime metadata and roots.
///
/// Third-party runtimes may implement this trait, but must preserve the X#
/// object metadata, precise tracing, and root-rewrite contracts.
pub trait XgcRuntimeBinding
{
  fn object_layout(&self, metadata: TypeMetadataId) -> Option<ObjectLayout>;

  /// Selects a runtime allocation class. Ordinary metadata starts in young.
  fn allocation_class(&self, _: TypeMetadataId) -> AllocationClass
  {
    AllocationClass::Young
  }

  fn trace_references(&self,
                      metadata: TypeMetadataId,
                      object: ObjectReference,
                      visitor: &mut dyn FnMut(ObjectReference));

  fn rewrite_root(&mut self, location: RootLocation, reference: Option<ObjectReference>);
}

/// XGC configuration paired with the runtime binding required to execute it.
#[derive(Debug)]
pub struct BoundXgc<B>
{
  configuration: XgcConfiguration,
  binding: B,
}

impl<B: XgcRuntimeBinding> BoundXgc<B>
{
  #[must_use]
  pub const fn new(configuration: XgcConfiguration, binding: B) -> Self
  {
    Self { configuration,
           binding }
  }

  #[must_use]
  pub const fn configuration(&self) -> XgcConfiguration
  {
    self.configuration
  }

  #[must_use]
  pub const fn binding(&self) -> &B
  {
    &self.binding
  }

  pub const fn binding_mut(&mut self) -> &mut B
  {
    &mut self.binding
  }

  #[must_use]
  pub fn into_binding(self) -> B
  {
    self.binding
  }
}

#[cfg(test)]
mod tests
{
  use super::*;

  struct EmptyBinding;

  impl XgcRuntimeBinding for EmptyBinding
  {
    fn object_layout(&self, _: TypeMetadataId) -> Option<ObjectLayout>
    {
      None
    }

    fn trace_references(&self, _: TypeMetadataId, _: ObjectReference, _: &mut dyn FnMut(ObjectReference)) {}

    fn rewrite_root(&mut self, _: RootLocation, _: Option<ObjectReference>) {}
  }

  #[test]
  fn binding_is_required_to_construct_an_executable_xgc_context()
  {
    let context = BoundXgc::new(XgcConfiguration::enabled(), EmptyBinding);
    assert!(context.configuration().enabled);
    assert_eq!(context.binding().object_layout(TypeMetadataId(1)), None);
  }

  #[test]
  fn object_layout_requires_natural_alignment()
  {
    assert_eq!(ObjectLayout::new(24, 8).unwrap().size_bytes(), 24);
    assert_eq!(ObjectLayout::new(24, 3), Err(ObjectLayoutError::NonPowerOfTwoAlignment));
    assert_eq!(ObjectLayout::new(10, 8), Err(ObjectLayoutError::MisalignedSize));
  }
}
