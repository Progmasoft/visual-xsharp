/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import kotlin.streams.toList

object ProjectOutput {
  private const val REGISTRY_VERSION = "visual-xsharp-sources-v1"

  fun emit(plan: ProjectPlan) {
    val root = projectRoot()
    val resolved = resolveSources(root, plan)
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

  private data class ResolvedProject(
    val sources: List<Path>,
    val tests: List<Path>,
  )

  private fun resolveSources(
    root: Path,
    plan: ProjectPlan,
  ): ResolvedProject {
    val extension = "vxs"
    val testRoots = if (plan.testIncludes.isEmpty()) defaultTestRoots(root) else plan.testIncludes
    val tests = collectRoots(root, testRoots, extension, plan.testExcludes)
    val testSet = tests.toSet()
    val sources =
      collectRoots(root, plan.sourceIncludes, extension, plan.sourceExcludes)
        .filterNot(testSet::contains)
        .sortedBy(Path::toString)
    if (sources.isEmpty()) {
      throw ProjectConfigurationException(
        "sources.main.srcDir contains no .$extension source files"
      )
    }
    return ResolvedProject(sources, tests)
  }

  private fun collectRoots(
    root: Path,
    configuredRoots: List<String>,
    extension: String,
    exclusions: List<String>?,
  ): List<Path> =
    configuredRoots
      .flatMap { configured ->
        val normalized = configured.replace('\\', '/')
        val relative = Path.of(normalized)
        if (relative.isAbsolute || normalized.split('/').any { it == ".." }) {
          throw ProjectConfigurationException(
            "source directory escapes the project root: $configured"
          )
        }
        val directory = root.resolve(relative).normalize()
        if (!directory.startsWith(root) || !Files.isDirectory(directory)) {
          throw ProjectConfigurationException("source directory does not exist: $directory")
        }
        Files.walk(directory).use { paths ->
          paths
            .filter { path -> Files.isRegularFile(path) }
            .filter { it.fileName.toString().endsWith(".$extension") }
            .filter { path -> !excluded(root, directory, path, exclusions.orEmpty()) }
            .toList()
        }
      }
      .distinct()
      .sortedBy(Path::toString)

  private fun excluded(
    root: Path,
    sourceRoot: Path,
    path: Path,
    exclusions: List<String>,
  ): Boolean {
    val projectRelative = relative(root, path)
    val sourceRelative = relative(sourceRoot, path)
    return exclusions.any { pattern ->
      val matcher = globRegex(pattern.replace('\\', '/').trimEnd('/'))
      matcher.matches(projectRelative) || matcher.matches(sourceRelative)
    }
  }

  private fun writeRegistry(
    plan: ProjectPlan,
    project: ResolvedProject,
  ) {
    val configuredOutput = System.getProperty("xs.project.sources")?.takeIf(String::isNotBlank)
    val output = configuredOutput?.let { Files.newOutputStream(Path.of(it)) } ?: System.out
    try {
      val compiler = plan.compiler
      val optLevel =
        compiler.llvmOptLevel
          ?: if (compiler.buildMode == BuildMode.DEBUG) LlvmOptLevel.O0 else LlvmOptLevel.O3
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
        )
        .forEach { writeRecord(output, it) }
      project.sources.forEach { writeRecord(output, it.toString()) }
      project.tests.forEach { writeRecord(output, it.toString()) }
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

  private fun relative(
    root: Path,
    path: Path,
  ): String = root.relativize(path).toString().replace('\\', '/')

  private fun globRegex(pattern: String): Regex {
    val regex = StringBuilder("^")
    var index = 0
    while (index < pattern.length) {
      when (val character = pattern[index]) {
        '*' -> {
          if (index + 1 < pattern.length && pattern[index + 1] == '*') {
            regex.append(".*")
            index++
          } else {
            regex.append("[^/]*")
          }
        }

        '?' -> {
          regex.append("[^/]")
        }

        else -> {
          regex.append(Regex.escape(character.toString()))
        }
      }
      index++
    }
    return Regex(regex.append("$").toString())
  }
}
