/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.nio.file.Path
import java.util.Locale

@DslMarker
annotation class XsProjectDsl

@XsProjectDsl
class SourcesScope internal constructor() {
  internal val includes = mutableListOf<String>()
  internal val excludes = mutableListOf<String>()
  internal var excludesConfigured = false
  internal var filters: List<String>? = null

  fun include(pattern: String) {
    val root = requireText(pattern, "source include")
    if (root.any { character -> character in "*?" }) {
      throw ProjectConfigurationException("source include must name a directory, not a glob: $root")
    }
    includes += root
  }

  fun exclude(vararg patterns: String) {
    excludesConfigured = true
    patterns.forEach { pattern -> excludes += requireText(pattern, "source exclude") }
  }

  fun filter(paths: List<String>) {
    filters = paths.map { path -> requireText(path, "source filter") }
  }
}

@XsProjectDsl
class ModuleMembersScope internal constructor() {
  internal val paths = mutableListOf<String>()

  fun add(path: String) {
    paths += requireText(path, "module member path")
  }
}

@XsProjectDsl
class SubmoduleScope internal constructor() {
  internal var moduleName: String? = null
  internal val paths = mutableListOf<String>()

  fun name(value: String) {
    if (moduleName != null) throw ProjectConfigurationException("submodule name(...) may be called only once")
    moduleName = requireModuleSegment(value, "submodule name")
  }

  fun add(path: String) {
    paths += requireText(path, "module member path")
  }
}

@XsProjectDsl
class ModuleScope internal constructor() {
  internal val includes = mutableListOf<String>()
  internal val excludes = mutableListOf<String>()
  internal var excludesConfigured = false
  internal var filters: List<String>? = null
  internal var moduleName: String? = null
  internal val paths = mutableListOf<String>()
  internal val submodules = mutableListOf<SubmoduleScope>()

  fun include(pattern: String) {
    val root = requireText(pattern, "module include")
    if (root.any { character -> character in "*?" }) {
      throw ProjectConfigurationException("module include must name a directory, not a glob: $root")
    }
    includes += root
  }

  fun exclude(vararg patterns: String) {
    excludesConfigured = true
    patterns.forEach { pattern -> excludes += requireText(pattern, "module exclude") }
  }

  fun filter(paths: List<String>) {
    filters = paths.map { path -> requireText(path, "module filter") }
  }

  fun name(value: String) {
    if (moduleName != null) throw ProjectConfigurationException("module name(...) may be called only once")
    moduleName = requireModuleSegment(value, "module name")
  }

  fun add(path: String) {
    paths += requireText(path, "module member path")
  }

  fun members(block: ModuleMembersScope.() -> Unit) {
    paths += ModuleMembersScope().apply(block).paths
  }

  fun submodule(block: SubmoduleScope.() -> Unit) {
    submodules += SubmoduleScope().apply(block)
  }
}

@XsProjectDsl
class DependenciesScope internal constructor() {
  internal val modules = mutableListOf<ModuleDependency>()

  fun addModule(
    name: String,
    stability: String,
    version: String,
  ) {
    modules +=
      ModuleDependency(
        requireText(name, "module name"),
        requireText(stability, "module stability"),
        requireText(version, "module version"),
      )
  }
}

@XsProjectDsl
class TestScope internal constructor() {
  internal val includes = mutableListOf<String>()
  internal val excludes = mutableListOf<String>()
  internal var excludesConfigured = false
  internal var filters: List<String>? = null
  internal var framework: String? = null

  fun include(path: String) {
    val root = requireText(path, "test include")
    if (root.any { character -> character in "*?" }) {
      throw ProjectConfigurationException("test include must name a directory, not a glob: $root")
    }
    includes += root
  }

  fun exclude(vararg patterns: String) {
    excludesConfigured = true
    patterns.forEach { pattern -> excludes += requireText(pattern, "test exclude") }
  }

  fun filter(paths: List<String>) {
    filters = paths.map { path -> requireText(path, "test filter") }
  }

  fun framework(name: String) {
    framework = requireText(name, "test framework")
  }
}

@XsProjectDsl
class CompilerScope internal constructor(
  private val settings: CompilerSettings,
) {
  fun warnings(level: String) {
    settings.warningLevel =
      try {
        WarningLevel.valueOf(level.uppercase(Locale.ROOT))
      } catch (_: IllegalArgumentException) {
        throw ProjectConfigurationException("unknown warning level '$level'")
      }
  }

  fun werror(enabled: Boolean) {
    settings.warningsAsErrors = enabled
  }

  fun verbose(enabled: Boolean) {
    settings.verbose = enabled
  }
}

class ProjectContext internal constructor(
  val host: Host = detectHost(),
  state: ProjectState? = null,
) {
  private var identity: ProjectIdentity? = state?.identity
  private val variables =
    (state?.variables
      ?: mapOf(
        "BUILD_MODE" to listOf("Release"),
        "RELEASE_OUTDIR" to listOf("build/release"),
        "DEBUG_OUTDIR" to listOf("build/debug"),
        "XS_BACKEND" to listOf("LLVM"),
        "XS_LLVM_LTO" to listOf("true"),
        "XS_LLVM_OPT_LEVEL" to listOf("3"),
        "XS_EXTENSION" to listOf("xs"),
        "PUBLISH" to listOf("false"),
      )).toMutableMap()
  private val authors = state?.authors?.toMutableList() ?: mutableListOf()
  private val binaries = state?.binaries?.toMutableList() ?: mutableListOf()
  private val libraries = state?.libraries?.toMutableList() ?: mutableListOf()
  private val modules = state?.modules?.toMutableList() ?: mutableListOf()
  private val sourceIncludes = state?.sourceIncludes?.toMutableList() ?: mutableListOf()
  private val sourceExcludes = state?.sourceExcludes?.toMutableList() ?: mutableListOf("*/**")
  private var sourceFilters = state?.sourceFilters
  private val moduleIncludes = state?.moduleIncludes?.toMutableList() ?: mutableListOf()
  private val moduleExcludes = state?.moduleExcludes?.toMutableList() ?: mutableListOf("*/**")
  private var moduleFilters = state?.moduleFilters
  private val moduleSources = state?.moduleSources?.toMutableList() ?: mutableListOf()
  private val testIncludes = state?.testIncludes?.toMutableList() ?: mutableListOf()
  private val testExcludes = state?.testExcludes?.toMutableList() ?: mutableListOf("*/**")
  private var testFilters = state?.testFilters
  private var testFramework: String? = state?.testFramework
  private val compilerSettings = state?.compiler ?: CompilerSettings()

  fun project(
    name: String,
    channel: String,
    version: String,
  ) {
    if (identity != null) throw ProjectConfigurationException("project(...) may be called only once")
    identity =
      ProjectIdentity(
        requireText(name, "project name"),
        requireText(channel, "release channel"),
        requireText(version, "project version"),
      )
  }

  fun set(
    name: String,
    vararg values: Any,
  ) {
    if (values.isEmpty()) throw ProjectConfigurationException("set(...) requires at least one value")
    val key = requireText(name, "variable name")
    val flattened =
      if (values.size == 1 && values.single() is List<*>) {
        (values.single() as List<*>).also {
          if (it.isEmpty()) throw ProjectConfigurationException("set(...) requires at least one value")
        }
      } else {
        values.toList()
      }
    if (key == "BINARY" || key == "LIBRARY") {
      val targets = parseArtifactTargets(key, flattened)
      val destination = if (key == "BINARY") binaries else libraries
      destination.clear()
      destination += targets
      return
    }
    if (key == "PUBLISH" && (flattened.size != 1 || flattened.single() !is Boolean)) {
      throw ProjectConfigurationException("PUBLISH must be exactly one boolean")
    }
    val normalized =
      flattened.map { value ->
        when (value) {
          is String -> requireText(value, "variable value")
          is Boolean -> value.toString()
          else -> throw ProjectConfigurationException("project variable values must be strings or booleans")
        }
      }
    validateReservedVariable(key, normalized)
    variables[key] = normalized
  }

  fun get(name: String): String {
    val key = requireText(name, "variable name")
    val project = identity
    if (key == "PROJECT" && project != null) return "${project.name}, ${project.channel}, ${project.version}"
    return getAll(key).joinToString(", ")
  }

  fun getAll(name: String): List<String> {
    val key = requireText(name, "variable name")
    val project = identity
    if (key == "PROJECT" && project != null) return listOf("${project.name}, ${project.channel}, ${project.version}")
    if (key == "BINARY") return binaries.map { target -> "${target.name}:${target.path}" }
    if (key == "LIBRARY") return libraries.map { target -> "${target.name}:${target.path}" }
    return variables[key]?.toList() ?: throw ProjectConfigurationException("unknown project variable '$key'")
  }

  fun authors(vararg entries: Array<String>) {
    entries.forEach { entry ->
      if (entry.size != 2) throw ProjectConfigurationException("each author requires a name and email")
      authors += Author(requireText(entry[0], "author name"), requireText(entry[1], "author email"))
    }
  }

  fun dependencies(block: DependenciesScope.() -> Unit) {
    val resolved = resolveModuleDependencies(modules + DependenciesScope().apply(block).modules)
    modules.clear()
    modules += resolved
  }

  fun source(block: SourcesScope.() -> Unit) {
    val scope = SourcesScope().apply(block)
    sourceIncludes += scope.includes
    if (scope.excludesConfigured) {
      sourceExcludes.clear()
      sourceExcludes += scope.excludes
    }
    if (scope.filters != null) sourceFilters = scope.filters
  }

  fun module(block: ModuleScope.() -> Unit) {
    val scope = ModuleScope().apply(block)
    if (scope.excludesConfigured) {
      moduleExcludes.clear()
      moduleExcludes += scope.excludes
    }
    if (scope.filters != null) moduleFilters = scope.filters
    if (scope.includes.isNotEmpty()) {
      if (scope.moduleName != null || scope.paths.isNotEmpty() || scope.submodules.isNotEmpty()) {
        throw ProjectConfigurationException("module include and module definition cannot share one module block")
      }
      moduleIncludes += scope.includes
      return
    }
    if (scope.moduleName == null && scope.paths.isEmpty() && scope.submodules.isEmpty()) return
    val name = scope.moduleName ?: throw ProjectConfigurationException("module definition requires name(...)")
    scope.paths.forEach { path -> moduleSources += ModuleSource(name, path) }
    scope.submodules.forEach { submodule ->
      val child = submodule.moduleName ?: throw ProjectConfigurationException("submodule requires name(...)")
      submodule.paths.forEach { path -> moduleSources += ModuleSource("$name::$child", path) }
    }
  }

  fun test(block: TestScope.() -> Unit) {
    val scope = TestScope().apply(block)
    testIncludes += scope.includes
    if (scope.excludesConfigured) {
      testExcludes.clear()
      testExcludes += scope.excludes
    }
    if (scope.filters != null) testFilters = scope.filters
    testFramework = scope.framework
  }

  fun compiler(block: CompilerScope.() -> Unit) {
    CompilerScope(compilerSettings).apply(block)
  }

  fun snapshot() =
    ProjectState(
      identity,
      variables.toMap(),
      binaries.toList(),
      libraries.toList(),
      authors.toList(),
      modules.toList(),
      sourceIncludes.toList(),
      sourceExcludes.toList(),
      sourceFilters,
      moduleIncludes.toList(),
      moduleExcludes.toList(),
      moduleFilters,
      moduleSources.toList(),
      testIncludes.toList(),
      testExcludes.toList(),
      testFilters,
      testFramework,
      compilerSettings.copy(),
    )

  fun build(): ProjectPlan {
    val project = identity ?: throw ProjectConfigurationException("project(...) is required")
    val resolvedModules = resolveModuleDependencies(modules)
    val effectiveSourceIncludes = sourceIncludes.ifEmpty { listOf("Sources") }
    val backend = variables.getValue("XS_BACKEND").single()
    val effectiveVariables =
      if (backend == "LLVM") {
        variables.toSortedMap()
      } else {
        variables
          .filterKeys { key -> key != "XS_LLVM_LTO" && key != "XS_LLVM_OPT_LEVEL" }
          .toSortedMap()
      }
    return ProjectPlan(
      project,
      effectiveVariables,
      binaries.toList(),
      libraries.toList(),
      authors.toList(),
      resolvedModules,
      effectiveSourceIncludes.distinct(),
      sourceExcludes.distinct(),
      sourceFilters?.distinct(),
      moduleIncludes.distinct(),
      moduleExcludes.distinct(),
      moduleFilters?.distinct(),
      moduleSources.distinct(),
      testIncludes.distinct(),
      testExcludes.distinct(),
      testFilters?.distinct(),
      testFramework,
      compilerSettings,
    )
  }
}

private fun parseArtifactTargets(
  key: String,
  values: List<Any?>,
): List<ArtifactTarget> {
  val targets =
    values.map { value ->
      val fields = value as? Map<*, *> ?: throw ProjectConfigurationException("$key values must be maps")
      if (fields.keys != setOf("name", "path")) {
        throw ProjectConfigurationException("$key maps require exactly name and path")
      }
      val name = fields["name"] as? String ?: throw ProjectConfigurationException("$key name must be a string")
      val path = fields["path"] as? String ?: throw ProjectConfigurationException("$key path must be a string")
      val normalizedPath = requireText(path, "$key path").replace('\\', '/')
      val parsedPath = Path.of(normalizedPath)
      if (parsedPath.isAbsolute || normalizedPath.split('/').any { component -> component == ".." }) {
        throw ProjectConfigurationException("$key path must stay inside the project: $path")
      }
      ArtifactTarget(requireText(name, "$key name"), normalizedPath)
    }
  if (targets.map(ArtifactTarget::name).distinct().size != targets.size) {
    throw ProjectConfigurationException("$key target names must be unique")
  }
  return targets
}

private fun validateReservedVariable(
  key: String,
  values: List<String>,
) {
  when (key) {
    "XGC_ENABLED", "XPG_ENABLED" -> {
      throw ProjectConfigurationException(
        "$key is not configurable; LLVM always uses RAII and XPLR always uses XPG",
      )
    }

    "BUILD_MODE" -> {
      if (values.size != 1 || values.single() !in setOf("Release", "Debug")) {
        throw ProjectConfigurationException("BUILD_MODE must be exactly Release or Debug")
      }
    }

    "RELEASE_OUTDIR", "DEBUG_OUTDIR" -> {
      if (values.size != 1) throw ProjectConfigurationException("$key must be exactly one path")
    }

    "XS_BACKEND" -> {
      if (values.size != 1 || values.single() !in setOf("LLVM", "XPLR")) {
        throw ProjectConfigurationException("XS_BACKEND must be exactly LLVM or XPLR")
      }
    }

    "XS_LLVM_LTO" -> {
      if (values.size != 1 || values.single() !in setOf("true", "false")) {
        throw ProjectConfigurationException("XS_LLVM_LTO must be exactly true or false")
      }
    }

    "XS_LLVM_OPT_LEVEL" -> {
      if (values.size != 1 || values.single() !in setOf("0", "1", "2", "3")) {
        throw ProjectConfigurationException("XS_LLVM_OPT_LEVEL must be exactly 0, 1, 2, or 3")
      }
    }

    "XSPKG_TYPE" -> {
      val supported = setOf("xlib", "dylib", "staticlib", "cdylib", "bin")
      if (values.any { value -> value !in supported }) {
        throw ProjectConfigurationException("XSPKG_TYPE values must be xlib, dylib, staticlib, cdylib, or bin")
      }
      if (values.distinct().size != values.size) {
        throw ProjectConfigurationException("XSPKG_TYPE cannot contain duplicate values")
      }
    }
  }
}

internal fun resolveModuleDependencies(modules: List<ModuleDependency>): List<ModuleDependency> {
  val resolved = linkedMapOf<String, ModuleDependency>()
  modules.forEach { module ->
    val previous = resolved[module.name]
    if (previous != null && previous != module) {
      throw ProjectConfigurationException(
        "module '${module.name}' has conflicting stability or version coordinates",
      )
    }
    resolved[module.name] = module
  }
  return resolved.values.sortedWith(
    compareBy(ModuleDependency::name, ModuleDependency::stability, ModuleDependency::version),
  )
}

internal fun requireText(
  value: String,
  field: String,
): String {
  if (value.isBlank()) throw ProjectConfigurationException("$field cannot be empty")
  return value
}

internal fun requireModuleSegment(
  value: String,
  field: String,
): String {
  val name = requireText(value, field)
  if (!name.matches(Regex("[A-Za-z_][A-Za-z0-9_]*"))) {
    throw ProjectConfigurationException("$field must be one case-sensitive X# identifier: $name")
  }
  return name
}

object ProjectRuntime {
  private var context = ProjectContext(state = ProjectStateFile.readConfigured())

  fun reset() {
    context = ProjectContext()
  }

  fun project(
    name: String,
    channel: String,
    version: String,
  ) = context.project(name, channel, version)

  fun set(
    name: String,
    vararg values: Any,
  ) = context.set(name, *values)

  fun get(name: String) = context.get(name)

  fun getAll(name: String) = context.getAll(name)

  fun authors(vararg entries: Array<String>) = context.authors(*entries)

  fun dependencies(block: DependenciesScope.() -> Unit) = context.dependencies(block)

  fun source(block: SourcesScope.() -> Unit) = context.source(block)

  fun module(block: ModuleScope.() -> Unit) = context.module(block)

  fun test(block: TestScope.() -> Unit) = context.test(block)

  fun compiler(block: CompilerScope.() -> Unit) = context.compiler(block)

  fun build() = context.build()

  fun snapshot() = context.snapshot()

  val host get() = context.host
}

val OS get() = ProjectRuntime.host.os
val FAMILY get() = ProjectRuntime.host.family
val ARCH get() = ProjectRuntime.host.architecture
val LINUX get() = OperatingSystem.LINUX
val MACOS get() = OperatingSystem.MACOS
val WINDOWS: Any get() = OperatingSystem.WINDOWS
val FREEBSD get() = OperatingSystem.FREEBSD
val OPENBSD get() = OperatingSystem.OPENBSD
val NETBSD get() = OperatingSystem.NETBSD
val UNIX get() = OperatingSystemFamily.UNIX
val BSD get() = OperatingSystemFamily.BSD
val X86_64 get() = Architecture.X86_64
val AARCH64 get() = Architecture.AARCH64
val ARMV7H get() = Architecture.ARMV7H
val RISCV64 get() = Architecture.RISCV64

fun cfg(condition: Boolean) = condition

fun panic(message: String): Nothing = throw ProjectAbort(message)

fun project(
  name: String,
  channel: String,
  version: String,
) = ProjectRuntime.project(name, channel, version)

fun set(
  name: String,
  vararg values: Any,
) = ProjectRuntime.set(name, *values)

fun get(name: String) = ProjectRuntime.get(name)

fun getAll(name: String) = ProjectRuntime.getAll(name)

fun authors(vararg entries: Array<String>) = ProjectRuntime.authors(*entries)

fun dependencies(block: DependenciesScope.() -> Unit) = ProjectRuntime.dependencies(block)

fun source(block: SourcesScope.() -> Unit) = ProjectRuntime.source(block)

fun module(block: ModuleScope.() -> Unit) = ProjectRuntime.module(block)

fun test(block: TestScope.() -> Unit) = ProjectRuntime.test(block)

fun compiler(block: CompilerScope.() -> Unit) = ProjectRuntime.compiler(block)
