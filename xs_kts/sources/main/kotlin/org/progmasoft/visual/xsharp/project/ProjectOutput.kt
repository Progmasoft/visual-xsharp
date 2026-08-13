/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path

object ProjectOutput {
  private const val REGISTRY_VERSION = "visual-xsharp-sources-v2"

  fun emit(plan: ProjectPlan) {
    val root = projectRoot()
    val resolved = resolveRoots(root, plan)
    ProjectLockFile.write(
      root,
      validateDependencies(
        plan.requiredDependencies,
        plan.optionalDependencies,
        plan.dependencyFeatures,
        plan.localDependencies,
      ),
      plan.plugins,
    )
    when (System.getProperty("xs.project.output", "plan")) {
      "plan" -> println(PlanWriter.write(plan))
      "resolve" -> Unit
      "sources0" -> writeRegistry(plan, resolved)
      else -> throw ProjectConfigurationException("unknown project output mode")
    }
  }

  private data class ResolvedRoots(
    val sources: List<Path>,
    val tests: List<Path>,
  )

  private fun resolveRoots(
    root: Path,
    plan: ProjectPlan,
  ): ResolvedRoots {
    val testRoots = if (plan.testIncludes.isEmpty()) defaultTestRoots(root) else plan.testIncludes
    return ResolvedRoots(
      validateRoots(root, plan.sourceIncludes, "sources.main.srcDir"),
      validateRoots(root, testRoots, "sources.test.testDir"),
    )
  }

  private fun validateRoots(
    root: Path,
    configuredRoots: List<String>,
    setting: String,
  ): List<Path> =
    configuredRoots
      .map { configured ->
        val normalized = configured.replace('\\', '/')
        val relative = Path.of(normalized)
        if (relative.isAbsolute || normalized.split('/').any { it == ".." }) {
          throw ProjectConfigurationException("$setting escapes the project root: $configured")
        }
        val directory = root.resolve(relative).normalize()
        if (!directory.startsWith(root) || !Files.isDirectory(directory)) {
          throw ProjectConfigurationException("$setting directory does not exist: $directory")
        }
        directory
      }
      .distinct()
      .sortedBy(Path::toString)

  private fun writeRegistry(
    plan: ProjectPlan,
    project: ResolvedRoots,
  ) {
    val configuredOutput = System.getProperty("xs.project.sources")?.takeIf(String::isNotBlank)
    val output = configuredOutput?.let { Files.newOutputStream(Path.of(it)) } ?: System.out
    try {
      val compiler = plan.compiler
      val optLevel =
        compiler.llvmOptLevel
          ?: if (compiler.buildMode == BuildMode.DEBUG) LlvmOptLevel.O0 else LlvmOptLevel.O3
      // The DSL exports roots and exclusion policy, not a snapshot of .vxs files.
      // Namespace resolution and source discovery are compiler responsibilities;
      // neither file names nor directory layout define the entry type.
      listOf(
          REGISTRY_VERSION,
          plan.entry,
          compiler.version,
          compiler.standard,
          compiler.backend.name.lowercase(),
          compiler.buildMode.name.lowercase(),
          compiler.emit.name.lowercase(),
          compiler.warningLevel.name.lowercase(),
          compiler.warningsAsErrors.toString(),
          compiler.experimentalWarnings.toString(),
          compiler.shadowWarnings.toString(),
          compiler.undefinedWarnings.toString(),
          compiler.typeSafeFormat.toString(),
          compiler.xppOptimizationPasses.toString(),
          compiler.xmmOptimizationPasses.toString(),
          optLevel.name.removePrefix("O").lowercase(),
          compiler.llvmCompiler.name.lowercase(),
          compiler.llvmLto.name.lowercase(),
          project.sources.size.toString(),
          project.tests.size.toString(),
          plan.sourceExcludes.orEmpty().size.toString(),
          plan.testExcludes.orEmpty().size.toString(),
        )
        .forEach { writeRecord(output, it) }
      project.sources.forEach { writeRecord(output, it.toString()) }
      project.tests.forEach { writeRecord(output, it.toString()) }
      plan.sourceExcludes.orEmpty().forEach { writeRecord(output, it) }
      plan.testExcludes.orEmpty().forEach { writeRecord(output, it) }
      output.flush()
    } finally {
      if (configuredOutput != null) output.close()
    }
  }

  private fun defaultTestRoots(root: Path): List<String> =
    if (Files.isDirectory(root.resolve("Tests"))) listOf("Tests") else emptyList()

  private fun projectRoot(): Path {
    val configured =
      System.getProperty("xs.project.root")?.takeIf(String::isNotBlank)
        ?: throw ProjectConfigurationException("project root is not configured")
    return Path.of(configured).toAbsolutePath().normalize()
  }

  private fun writeRecord(
    stream: java.io.OutputStream,
    value: String,
  ) {
    stream.write(value.toByteArray(StandardCharsets.UTF_8))
    stream.write(0)
  }
}
