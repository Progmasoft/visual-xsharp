/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.io.File
import java.nio.file.Files
import java.nio.file.Path
import kotlin.io.path.writeText
import kotlin.system.exitProcess

private const val MINIMUM_JAVA = 25

private data class ProjectFiles(
  val root: File,
  val project: File?,
  val settings: File?,
  val build: File?,
  val modules: File?,
)

private fun usage() =
  "internal xs project runtime usage: evaluate [project-root]\n" +
    "       resolve [project-root]\n" +
    "       sources0 <project-root> <output-file> [module-root]\n" +
    "       modules0 <project-root> <output-file> <module-root>"

internal fun isSupportedJavaFeature(actual: Int) = actual >= MINIMUM_JAVA

internal fun requireSupportedJava() {
  val actual = Runtime.version().feature()
  if (!isSupportedJavaFeature(actual)) {
    throw ProjectConfigurationException("JRE 25 or newer is required; found Java $actual")
  }
}

private fun filesAt(root: File): ProjectFiles? {
  val project = root.resolve("xs.project.kts").takeIf(File::isFile)
  val settings = root.resolve("xs.settings.kts").takeIf(File::isFile)
  val build = root.resolve("xs.build.kts").takeIf(File::isFile)
  val modules = root.resolve("xs.module.kts").takeIf(File::isFile)
  if (project == null && settings == null && build == null) return null
  if (project != null && (settings != null || build != null)) {
    throw ProjectConfigurationException("xs.project.kts cannot be combined with split project files")
  }
  if (project != null) return ProjectFiles(root, project, null, null, modules)
  if (settings == null || build == null) {
    throw ProjectConfigurationException("split projects require both xs.settings.kts and xs.build.kts")
  }
  return ProjectFiles(root, null, settings, build, modules)
}

private fun discover(input: File): ProjectFiles {
  if (input.isFile) {
    return filesAt(input.absoluteFile.parentFile.canonicalFile)
      ?: throw ProjectConfigurationException("no X# Kotlin project file found beside $input")
  }
  var root: File? = input.canonicalFile
  while (root != null) {
    filesAt(root)?.let { files -> return files }
    root = root.parentFile
  }
  throw ProjectConfigurationException("no X# Kotlin project file found from ${input.canonicalFile}")
}

private fun existingClasspath(): String =
  System
    .getProperty("java.class.path")
    .split(File.pathSeparator)
    .filter { entry -> File(entry).exists() }
    .joinToString(File.pathSeparator)

private fun runKotlin(
  script: File,
  root: File,
  output: String,
  state: Path?,
  sourcesOutput: Path?,
): Int {
  val directory = Files.createTempDirectory("xs-project-kts-")
  return try {
    val wrapped = directory.resolve(script.name)
    val suffix =
      if (output == "state") {
        "\nProjectStateFile.writeConfigured(ProjectRuntime.snapshot())\n"
      } else if (output == "modules0") {
        "\nProjectOutput.emitModules(ProjectRuntime.snapshot())\n"
      } else {
        "\nProjectOutput.emit(ProjectRuntime.build())\n"
      }
    wrapped.writeText("import org.xsslang.project.*\n" + script.readText() + suffix)
    val kotlin = System.getenv("XS_KOTLIN")?.takeIf(String::isNotBlank) ?: "kotlin"
    val arguments =
      mutableListOf(
        kotlin,
        "-J--enable-native-access=ALL-UNNAMED",
        "-Dxs.project.root=${root.absolutePath}",
        "-Dxs.project.output=$output",
      )
    if (state != null) arguments += "-Dxs.project.state=$state"
    if (sourcesOutput != null) arguments += "-Dxs.project.sources=$sourcesOutput"
    System.getProperty("xs.project.moduleRoot")?.takeIf(String::isNotBlank)?.let { rootPath ->
      arguments += "-Dxs.project.moduleRoot=$rootPath"
    }
    arguments +=
      listOf(
        "-classpath",
        existingClasspath(),
        "-howtorun",
        "script",
        wrapped.toString(),
      )
    try {
      ProcessBuilder(arguments)
        .apply { environment()["JAVA_HOME"] = System.getProperty("java.home") }
        .inheritIO()
        .start()
        .waitFor()
    } catch (error: java.io.IOException) {
      throw ProjectConfigurationException("kotlin is required and could not be started: ${error.message}")
    }
  } finally {
    directory.toFile().deleteRecursively()
  }
}

internal fun evaluateWithKotlin(
  input: File,
  output: String = "plan",
  sourcesOutput: Path? = null,
  moduleRoot: String? = null,
): Int {
  requireSupportedJava()
  if (moduleRoot != null) System.setProperty("xs.project.moduleRoot", moduleRoot)
  val files = discover(input)
  if (files.project != null && files.modules == null) {
    return runKotlin(files.project, files.root, output, null, sourcesOutput)
  }
  val stateDirectory = Files.createTempDirectory("xs-project-state-")
  return try {
    val state = stateDirectory.resolve("state.bin")
    val first = files.project ?: files.settings!!
    val firstStatus = runKotlin(first, files.root, "state", state, null)
    if (firstStatus != 0) return firstStatus
    if (files.project == null) {
      val buildOutput = if (files.modules == null) output else "state"
      val buildStatus = runKotlin(files.build!!, files.root, buildOutput, state, sourcesOutput)
      if (buildStatus != 0 || files.modules == null) return buildStatus
    }
    runKotlin(files.modules!!, files.root, output, state, sourcesOutput)
  } finally {
    stateDirectory.toFile().deleteRecursively()
  }
}

internal fun evaluateModulesWithKotlin(
  input: File,
  sourcesOutput: Path,
  moduleRoot: String,
): Int {
  requireSupportedJava()
  val root = input.canonicalFile
  val script = root.resolve("xs.module.kts")
  if (!script.isFile) throw ProjectConfigurationException("xs.module.kts was not found in $root")
  System.setProperty("xs.project.moduleRoot", moduleRoot)
  return runKotlin(script, root, "modules0", null, sourcesOutput)
}

fun main(args: Array<String>) {
  val validEvaluate = args.size in 1..2 && args[0] == "evaluate"
  val validResolve = args.size in 1..2 && args[0] == "resolve"
  val validSources = args.size in 3..4 && args[0] == "sources0"
  val validModules = args.size == 4 && args[0] == "modules0"
  if (!validEvaluate && !validResolve && !validSources && !validModules) {
    System.err.println(usage())
    exitProcess(2)
  }
  val input = File(args.getOrElse(1) { "." })
  try {
    val status =
      if (args[0] == "evaluate") {
        evaluateWithKotlin(input)
      } else if (args[0] == "resolve") {
        evaluateWithKotlin(input, "resolve")
      } else if (args[0] == "sources0") {
        evaluateWithKotlin(input, "sources0", Path.of(args[2]), args.getOrNull(3))
      } else {
        evaluateModulesWithKotlin(input, Path.of(args[2]), args[3])
      }
    exitProcess(status)
  } catch (error: ProjectConfigurationException) {
    System.err.println("xs: project runtime: ${error.message}")
    exitProcess(1)
  }
}
