/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import java.util.Locale

@DslMarker annotation class XsProjectDsl

@XsProjectDsl
class SourcesScope internal constructor() {
  internal val includes = mutableListOf<String>()
  internal var excludes: MutableList<String>? = null

  fun include(pattern: String) {
    val root = requireText(pattern, "source include")
    if (root.any { character -> character in "*?" }) {
      throw ProjectConfigurationException("source include must name a directory, not a glob: $root")
    }
    includes += root
  }

  fun exclude(vararg patterns: String) {
    val configured = excludes ?: mutableListOf<String>().also { excludes = it }
    patterns.forEach { pattern -> configured += requireText(pattern, "source exclude") }
  }
}

@XsProjectDsl
class CompilerScope internal constructor(private val settings: CompilerSettings) {
  var version: String
    get() = settings.version
    set(value) {
      settings.version = requireText(value, "compiler version")
    }

  var standard: String
    get() = settings.standard
    set(value) {
      val normalized = requireText(value, "language standard")
      if (normalized != "latest" && normalized != "26") {
        throw ProjectConfigurationException("language standard must be 26 or latest")
      }
      settings.standard = normalized
    }

  var backend: Backend
    get() = settings.backend
    set(value) {
      settings.backend = value
    }

  var buildMode: BuildMode
    get() = settings.buildMode
    set(value) {
      settings.buildMode = value
    }

  var emit: Emit
    get() = settings.emit
    set(value) {
      settings.emit = value
    }

  var warningsAsErrors: Boolean
    get() = settings.warningsAsErrors
    set(value) {
      settings.warningsAsErrors = value
    }

  var warnings: Warnings
    get() = org.progmasoft.visual.xsharp.project.Warnings.valueOf(settings.warningLevel.name)
    set(value) {
      settings.warningLevel = WarningLevel.valueOf(value.name)
    }

  var experimentalWarnings: Boolean
    get() = settings.experimentalWarnings
    set(value) {
      settings.experimentalWarnings = value
    }

  var shadowWarnings: Boolean
    get() = settings.shadowWarnings
    set(value) {
      settings.shadowWarnings = value
    }

  var undefinedWarnings: Boolean
    get() = settings.undefinedWarnings
    set(value) {
      settings.undefinedWarnings = value
    }

  fun unsafe(block: UnsafeCompilerScope.() -> Unit) {
    UnsafeCompilerScope(settings).apply(block)
  }

  fun llvm(block: LlvmCompilerScope.() -> Unit) {
    LlvmCompilerScope(settings).apply(block)
  }

  internal fun warnings(level: String) {
    settings.warningLevel =
      try {
        WarningLevel.valueOf(level.uppercase(Locale.ROOT))
      } catch (_: IllegalArgumentException) {
        throw ProjectConfigurationException("unknown warning level '$level'")
      }
  }

  internal fun werror(enabled: Boolean) {
    settings.warningsAsErrors = enabled
  }
}

@XsProjectDsl
class UnsafeCompilerScope internal constructor(private val settings: CompilerSettings) {
  var xppOptimizationPasses: Boolean
    get() = settings.xppOptimizationPasses
    set(value) {
      settings.xppOptimizationPasses = value
    }

  var xmmOptimizationPasses: Boolean
    get() = settings.xmmOptimizationPasses
    set(value) {
      settings.xmmOptimizationPasses = value
    }

  var typeSafeFormat: Boolean
    get() = settings.typeSafeFormat
    set(value) {
      settings.typeSafeFormat = value
    }
}

@XsProjectDsl
class LlvmCompilerScope internal constructor(private val settings: CompilerSettings) {
  var optLevel: LlvmOptLevel
    get() =
      settings.llvmOptLevel
        ?: if (settings.buildMode == BuildMode.DEBUG) LlvmOptLevel.O0 else LlvmOptLevel.O3
    set(value) {
      settings.llvmOptLevel = value
    }

  var compiler: LlvmCompiler
    get() = settings.llvmCompiler
    set(value) {
      settings.llvmCompiler = value
    }

  var lto: LlvmLto
    get() = settings.llvmLto
    set(value) {
      settings.llvmLto = value
    }
}

class ProjectContext internal constructor(val host: Host = detectHost()) {
  private var identity: ProjectIdentity? = null
  private val authors = mutableListOf<Author>()
  private val dependencies = mutableListOf<PackageDependency>()
  private val optionalDependencies = mutableListOf<OptionalPackageDependency>()
  private val dependencyFeatures = mutableListOf<PackageFeatureSelection>()
  private val localDependencies = mutableListOf<LocalPackageDependency>()
  private var entry: String? = null
  private var releaseOutputDirectory = "build/release"
  private var debugOutputDirectory = "build/debug"
  private val targets = mutableListOf<String>()
  private val workspaces = mutableListOf<Workspace>()
  private var pmlEnabled = true
  private var publishSources = false
  private var publishExcludes: List<String>? = null
  private val sourceIncludes = mutableListOf<String>()
  private var sourceExcludes: List<String>? = null
  private val testSuites = mutableListOf<TestSuite>()
  private val compilerSettings = CompilerSettings()

  internal fun configureIdentity(
    name: String?,
    stability: String?,
    version: String?,
  ) {
    if (identity != null) throw ProjectConfigurationException("project may be configured only once")
    identity =
      ProjectIdentity(
        name?.let { requireText(it, "project name") },
        stability?.let { requireText(it, "project stability") },
        version?.let { requireText(it, "project version") },
      )
  }

  internal fun configureEntry(value: String) {
    entry = requireText(value, "sources.main.entry")
  }

  internal fun configureOutputDirectories(
    release: String,
    debug: String,
  ) {
    releaseOutputDirectory = requireText(release, "release output directory")
    debugOutputDirectory = requireText(debug, "debug output directory")
  }

  internal fun configureTargets(values: List<String>) {
    targets.clear()
    targets += values.distinct()
  }

  internal fun configureWorkspaces(values: List<Workspace>) {
    workspaces.clear()
    workspaces += values.distinctBy(Workspace::name)
  }

  internal fun configurePml(enabled: Boolean) {
    pmlEnabled = enabled
  }

  internal fun configurePublishing(
    publish: Boolean,
    excludes: List<String>?,
  ) {
    publishSources = publish
    publishExcludes = excludes?.distinct()
  }

  internal fun configureAuthors(entries: List<Author>) {
    authors += entries
  }

  fun dependencies(block: DependenciesScope.() -> Unit) {
    val scope = DependenciesScope().apply(block)
    val validated =
      validateDependencies(
        dependencies + scope.required,
        optionalDependencies + scope.optional,
        dependencyFeatures + scope.selections,
        localDependencies + scope.local,
      )
    dependencies.clear()
    dependencies += validated.required
    optionalDependencies.clear()
    optionalDependencies += validated.optional
    dependencyFeatures.clear()
    dependencyFeatures += validated.features
    localDependencies.clear()
    localDependencies += validated.local
  }

  internal fun configureMainSources(block: SourcesScope.() -> Unit) {
    val scope = SourcesScope().apply(block)
    sourceIncludes += scope.includes
    scope.excludes?.let { sourceExcludes = it.distinct() }
  }

  internal fun configureTestSuites(suites: List<TestSuite>) {
    val duplicates = suites.groupingBy(TestSuite::name).eachCount().filterValues { it > 1 }.keys
    if (duplicates.isNotEmpty()) {
      throw ProjectConfigurationException(
        "test suite names must be unique: ${duplicates.sorted().joinToString()}"
      )
    }
    testSuites.clear()
    testSuites += suites
  }

  fun compiler(block: CompilerScope.() -> Unit) {
    CompilerScope(compilerSettings).apply(block)
  }

  fun build(): ProjectPlan {
    val configuredEntry =
      entry ?: throw ProjectConfigurationException("sources.main.entry is required")
    if (!configuredEntry.matches(Regex("[A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)+"))) {
      throw ProjectConfigurationException(
        "sources.main.entry must be a qualified type name: $configuredEntry"
      )
    }
    val project = identity
    if (publishSources) {
      val missingFields = buildList {
        if (project?.name == null) add("name")
        if (project?.version == null) add("version")
        if (project?.stability == null) add("stability")
      }
      if (missingFields.isNotEmpty()) {
        throw ProjectConfigurationException(
          "sources.viget.publish requires every project field; missing: ${missingFields.joinToString()}"
        )
      }
    }
    val dependencyManifest =
      validateDependencies(
        dependencies,
        optionalDependencies,
        dependencyFeatures,
        localDependencies,
      )
    val effectiveSourceIncludes = sourceIncludes.ifEmpty { listOf("Sources") }
    val plan =
      ProjectPlan(
        project,
        authors.toList(),
        dependencyManifest.required,
        dependencyManifest.optional,
        dependencyManifest.features,
        dependencyManifest.local,
        configuredEntry,
        releaseOutputDirectory,
        debugOutputDirectory,
        targets.toList(),
        workspaces.toList(),
        pmlEnabled,
        publishSources,
        publishExcludes,
        effectiveSourceIncludes.distinct(),
        sourceExcludes,
        testSuites.toList(),
        compilerSettings,
        emptyList(),
      )
    return PluginRuntime.finish(plan)
  }
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

internal object ProjectRuntime {
  private var context = ProjectContext()

  fun reset() {
    PluginRuntime.reset()
    context = ProjectContext()
  }

  fun configureIdentity(
    name: String?,
    stability: String?,
    version: String?,
  ) = context.configureIdentity(name, stability, version)

  fun configureEntry(value: String) = context.configureEntry(value)

  fun configureOutputDirectories(
    release: String,
    debug: String,
  ) = context.configureOutputDirectories(release, debug)

  fun configureTargets(values: List<String>) = context.configureTargets(values)

  fun configureWorkspaces(values: List<Workspace>) = context.configureWorkspaces(values)

  fun configurePml(enabled: Boolean) = context.configurePml(enabled)

  fun configurePublishing(
    publish: Boolean,
    excludes: List<String>?,
  ) = context.configurePublishing(publish, excludes)

  fun configureAuthors(entries: List<Author>) = context.configureAuthors(entries)

  fun dependencies(block: DependenciesScope.() -> Unit) = context.dependencies(block)

  fun configureMainSources(block: SourcesScope.() -> Unit) = context.configureMainSources(block)

  fun configureTestSuites(suites: List<TestSuite>) = context.configureTestSuites(suites)

  fun compiler(block: CompilerScope.() -> Unit) = context.compiler(block)

  fun build() = context.build()

  val host
    get() = context.host
}

val OS
  get() = ProjectRuntime.host.os
val FAMILY
  get() = ProjectRuntime.host.family
val ARCH
  get() = ProjectRuntime.host.architecture
val LINUX
  get() = OperatingSystem.LINUX
val MACOS
  get() = OperatingSystem.MACOS
val WINDOWS: Any
  get() = OperatingSystem.WINDOWS
val FREEBSD
  get() = OperatingSystem.FREEBSD
val OPENBSD
  get() = OperatingSystem.OPENBSD
val NETBSD
  get() = OperatingSystem.NETBSD
val UNIX
  get() = OperatingSystemFamily.UNIX
val BSD
  get() = OperatingSystemFamily.BSD
val X86_64
  get() = Architecture.X86_64
val AARCH64
  get() = Architecture.AARCH64
val ARMV7H
  get() = Architecture.ARMV7H
val RISCV64
  get() = Architecture.RISCV64

fun cfg(condition: Boolean) = condition

fun panic(message: String): Nothing = throw ProjectAbort(message)

fun eprint(message: Any?) = System.err.print(message)

fun eprintln(message: Any?) = System.err.println(message)

fun dependencies(block: DependenciesScope.() -> Unit) = ProjectRuntime.dependencies(block)

fun compiler(block: CompilerScope.() -> Unit) = ProjectRuntime.compiler(block)
