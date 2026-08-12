/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual_xsharp.project

@XsProjectDsl
class ProjectScope internal constructor() {
  var name: String? = null
  var version: String? = null
  var stability: Stability? = null

  internal fun apply() {
    val configured = listOf(name, version, stability).count { it != null }
    if (configured == 0) return
    if (configured != 3) {
      throw ProjectConfigurationException("project name, version, and stability must be configured together")
    }
    ProjectRuntime.configureIdentity(name!!, stability!!.name, version!!)
  }
}

@XsProjectDsl
class MainSourcesScope internal constructor() {
  var srcDir: String = "Sources"
  var entry: String? = null
  internal val excludes = mutableListOf<String>()

  fun exclude(vararg patterns: String) {
    patterns.forEach { excludes += requireText(it, "main source exclude") }
  }
}

@XsProjectDsl
class TestSourcesScope internal constructor() {
  var testDir: String = "Tests"
  var framework: String? = null
  internal val excludes = mutableListOf<String>()

  fun exclude(vararg patterns: String) {
    patterns.forEach { excludes += requireText(it, "test source exclude") }
  }
}

@XsProjectDsl
class ViGetSourcesScope internal constructor() {
  var publish: Boolean = false
  internal val excludes = mutableListOf<String>()

  fun exclude(vararg patterns: String) {
    patterns.forEach { excludes += requireText(it, "ViGet source exclude") }
  }
}

@XsProjectDsl
class ProjectSourcesScope internal constructor() {
  private var main: MainSourcesScope? = null
  private var test: TestSourcesScope? = null
  private var viget: ViGetSourcesScope? = null

  fun main(block: MainSourcesScope.() -> Unit) {
    if (main != null) throw ProjectConfigurationException("sources.main may be configured only once")
    main = MainSourcesScope().apply(block)
  }

  fun test(block: TestSourcesScope.() -> Unit) {
    if (test != null) throw ProjectConfigurationException("sources.test may be configured only once")
    test = TestSourcesScope().apply(block)
  }

  fun viget(block: ViGetSourcesScope.() -> Unit) {
    if (viget != null) throw ProjectConfigurationException("sources.viget may be configured only once")
    viget = ViGetSourcesScope().apply(block)
  }

  internal fun apply() {
    val mainSources = main ?: throw ProjectConfigurationException("sources.main is required")
    val entry = mainSources.entry ?: throw ProjectConfigurationException("sources.main.entry is required")
    ProjectRuntime.configureEntry(entry)
    ProjectRuntime.configureMainSources {
      include(requireText(mainSources.srcDir, "sources.main.srcDir"))
      exclude(*mainSources.excludes.toTypedArray())
    }
    test?.let { testSources ->
      ProjectRuntime.configureTestSources {
        include(requireText(testSources.testDir, "sources.test.testDir"))
        exclude(*testSources.excludes.toTypedArray())
        testSources.framework?.let(::framework)
      }
    }
    viget?.let { publishing ->
      ProjectRuntime.configurePublishing(publishing.publish, publishing.excludes)
    }
  }
}

@XsProjectDsl
class OutputDirectoriesScope internal constructor() {
  var release: String = "build/release"
  var debug: String = "build/debug"

  internal fun apply() {
    ProjectRuntime.configureOutputDirectories(release, debug)
  }
}

@XsProjectDsl
class TargetsScope internal constructor() {
  private val values = mutableListOf<String>()

  fun target(vararg triples: String) {
    triples.forEach { value ->
      val target = requireText(value, "target triple")
      if (!target.matches(Regex("[A-Za-z0-9_+.]+(?:-[A-Za-z0-9_+.]+){2,}"))) {
        throw ProjectConfigurationException("invalid target triple: $target")
      }
      values += target
    }
  }

  internal fun apply() {
    ProjectRuntime.configureTargets(values)
  }
}

@XsProjectDsl
class AuthorsScope internal constructor() {
  private val values = mutableListOf<Array<String>>()

  fun author(name: String, email: String) {
    values += arrayOf(requireText(name, "author name"), requireText(email, "author email"))
  }

  internal fun apply() {
    if (values.isNotEmpty()) ProjectRuntime.configureAuthors(*values.toTypedArray())
  }
}

@XsProjectDsl
class PmlScope internal constructor() {
  var enabled: Boolean = true

  internal fun apply() {
    ProjectRuntime.configurePml(enabled)
  }
}

@XsProjectDsl
class WorkspaceScope internal constructor(private val name: String) {
  var path: String? = null

  internal fun build(): Workspace {
    val configuredPath = path ?: throw ProjectConfigurationException("workspace '$name' requires path")
    return Workspace(name, requireText(configuredPath, "workspace path"))
  }
}

@XsProjectDsl
class WorkspacesScope internal constructor() {
  private val values = mutableListOf<Workspace>()

  fun workspace(name: String, block: WorkspaceScope.() -> Unit) {
    val normalized = requireModuleSegment(name, "workspace name")
    values += WorkspaceScope(normalized).apply(block).build()
  }

  internal fun apply() {
    ProjectRuntime.configureWorkspaces(values)
  }
}

@XsProjectDsl
class DependencyDeclarationScope internal constructor(
  private val publisher: String,
) {
  var name: String? = null
  var version: String? = null
  var stability: Stability = Stability.STABLE
  var optional: String? = null
  private val features = linkedMapOf<String, Boolean>()

  fun feature(name: String, block: DependencyFeatureDeclarationScope.() -> Unit) {
    val normalized = requireFeatureName(name)
    features[normalized] = DependencyFeatureDeclarationScope().apply(block).enabled
  }

  internal fun applyTo(scope: DependenciesScope) {
    val packageName = requireModuleSegment(name ?: throw ProjectConfigurationException("dependency requires name"), "dependency name")
    val packageVersion = requirePackageVersion(version ?: throw ProjectConfigurationException("dependency requires version"))
    val optionalFeature = optional
    if (optionalFeature == null) {
      scope.add(packageDependency(publisher, packageName, stability, packageVersion))
    } else {
      val feature = requireFeatureName(optionalFeature)
      val dependency = packageDependency(publisher, packageName, stability, packageVersion)
      val enabled = features[feature] ?: false
      scope.addOptional(feature, dependency, enabled)
    }
  }
}

@XsProjectDsl
class DependencyFeatureDeclarationScope internal constructor() {
  var enabled: Boolean = false
}

fun project(block: ProjectScope.() -> Unit) = ProjectScope().apply(block).apply()

fun sources(block: ProjectSourcesScope.() -> Unit) = ProjectSourcesScope().apply(block).apply()

fun outdirs(block: OutputDirectoriesScope.() -> Unit) = OutputDirectoriesScope().apply(block).apply()

fun targets(block: TargetsScope.() -> Unit) = TargetsScope().apply(block).apply()

fun authors(block: AuthorsScope.() -> Unit) = AuthorsScope().apply(block).apply()

fun pml(block: PmlScope.() -> Unit) = PmlScope().apply(block).apply()

fun workspaces(block: WorkspacesScope.() -> Unit) = WorkspacesScope().apply(block).apply()

fun DependenciesScope.dependency(
  publisher: String,
  block: DependencyDeclarationScope.() -> Unit,
) = DependencyDeclarationScope(publisher).apply(block).applyTo(this)

fun emitProject() = ProjectOutput.emit(ProjectRuntime.build())
