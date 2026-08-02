/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.util.Locale

@XsProjectDsl
class DependenciesScope internal constructor() {
  internal val required = mutableListOf<ModuleDependency>()
  internal val optional = mutableListOf<OptionalModuleDependency>()

  fun addModule(
    name: String,
    stability: String,
    version: String,
  ) {
    required += moduleCoordinate(name, stability, version)
  }

  fun addOptionalModule(
    feature: String,
    name: String,
    stability: String,
    version: String,
  ) {
    optional +=
      OptionalModuleDependency(
        requireFeatureName(feature),
        moduleCoordinate(name, stability, version),
      )
  }
}

@XsProjectDsl
class DependencyFeatureScope internal constructor(
  private val moduleName: String,
) {
  internal val selections = mutableListOf<ModuleFeatureSelection>()

  fun feature(name: String) {
    selections += ModuleFeatureSelection(moduleName, requireFeatureName(name), true)
  }

  fun enable(name: String) {
    feature(name)
  }
}

@XsProjectDsl
class FeaturesScope internal constructor() {
  internal val selections = mutableListOf<ModuleFeatureSelection>()

  fun dependency(
    name: String,
    block: DependencyFeatureScope.() -> Unit,
  ) {
    val moduleName = requireModuleCoordinateName(name)
    selections += DependencyFeatureScope(moduleName).apply(block).selections
  }
}

internal fun resolveDependencies(
  required: List<ModuleDependency>,
  optional: List<OptionalModuleDependency>,
  selections: List<ModuleFeatureSelection>,
): DependencyResolution {
  val requiredByName = linkedMapOf<String, ModuleDependency>()
  required.forEach { module ->
    val normalized = normalizeCoordinate(module)
    val previous = requiredByName[normalized.name]
    if (previous != null && previous != normalized) {
      throw ProjectConfigurationException(
        "module '${normalized.name}' has conflicting stability or version coordinates",
      )
    }
    requiredByName[normalized.name] = normalized
  }

  val optionalByKey = linkedMapOf<Pair<String, String>, OptionalModuleDependency>()
  val optionalCoordinateByName = linkedMapOf<String, ModuleDependency>()
  optional.forEach { declaration ->
    val normalized =
      OptionalModuleDependency(
        requireFeatureName(declaration.feature),
        normalizeCoordinate(declaration.module),
      )
    if (normalized.module.name in requiredByName) {
      throw ProjectConfigurationException(
        "module '${normalized.module.name}' cannot be both required and optional",
      )
    }
    val previousCoordinate = optionalCoordinateByName[normalized.module.name]
    if (previousCoordinate != null && previousCoordinate != normalized.module) {
      throw ProjectConfigurationException(
        "optional module '${normalized.module.name}' has conflicting coordinates",
      )
    }
    optionalCoordinateByName[normalized.module.name] = normalized.module
    val key = normalized.module.name to normalized.feature
    val previous = optionalByKey[key]
    if (previous != null && previous != normalized) {
      throw ProjectConfigurationException(
        "optional feature '${normalized.feature}' for '${normalized.module.name}' is declared more than once",
      )
    }
    optionalByKey[key] = normalized
  }

  val selectedByKey = linkedMapOf<Pair<String, String>, ModuleFeatureSelection>()
  selections.forEach { selection ->
    val normalized =
      ModuleFeatureSelection(
        requireModuleCoordinateName(selection.moduleName),
        requireFeatureName(selection.feature),
        selection.enabled,
      )
    val key = normalized.moduleName to normalized.feature
    if (key !in optionalByKey) {
      throw ProjectConfigurationException(
        "feature '${normalized.feature}' is not declared by optional module '${normalized.moduleName}'",
      )
    }
    selectedByKey[key] = normalized
  }

  val completeSelections =
    optionalByKey.keys
      .map { key -> selectedByKey[key] ?: ModuleFeatureSelection(key.first, key.second, false) }
      .sortedWith(compareBy(ModuleFeatureSelection::moduleName, ModuleFeatureSelection::feature))
  return DependencyResolution(
    requiredByName.values.sortedWith(moduleDependencyOrder),
    optionalByKey.values.sortedWith(
      compareBy<OptionalModuleDependency> { it.module.name }
        .thenBy(OptionalModuleDependency::feature)
        .thenBy { it.module.stability }
        .thenBy { it.module.version },
    ),
    completeSelections,
  )
}

internal fun resolveModuleDependencies(modules: List<ModuleDependency>): List<ModuleDependency> =
  resolveDependencies(modules, emptyList(), emptyList()).required

private fun moduleCoordinate(
  name: String,
  stability: String,
  version: String,
) =
  ModuleDependency(
    requireModuleCoordinateName(name),
    normalizeStability(stability),
    requireModuleVersion(version),
  )

private fun normalizeCoordinate(module: ModuleDependency) =
  moduleCoordinate(module.name, module.stability, module.version)

internal fun requireModuleCoordinateName(value: String): String {
  val name = requireText(value, "module name")
  val segment = "[A-Za-z][A-Za-z0-9_]*"
  if (!name.matches(Regex("$segment\\.$segment(?:\\.$segment)*"))) {
    throw ProjectConfigurationException("module name must use Publisher.Name coordinates: $name")
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
  val stability = requireText(value, "module stability").uppercase(Locale.ROOT)
  if (stability !in setOf("STABLE", "BETA", "ALPHA")) {
    throw ProjectConfigurationException("module stability must be STABLE, BETA, or ALPHA")
  }
  return stability
}

internal fun requireModuleVersion(value: String): String {
  val version = requireText(value, "module version")
  val component = "(?:0|[1-9][0-9]*)"
  if (!version.matches(Regex("$component\\.$component\\.$component(?:-[0-9A-Za-z.-]+)?"))) {
    throw ProjectConfigurationException("module version must be an exact semantic version: $version")
  }
  return version
}
