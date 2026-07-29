/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.isRegularFile
import kotlin.streams.toList

object ProjectOutput {
  fun emit(plan: ProjectPlan) {
    val root = projectRoot()
    val configuredPlan = plan.withFilesystemDefaults(root)
    val resolved = resolveSources(configuredPlan)
    val effectivePlan = configuredPlan.withInferredPackageTypes(resolved.sources)
    validatePackageTypeInputs(effectivePlan, resolved.sources)
    writeModuleLock(plan.modules)
    when (System.getProperty("xs.project.output", "plan")) {
      "plan" -> println(PlanWriter.write(effectivePlan))
      "resolve" -> Unit
      "sources0" -> writeSources(resolved, plan.compiler)
      else -> throw ProjectConfigurationException("unknown project output mode")
    }
  }

  fun emitModules(state: ProjectState) {
    val root = projectRoot()
    val extension = sourceExtension(state.variables)
    val moduleIncludes =
      if (state.moduleIncludes.isEmpty() && moduleRootOverride() == null) {
        defaultModuleRoots(root)
      } else {
        state.moduleIncludes
      }
    val modules = resolveModules(root, moduleIncludes, state.moduleSources, extension)
    if (state.identity != null) ModuleLockFile.write(root, state.modules)
    writeSources(ResolvedProject(emptyList(), modules, emptyList()), state.compiler)
  }

  private fun writeModuleLock(modules: List<ModuleDependency>) {
    ModuleLockFile.write(projectRoot(), modules)
  }

  private data class ResolvedProject(
    val sources: List<Path>,
    val modules: List<Pair<String, Path>>,
    val tests: List<Path>,
  )

  private fun resolveSources(plan: ProjectPlan): ResolvedProject {
    val root = projectRoot()
    val extension = sourceExtension(plan.variables)
    val tests =
      plan.testIncludes
        .flatMap { include -> expandSourceDirectory(root, include, extension) }
        .distinct()
        .sortedBy(Path::toString)
    val testSet = tests.toSet()
    val modules = resolveModules(root, plan.moduleIncludes, plan.moduleSources, extension, testSet)
    val moduleSet = modules.mapTo(mutableSetOf()) { (_, path) -> path }
    val sources =
      plan.sourceIncludes
        .flatMap { include -> expandSourceDirectory(root, include, extension) }
        .filterNot { path -> path in testSet || path in moduleSet }
        .distinct()
        .sortedWith(compareBy<Path> { path -> path.fileName.toString() != "main.$extension" }.thenBy(Path::toString))
    val mainCount = sources.count { path -> path.fileName.toString() == "main.$extension" }
    if (mainCount > 1) {
      throw ProjectConfigurationException(
        "source roots may resolve at most one case-sensitive main.$extension; found $mainCount",
      )
    }
    validateArtifactTargets(plan, root, sources, extension)
    return ResolvedProject(sources, modules, tests)
  }

  private fun resolveModules(
    root: Path,
    configuredRoots: List<String>,
    moduleSources: List<ModuleSource>,
    extension: String,
    reservedPaths: Set<Path> = emptySet(),
  ): List<Pair<String, Path>> {
    val override = System.getProperty("xs.project.moduleRoot")?.takeIf(String::isNotBlank)
    val roots = if (configuredRoots.isEmpty() && override != null) listOf(override) else configuredRoots
    if (moduleSources.isNotEmpty() && roots.isEmpty()) {
      throw ProjectConfigurationException("module sources require module.include(...) or the --module option")
    }
    val modulePool =
      roots
        .flatMap { include -> expandSourceDirectory(root, include, extension) }
        .filterNot(reservedPaths::contains)
        .distinct()
    val assigned = mutableSetOf<Path>()
    val modules =
      moduleSources.flatMap { source ->
        expandInclude(root, source.path, extension).mapNotNull { path ->
          if (path in reservedPaths) return@mapNotNull null
          if (path !in modulePool) {
            throw ProjectConfigurationException("module member is not selected by module.include(...): ${source.path}")
          }
          if (!assigned.add(path)) {
            throw ProjectConfigurationException("module member is assigned more than once: ${relative(root, path)}")
          }
          source.moduleName to path
        }
      }
    val unassigned = modulePool.filterNot(assigned::contains)
    if (unassigned.isNotEmpty()) {
      throw ProjectConfigurationException("module source has no xs.module.kts membership: ${relative(root, unassigned.first())}")
    }
    return modules.sortedWith(compareBy<Pair<String, Path>> { it.first }.thenBy { it.second.toString() })
  }

  private fun expandInclude(
    root: Path,
    include: String,
    extension: String,
  ): List<Path> {
    val normalized = include.replace('\\', '/')
    val includePath = Path.of(normalized)
    if (includePath.isAbsolute || normalized.split('/').any { component -> component == ".." }) {
      throw ProjectConfigurationException("source include escapes the project root: $include")
    }
    if (normalized.none { character -> character in "*?" }) {
      val path = root.resolve(includePath).normalize()
      if (!path.startsWith(root) || !path.isRegularFile() || !path.fileName.toString().endsWith(".$extension")) {
        throw ProjectConfigurationException("included source file does not exist: $path")
      }
      return listOf(path)
    }
    val matcher = globRegex(normalized)
    val matches =
      Files.walk(root).use { paths ->
        paths
          .filter(Path::isRegularFile)
          .filter { path -> matcher.matches(relative(root, path)) }
          .filter { path -> path.fileName.toString().endsWith(".$extension") }
          .toList()
      }
    if (matches.isEmpty()) throw ProjectConfigurationException("source include pattern matched no files: $include")
    return matches
  }

  private fun expandSourceDirectory(
    root: Path,
    include: String,
    extension: String,
  ): List<Path> {
    val normalized = include.replace('\\', '/')
    val includePath = Path.of(normalized)
    if (includePath.isAbsolute || normalized.split('/').any { component -> component == ".." }) {
      throw ProjectConfigurationException("source root escapes the project root: $include")
    }
    val directory = root.resolve(includePath).normalize()
    if (!directory.startsWith(root) || !Files.isDirectory(directory)) {
      throw ProjectConfigurationException("source root directory does not exist: $directory")
    }
    return Files.walk(directory).use { paths ->
      paths
        .filter(Path::isRegularFile)
        .filter { path -> path.fileName.toString().endsWith(".$extension") }
        .toList()
    }
  }

  private fun sourceExtension(variables: Map<String, List<String>>): String {
    val values = variables["XS_EXTENSION"] ?: listOf("xs")
    if (values.size != 1 || !values.single().matches(Regex("[A-Za-z0-9]+"))) {
      throw ProjectConfigurationException("XS_EXTENSION must be one extension without a leading dot")
    }
    return values.single()
  }

  private fun ProjectPlan.withInferredPackageTypes(sources: List<Path>): ProjectPlan {
    if ("XSPKG_TYPE" in variables) return this
    val extension = sourceExtension(variables)
    val inferred = inferPackageTypes(sources, extension, binaries, libraries)
    if (inferred.isEmpty()) return this
    return copy(variables = variables + ("XSPKG_TYPE" to inferred))
  }

  private fun ProjectPlan.withFilesystemDefaults(root: Path): ProjectPlan {
    val effectiveModules =
      if (moduleIncludes.isEmpty() && moduleRootOverride() == null) defaultModuleRoots(root) else moduleIncludes
    val effectiveTests = if (testIncludes.isEmpty()) defaultTestRoots(root, sourceIncludes) else testIncludes
    val effectiveSourceFilters = sourceFilters ?: (effectiveTests + effectiveModules).distinct()
    val effectiveModuleFilters = moduleFilters ?: (sourceIncludes + effectiveTests).distinct()
    val effectiveTestFilters = testFilters ?: (sourceIncludes + effectiveModules).distinct()
    return copy(
      moduleIncludes = effectiveModules,
      testIncludes = effectiveTests,
      sourceFilters = effectiveSourceFilters,
      moduleFilters = effectiveModuleFilters,
      testFilters = effectiveTestFilters,
    )
  }

  private fun validateArtifactTargets(
    plan: ProjectPlan,
    root: Path,
    sources: List<Path>,
    extension: String,
  ) {
    val selected = sources.toSet()
    (plan.binaries + plan.libraries).forEach { target ->
      val path = root.resolve(target.path).normalize()
      if (!path.startsWith(root) || path !in selected) {
        throw ProjectConfigurationException("artifact target is not selected by source.include(...): ${target.path}")
      }
      if (!path.fileName.toString().endsWith(".$extension")) {
        throw ProjectConfigurationException("artifact target must use the .$extension source extension: ${target.path}")
      }
    }
  }

  private fun validatePackageTypeInputs(
    plan: ProjectPlan,
    sources: List<Path>,
  ) {
    val extension = sourceExtension(plan.variables)
    val types = plan.variables["XSPKG_TYPE"].orEmpty()
    val hasBinarySource =
      plan.binaries.isNotEmpty() || sources.any { path -> path.fileName.toString() == "main.$extension" }
    val hasLibrarySource =
      plan.libraries.isNotEmpty() || sources.any { path -> path.fileName.toString() == "lib.$extension" }
    if ("bin" in types && !hasBinarySource) {
      throw ProjectConfigurationException("bin output requires main.$extension or set(\"BINARY\", ...)")
    }
    if (types.any { type -> type in setOf("xlib", "dylib", "staticlib", "cdylib") } && !hasLibrarySource) {
      throw ProjectConfigurationException("library output requires lib.$extension or set(\"LIBRARY\", ...)")
    }
  }

  private fun writeSources(
    project: ResolvedProject,
    compiler: CompilerSettings,
  ) {
    val configuredOutput = System.getProperty("xs.project.sources")?.takeIf(String::isNotBlank)
    val output = configuredOutput?.let { path -> Files.newOutputStream(Path.of(path)) } ?: System.out
    output.useIfOwned(configuredOutput != null) { stream ->
      writeRecord(stream, "xs-project-sources-v5")
      writeRecord(stream, compiler.warningLevel.name.lowercase())
      writeRecord(stream, compiler.warningsAsErrors.toString())
      writeRecord(stream, compiler.verbose.toString())
      writeRecord(stream, project.sources.size.toString())
      writeRecord(stream, project.modules.size.toString())
      writeRecord(stream, project.tests.size.toString())
      project.sources.forEach { path ->
        writeRecord(stream, path.toString())
      }
      project.modules.forEach { (name, path) ->
        writeRecord(stream, name)
        writeRecord(stream, path.toString())
      }
      project.tests.forEach { path ->
        writeRecord(stream, path.toString())
      }
      stream.flush()
    }
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
  ) = root.relativize(path).toString().replace('\\', '/')

  private fun globRegex(pattern: String): Regex {
    val result = StringBuilder("^")
    var index = 0
    while (index < pattern.length) {
      val character = pattern[index]
      when {
        character == '*' && index + 2 < pattern.length && pattern[index + 1] == '*' && pattern[index + 2] == '/' -> {
          result.append("(?:.*/)?")
          index += 3
        }

        character == '*' && index + 1 < pattern.length && pattern[index + 1] == '*' -> {
          result.append(".*")
          index += 2
        }

        character == '*' -> {
          result.append("[^/]*")
          index++
        }

        character == '?' -> {
          result.append("[^/]")
          index++
        }

        character in ".+()^$|{}[]\\" -> {
          result.append('\\').append(character)
          index++
        }

        else -> {
          result.append(character)
          index++
        }
      }
    }
    return Regex(result.append('$').toString())
  }

  private inline fun java.io.OutputStream.useIfOwned(
    owned: Boolean,
    block: (java.io.OutputStream) -> Unit,
  ) {
    if (owned) use(block) else block(this)
  }
}

private fun projectRoot(): Path =
  Path.of(System.getProperty("xs.project.root")).toAbsolutePath().normalize()

private fun defaultModuleRoots(root: Path): List<String> =
  if (Files.isDirectory(root.resolve("Modules"))) listOf("Modules") else emptyList()

internal fun defaultTestRoots(
  root: Path,
  sourceRoots: List<String>,
): List<String> =
  sourceRoots.mapNotNull { sourceRoot ->
    val candidate = root.resolve(sourceRoot).resolve("Test").normalize()
    if (!candidate.startsWith(root) || !Files.isDirectory(candidate)) {
      null
    } else {
      root.relativize(candidate).toString().replace('\\', '/')
    }
  }

private fun moduleRootOverride(): String? =
  System.getProperty("xs.project.moduleRoot")?.takeIf(String::isNotBlank)

internal fun inferPackageTypes(
  sources: List<Path>,
  extension: String,
  binaries: List<ArtifactTarget> = emptyList(),
  libraries: List<ArtifactTarget> = emptyList(),
): List<String> =
  buildList {
    if (libraries.isNotEmpty() || sources.any { path -> path.fileName.toString() == "lib.$extension" }) add("xlib")
    if (binaries.isNotEmpty() || sources.any { path -> path.fileName.toString() == "main.$extension" }) add("bin")
  }
