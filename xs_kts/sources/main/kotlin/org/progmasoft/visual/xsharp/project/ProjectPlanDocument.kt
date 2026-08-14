/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import kotlinx.serialization.Serializable

private const val PROJECT_PLAN_FORMAT = "visual-xsharp-project-plan"
private const val PROJECT_PLAN_VERSION = 4

/**
 * Stable wire representation of [ProjectPlan]. Keeping the transport schema separate from the
 * mutable DSL model prevents implementation-only fields from leaking into the native driver.
 */
@Serializable
internal data class ProjectPlanDocument(
  val format: String = PROJECT_PLAN_FORMAT,
  val version: Int = PROJECT_PLAN_VERSION,
  val project: ProjectIdentityDocument?,
  val compiler: CompilerDocument,
  val outdirs: OutputDirectoriesDocument,
  val targets: List<String>,
  val authors: List<AuthorDocument>,
  val pml: PmlDocument,
  val dependencies: List<DependencyDocument>,
  val plugins: List<PluginDocument>,
  val workspaces: List<WorkspaceDocument>,
  val sources: SourcesDocument,
) {
  companion object {
    fun from(plan: ProjectPlan): ProjectPlanDocument =
      ProjectPlanDocument(
        project = plan.identity?.let(ProjectIdentityDocument::from),
        compiler = CompilerDocument.from(plan.compiler),
        outdirs =
          OutputDirectoriesDocument(
            release = plan.releaseOutputDirectory,
            debug = plan.debugOutputDirectory,
          ),
        targets = plan.targets,
        authors = plan.authors.map(AuthorDocument::from),
        pml = PmlDocument(enabled = plan.pmlEnabled),
        dependencies = dependenciesFrom(plan),
        plugins = plan.plugins.map(PluginDocument::from),
        workspaces = plan.workspaces.map(WorkspaceDocument::from),
        sources = SourcesDocument.from(plan),
      )

    private fun dependenciesFrom(plan: ProjectPlan): List<DependencyDocument> =
      buildList(
        plan.requiredDependencies.size +
          plan.optionalDependencies.size +
          plan.localDependencies.size
      ) {
        plan.requiredDependencies.mapTo(this, DependencyDocument::required)
        plan.optionalDependencies.mapTo(this) { declaration ->
          val enabled =
            plan.dependencyFeatures.any { selection ->
              selection.packageName == declaration.dependency.coordinate &&
                selection.feature == declaration.feature &&
                selection.enabled
            }
          DependencyDocument.optional(declaration, enabled)
        }
        plan.localDependencies.mapTo(this, DependencyDocument::local)
      }
  }
}

@Serializable
internal data class ProjectIdentityDocument(
  val name: String,
  val version: String,
  val stability: String,
) {
  companion object {
    fun from(identity: ProjectIdentity) =
      ProjectIdentityDocument(identity.name, identity.version, identity.channel)
  }
}

@Serializable
internal data class CompilerDocument(
  val version: String,
  val standard: String,
  val backend: String,
  val buildMode: String,
  val emit: String,
  val warnings: String,
  val warningsAsErrors: Boolean,
  val experimentalWarnings: Boolean,
  val shadowWarnings: Boolean,
  val undefinedWarnings: Boolean,
  val unsafe: UnsafeCompilerDocument,
  val llvm: LlvmDocument,
) {
  companion object {
    fun from(settings: CompilerSettings): CompilerDocument =
      CompilerDocument(
        version = settings.version,
        standard = settings.standard,
        backend = settings.backend.name.lowercase(),
        buildMode = settings.buildMode.name.lowercase(),
        emit = settings.emit.name.lowercase(),
        warnings = settings.warningLevel.name.lowercase(),
        warningsAsErrors = settings.warningsAsErrors,
        experimentalWarnings = settings.experimentalWarnings,
        shadowWarnings = settings.shadowWarnings,
        undefinedWarnings = settings.undefinedWarnings,
        unsafe =
          UnsafeCompilerDocument(
            xppOptimizationPasses = settings.xppOptimizationPasses,
            xmmOptimizationPasses = settings.xmmOptimizationPasses,
            typeSafeFormat = settings.typeSafeFormat,
          ),
        llvm =
          LlvmDocument(
            optLevel =
              (settings.llvmOptLevel
                  ?: if (settings.buildMode == BuildMode.DEBUG) LlvmOptLevel.O0
                  else LlvmOptLevel.O3)
                .name
                .lowercase(),
            compiler = settings.llvmCompiler.name.lowercase(),
            lto = settings.llvmLto.name.lowercase(),
          ),
      )
  }
}

@Serializable
internal data class UnsafeCompilerDocument(
  val xppOptimizationPasses: Boolean,
  val xmmOptimizationPasses: Boolean,
  val typeSafeFormat: Boolean,
)

@Serializable
internal data class LlvmDocument(
  val optLevel: String,
  val compiler: String,
  val lto: String,
)

@Serializable internal data class OutputDirectoriesDocument(val release: String, val debug: String)

@Serializable
internal data class AuthorDocument(val user: String, val mail: String) {
  companion object {
    fun from(author: Author) = AuthorDocument(author.user, author.mail)
  }
}

@Serializable internal data class PmlDocument(val enabled: Boolean)

@Serializable
internal data class DependencyDocument(
  val source: String,
  val publisher: String? = null,
  val name: String? = null,
  val version: String? = null,
  val stability: String? = null,
  val path: String? = null,
  val optional: String? = null,
  val features: Map<String, Boolean> = emptyMap(),
) {
  companion object {
    fun required(dependency: PackageDependency) = from(dependency)

    fun local(dependency: LocalPackageDependency) =
      DependencyDocument(source = "local", path = dependency.path)

    fun optional(declaration: OptionalPackageDependency, enabled: Boolean) =
      from(
        dependency = declaration.dependency,
        optional = declaration.feature,
        features = mapOf(declaration.feature to enabled),
      )

    private fun from(
      dependency: PackageDependency,
      optional: String? = null,
      features: Map<String, Boolean> = emptyMap(),
    ) =
      DependencyDocument(
        source = "viget",
        publisher = dependency.publisher,
        name = dependency.name,
        version = dependency.version,
        stability = dependency.stability.name,
        optional = optional,
        features = features,
      )
  }
}

@Serializable
internal data class PluginDocument(
  val publisher: String,
  val name: String,
  val version: String,
  val apiVersion: Int,
  val sha256: String,
  val extensions: List<String>,
  val contributions: Map<String, String>,
) {
  companion object {
    fun from(plugin: PluginPlanEntry) =
      PluginDocument(
        publisher = plugin.publisher,
        name = plugin.name,
        version = plugin.version,
        apiVersion = plugin.apiVersion,
        sha256 = plugin.sha256,
        extensions = plugin.extensions.sorted(),
        contributions = plugin.contributions.toSortedMap(),
      )
  }
}

@Serializable
internal data class WorkspaceDocument(val name: String, val path: String) {
  companion object {
    fun from(workspace: Workspace) = WorkspaceDocument(workspace.name, workspace.path)
  }
}

@Serializable
internal data class SourcesDocument(
  val viget: PublishedSourcesDocument,
  val main: MainSourcesDocument,
  val tests: List<TestSuiteDocument>,
) {
  companion object {
    fun from(plan: ProjectPlan) =
      SourcesDocument(
        viget = PublishedSourcesDocument(plan.publishSources, plan.publishExcludes),
        main =
          MainSourcesDocument(
            srcDir = plan.sourceIncludes.singleOrNull() ?: "Sources",
            entry = plan.entry,
            exclude = plan.sourceExcludes,
          ),
        tests = plan.testSuites.map(TestSuiteDocument::from),
      )
  }
}

@Serializable
internal data class PublishedSourcesDocument(val publish: Boolean, val exclude: List<String>?)

@Serializable
internal data class MainSourcesDocument(
  val srcDir: String,
  val entry: String,
  val exclude: List<String>?,
)

@Serializable
internal data class TestSuiteDocument(
  val name: String,
  val testDir: String,
  val framework: String?,
  val exclude: List<String>?,
) {
  companion object {
    fun from(suite: TestSuite) =
      TestSuiteDocument(suite.name, suite.testDir, suite.framework, suite.exclude)
  }
}
