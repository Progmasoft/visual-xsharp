/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual_xsharp.project

import java.util.Locale

@XsProjectDsl
class DependenciesScope internal constructor() {
  internal val required = mutableListOf<PackageDependency>()
  internal val optional = mutableListOf<OptionalPackageDependency>()
  internal val selections = mutableListOf<PackageFeatureSelection>()

  internal fun add(dependency: PackageDependency) {
    required += dependency
  }

  internal fun addOptional(feature: String, dependency: PackageDependency, enabled: Boolean) {
    val normalizedFeature = requireFeatureName(feature)
    optional += OptionalPackageDependency(normalizedFeature, dependency)
    selections += PackageFeatureSelection(dependency.name, normalizedFeature, enabled)
  }
}

internal fun resolveDependencies(
  required: List<PackageDependency>,
  optional: List<OptionalPackageDependency>,
  selections: List<PackageFeatureSelection>,
): DependencyResolution {
  val requiredByName = linkedMapOf<String, PackageDependency>()
  required.forEach { dependency ->
    val normalized = normalizeCoordinate(dependency)
    val previous = requiredByName[normalized.name]
    if (previous != null && previous != normalized) {
      throw ProjectConfigurationException("package '${normalized.name}' has conflicting coordinates")
    }
    requiredByName[normalized.name] = normalized
  }

  val optionalByKey = linkedMapOf<Pair<String, String>, OptionalPackageDependency>()
  optional.forEach { declaration ->
    val dependency = normalizeCoordinate(declaration.dependency)
    val feature = requireFeatureName(declaration.feature)
    if (dependency.name in requiredByName) {
      throw ProjectConfigurationException("package '${dependency.name}' cannot be both required and optional")
    }
    val key = dependency.name to feature
    val previous = optionalByKey[key]
    val normalized = OptionalPackageDependency(feature, dependency)
    if (previous != null && previous != normalized) {
      throw ProjectConfigurationException("optional package '${dependency.name}' has conflicting coordinates")
    }
    optionalByKey[key] = normalized
  }

  val selectedByKey = linkedMapOf<Pair<String, String>, PackageFeatureSelection>()
  selections.forEach { selection ->
    val normalized = PackageFeatureSelection(
      requirePackageCoordinate(selection.packageName),
      requireFeatureName(selection.feature),
      selection.enabled,
    )
    val key = normalized.packageName to normalized.feature
    if (key !in optionalByKey) {
      throw ProjectConfigurationException(
        "feature '${normalized.feature}' is not declared by optional package '${normalized.packageName}'",
      )
    }
    selectedByKey[key] = normalized
  }

  val completeSelections = optionalByKey.keys
    .map { key -> selectedByKey[key] ?: PackageFeatureSelection(key.first, key.second, false) }
    .sortedWith(compareBy(PackageFeatureSelection::packageName, PackageFeatureSelection::feature))
  return DependencyResolution(
    requiredByName.values.sortedWith(packageDependencyOrder),
    optionalByKey.values.sortedWith(
      compareBy<OptionalPackageDependency> { it.dependency.name }
        .thenBy(OptionalPackageDependency::feature)
        .thenBy { it.dependency.stability }
        .thenBy { it.dependency.version },
    ),
    completeSelections,
  )
}

internal fun packageCoordinate(name: String, stability: Stability, version: String) =
  PackageDependency(requirePackageCoordinate(name), stability.name, requirePackageVersion(version))

private fun normalizeCoordinate(dependency: PackageDependency) =
  PackageDependency(
    requirePackageCoordinate(dependency.name),
    normalizeStability(dependency.stability),
    requirePackageVersion(dependency.version),
  )

internal fun requirePackageCoordinate(value: String): String {
  val name = requireText(value, "package name")
  val segment = "[A-Za-z][A-Za-z0-9_]*"
  if (!name.matches(Regex("$segment\\.$segment"))) {
    throw ProjectConfigurationException("package name must use Publisher.Name coordinates: $name")
  }
  return name
}

internal fun requireFeatureName(value: String): String {
  val name = requireText(value, "dependency feature")
  if (!name.matches(Regex("[A-Za-z_][A-Za-z0-9_]*"))) {
    throw ProjectConfigurationException("dependency feature must be one identifier: $name")
  }
  return name
}

internal fun normalizeStability(value: String): String {
  val stability = requireText(value, "dependency stability").uppercase(Locale.ROOT)
  if (stability !in Stability.entries.map(Stability::name)) {
    throw ProjectConfigurationException("dependency stability is not recognized: $stability")
  }
  return stability
}

internal fun requirePackageVersion(value: String): String {
  val version = requireText(value, "package version")
  val component = "(?:0|[1-9][0-9]*)"
  if (!version.matches(Regex("$component\\.$component\\.$component(?:-[0-9A-Za-z.-]+)?"))) {
    throw ProjectConfigurationException("package version must be an exact semantic version: $version")
  }
  return version
}
