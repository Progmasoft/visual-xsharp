/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.nio.charset.StandardCharsets
import java.nio.file.Files
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class ProjectDslTest {
  @Test
  fun recordsStructuredModuleCoordinates() {
    val root = Files.createTempDirectory("xs-project-lock-test-")
    val context =
      ProjectContext().apply {
        project("Locked", "BETA", "0.1.0")
        source { include("Sources") }
        dependencies {
          addModule("XML", "stable", "2.0.0")
          addModule("JSON", "beta", "1.4.0")
          addModule("JSON", "beta", "1.4.0")
        }
      }
    try {
      val plan = context.build()
      val expected =
        listOf(
          ModuleDependency("JSON", "beta", "1.4.0"),
          ModuleDependency("XML", "stable", "2.0.0"),
        )
      assertEquals(expected, plan.modules)
      assertTrue(
        PlanWriter.write(plan).contains(
          "\"modules\":[{\"name\":\"JSON\",\"stability\":\"beta\",\"version\":\"1.4.0\"}",
        ),
      )
      ModuleLockFile.write(root, plan.modules)
      val lock = root.resolve(ModuleLockFile.FILE_NAME)
      assertTrue(Files.isRegularFile(lock))
      val header = Files.readAllBytes(lock).copyOfRange(0, 16).toString(StandardCharsets.UTF_8)
      assertEquals("SQLite format 3\u0000", header)
      assertEquals(expected, ModuleLockFile.read(lock))
      val firstWrite = Files.readAllBytes(lock)
      ModuleLockFile.write(root, plan.modules)
      assertTrue(firstWrite.contentEquals(Files.readAllBytes(lock)))
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun rejectsConflictingModuleCoordinates() {
    val context = ProjectContext()
    val error =
      assertFailsWith<ProjectConfigurationException> {
        context.dependencies {
          addModule("JSON", "stable", "1.0.0")
          addModule("JSON", "stable", "2.0.0")
        }
      }
    assertTrue(error.message.orEmpty().contains("conflicting"))
  }

  @Test
  fun compilerPolicyDefaultsAreStable() {
    val settings = CompilerSettings()
    assertEquals(WarningLevel.MEDIUM, settings.warningLevel)
    assertTrue(!settings.warningsAsErrors)
    assertTrue(settings.verbose)
  }

  @Test
  fun buildDefaultsAndPackageTypesAreValidated() {
    val context = ProjectContext()
    assertEquals("Release", context.get("BUILD_MODE"))
    assertEquals("build/release", context.get("RELEASE_OUTDIR"))
    assertEquals("build/debug", context.get("DEBUG_OUTDIR"))

    context.set("BUILD_MODE", "Debug")
    context.set("RELEASE_OUTDIR", "out/release")
    context.set("DEBUG_OUTDIR", "out/debug")
    context.set("XSPKG_TYPE", listOf("xlib", "bin"))
    assertEquals(listOf("xlib", "bin"), context.getAll("XSPKG_TYPE"))

    context.set(
      "BINARY",
      mapOf("name" to "Name", "path" to "Sources/program.xs"),
      mapOf("name" to "Example", "path" to "Sources/example.xs"),
    )
    context.set("LIBRARY", mapOf("name" to "Library", "path" to "Sources/library.xs"))
    assertEquals(listOf("Name:Sources/program.xs", "Example:Sources/example.xs"), context.getAll("BINARY"))
    assertEquals(listOf("Library:Sources/library.xs"), context.getAll("LIBRARY"))

    val artifactPlan =
      ProjectContext().apply {
        project("Artifacts", "BETA", "0.1.0")
        source { include("Sources") }
        set("BINARY", mapOf("name" to "tool", "path" to "Sources/tool.xs"))
      }.build()
    assertEquals(listOf(ArtifactTarget("tool", "Sources/tool.xs")), artifactPlan.binaries)
    assertTrue(PlanWriter.write(artifactPlan).contains("\"binaries\":[{\"name\":\"tool\""))

    assertFailsWith<ProjectConfigurationException> { context.set("BUILD_MODE", "RELEASE") }
    assertFailsWith<ProjectConfigurationException> { context.set("XSPKG_TYPE", listOf("bin", "bin")) }
    assertFailsWith<ProjectConfigurationException> { context.set("XSPKG_TYPE", listOf("unknown")) }
    assertFailsWith<ProjectConfigurationException> {
      context.set("BINARY", mapOf("name" to "MissingPath"))
    }
    assertFailsWith<ProjectConfigurationException> {
      context.set(
        "LIBRARY",
        mapOf("name" to "Duplicate", "path" to "Sources/a.xs"),
        mapOf("name" to "Duplicate", "path" to "Sources/b.xs"),
      )
    }
  }

  @Test
  fun infersPackageTypesWithoutRequiringMain() {
    val root = Files.createTempDirectory("xs-project-package-type-test-")
    val sources = Files.createDirectories(root.resolve("Sources"))
    val output = root.resolve("sources.bin")
    val oldRoot = System.getProperty("xs.project.root")
    val oldOutput = System.getProperty("xs.project.output")
    val oldSources = System.getProperty("xs.project.sources")
    try {
      val lib = sources.resolve("lib.xs")
      val main = sources.resolve("main.xs")
      Files.writeString(lib, "public fn answer() -> Long { 42 }")
      Files.writeString(main, "fn main() -> Long { 0 }")
      assertEquals(listOf("xlib"), inferPackageTypes(listOf(lib), "xs"))
      assertEquals(listOf("bin"), inferPackageTypes(listOf(main), "xs"))
      assertEquals(listOf("xlib", "bin"), inferPackageTypes(listOf(main, lib), "xs"))
      assertEquals(
        listOf("bin"),
        inferPackageTypes(
          emptyList(),
          "xs",
          binaries = listOf(ArtifactTarget("tool", "Sources/tool.xs")),
        ),
      )
      assertEquals(emptyList(), inferPackageTypes(listOf(root.resolve("helper.xs")), "xs"))

      Files.delete(main)
      Files.writeString(sources.resolve("tool.xs"), "fn main() -> Long { 0 }")
      val context =
        ProjectContext().apply {
          project("Library", "BETA", "0.1.0")
          set("BINARY", mapOf("name" to "tool", "path" to "Sources/tool.xs"))
        }
      System.setProperty("xs.project.root", root.toString())
      System.setProperty("xs.project.output", "sources0")
      System.setProperty("xs.project.sources", output.toString())
      ProjectOutput.emit(context.build())
      assertTrue(Files.size(output) > 0)
    } finally {
      restoreProperty("xs.project.root", oldRoot)
      restoreProperty("xs.project.output", oldOutput)
      restoreProperty("xs.project.sources", oldSources)
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun infersExistingTestChildrenFromSourceRoots() {
    val root = Files.createTempDirectory("xs-project-default-tests-")
    try {
      Files.createDirectories(root.resolve("Sources/Test"))
      Files.createDirectories(root.resolve("Platform"))
      assertEquals(listOf("Sources/Test"), defaultTestRoots(root, listOf("Sources", "Platform")))
      assertEquals(emptyList(), defaultTestRoots(root, listOf("Missing")))
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun excludeMetadataDoesNotFilterCompilerSources() {
    val root = Files.createTempDirectory("xs-project-default-exclude-")
    val sources = Files.createDirectories(root.resolve("Sources"))
    val nested = Files.createDirectories(sources.resolve("Nested"))
    val tests = Files.createDirectories(sources.resolve("Test"))
    val output = root.resolve("sources.bin")
    val oldRoot = System.getProperty("xs.project.root")
    val oldOutput = System.getProperty("xs.project.output")
    val oldSources = System.getProperty("xs.project.sources")
    try {
      Files.writeString(sources.resolve("main.xs"), "fn main() -> Long { 0 }")
      Files.writeString(nested.resolve("helper.xs"), "fn helper() -> Long { 1 }")
      Files.writeString(tests.resolve("smoke.xs"), "fn smoke() {}")
      System.setProperty("xs.project.root", root.toString())
      System.setProperty("xs.project.output", "sources0")
      System.setProperty("xs.project.sources", output.toString())

      val direct = ProjectContext().apply { project("Direct", "BETA", "0.1.0") }
      ProjectOutput.emit(direct.build())
      assertEquals("2", readSourceRecords(output)[4])

      val recursive =
        ProjectContext().apply {
          project("Recursive", "BETA", "0.1.0")
          source { exclude() }
        }
      ProjectOutput.emit(recursive.build())
      assertEquals("2", readSourceRecords(output)[4])
    } finally {
      restoreProperty("xs.project.root", oldRoot)
      restoreProperty("xs.project.output", oldOutput)
      restoreProperty("xs.project.sources", oldSources)
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun splitFilesShareStateWithoutSectionOwnership() {
    val settings =
      ProjectContext().apply {
        source { include("sources") }
        compiler { warnings("low") }
      }
    val build =
      ProjectContext(state = settings.snapshot()).apply {
        project("Split", "BETA", "0.1.0")
        authors(arrayOf("Leitwolf", "leitwolf@example.me"))
      }
    val plan = build.build()
    assertEquals("Split", plan.identity.name)
    assertEquals(listOf("sources"), plan.sourceIncludes)
    assertEquals(WarningLevel.LOW, plan.compiler.warningLevel)
  }

  @Test
  fun buildsConditionalProjectPlan() {
    val context = ProjectContext(Host(OperatingSystem.LINUX, OperatingSystemFamily.UNIX, Architecture.X86_64))
    context.project("Demo", "BETA", "0.1.0")
    if (context.host.os == OperatingSystem.LINUX) context.set("MODE", "native")
    context.set(
      "TARGET",
      "x86_64-unknown-linux-gnu",
      "x86_64-unknown-linux-musl",
      "aarch64-unknown-linux-gnu",
      "armv7h-unknown-linux-gnueabihf",
      "riscv64gc-unknown-linux-gnu",
      "x86_64-apple-darwin",
      "aarch64-apple-darwin",
      "x86_64-pc-windows-msvc",
      "aarch64-pc-windows-msvc",
      "x86_64-unknown-freebsd",
      "aarch64-unknown-freebsd",
    )
    assertEquals("native", context.get("MODE"))
    assertEquals(
      listOf(
        "x86_64-unknown-linux-gnu",
        "x86_64-unknown-linux-musl",
        "aarch64-unknown-linux-gnu",
        "armv7h-unknown-linux-gnueabihf",
        "riscv64gc-unknown-linux-gnu",
        "x86_64-apple-darwin",
        "aarch64-apple-darwin",
        "x86_64-pc-windows-msvc",
        "aarch64-pc-windows-msvc",
        "x86_64-unknown-freebsd",
        "aarch64-unknown-freebsd",
      ),
      context.getAll("TARGET"),
    )
    assertEquals("Demo, BETA, 0.1.0", context.get("PROJECT"))
    assertEquals("LLVM", context.get("XS_BACKEND"))
    assertEquals("true", context.get("XS_LLVM_LTO"))
    assertEquals("3", context.get("XS_LLVM_OPT_LEVEL"))
    assertEquals("xs", context.get("XS_EXTENSION"))
    assertEquals("false", context.get("PUBLISH"))
    context.set("PUBLISH", true)
    assertEquals("true", context.get("PUBLISH"))
    assertFailsWith<ProjectConfigurationException> { context.set("PUBLISH", "true") }
    assertFailsWith<ProjectConfigurationException> { context.set("PUBLISH", true, false) }
    context.set("XS_LLVM_LTO", false)
    context.set("XS_LLVM_OPT_LEVEL", "2")
    assertEquals("false", context.get("XS_LLVM_LTO"))
    assertEquals("2", context.get("XS_LLVM_OPT_LEVEL"))
    assertFailsWith<ProjectConfigurationException> { context.set("XS_LLVM_LTO", "yes") }
    assertFailsWith<ProjectConfigurationException> { context.set("XS_LLVM_OPT_LEVEL", "fast") }
    assertFailsWith<ProjectConfigurationException> { context.get("MISSING") }
    context.authors(arrayOf("Leitwolf", "leitwolf@example.me"))
    context.source {
      include("sources")
      exclude("sources/tests/**")
    }
    context.compiler {
      warnings("all")
      werror(true)
      verbose(true)
    }
    val plan = context.build()
    assertEquals(listOf("native"), plan.variables["MODE"])
    assertEquals(listOf("LLVM"), plan.variables["XS_BACKEND"])
    assertEquals(listOf("false"), plan.variables["XS_LLVM_LTO"])
    assertEquals(listOf("2"), plan.variables["XS_LLVM_OPT_LEVEL"])
    assertEquals(WarningLevel.ALL, plan.compiler.warningLevel)
    assertTrue(PlanWriter.write(plan).startsWith("{\"format\":\"xs-project-plan\",\"version\":0"))
  }

  @Test
  fun removesLlvmOnlySettingsFromXplrPlans() {
    val context = ProjectContext()
    context.project("RegisterVm", "BETA", "0.1.0")
    context.set("XS_BACKEND", "XPLR")
    val plan = context.build()
    assertEquals(listOf("XPLR"), plan.variables["XS_BACKEND"])
    assertFalse(plan.variables.containsKey("XS_LLVM_LTO"))
    assertFalse(plan.variables.containsKey("XS_LLVM_OPT_LEVEL"))
  }

  @Test
  fun derivesMemoryManagementFromTheBackend() {
    val context = ProjectContext()
    assertFailsWith<ProjectConfigurationException> { context.set("XGC_ENABLED", true) }
    assertFailsWith<ProjectConfigurationException> { context.set("XPG_ENABLED", true) }
  }

  @Test
  fun requiresProjectAndDefaultsSourceInclude() {
    val context = ProjectContext()
    assertFailsWith<ProjectConfigurationException> { context.build() }
    context.project("Demo", "BETA", "0.1.0")
    val defaults = context.build()
    assertEquals(listOf("Sources"), defaults.sourceIncludes)
    assertEquals(listOf("*/**"), defaults.sourceExcludes)
    assertEquals(listOf("*/**"), defaults.moduleExcludes)
    assertEquals(listOf("*/**"), defaults.testExcludes)

    context.source {
      exclude()
      filter(listOf("Tests", "Modules"))
    }
    context.module {
      include("Modules")
      exclude()
      filter(listOf("Sources", "Tests"))
    }
    context.test {
      exclude()
      filter(listOf("Sources", "Modules"))
    }
    val recursive = context.build()
    assertEquals(emptyList(), recursive.sourceExcludes)
    assertEquals(emptyList(), recursive.moduleExcludes)
    assertEquals(emptyList(), recursive.testExcludes)
    assertEquals(listOf("Tests", "Modules"), recursive.sourceFilters)
    assertEquals(listOf("Sources", "Tests"), recursive.moduleFilters)
    assertEquals(listOf("Sources", "Modules"), recursive.testFilters)
  }

  @Test
  fun sourceAndModuleRootsRejectGlobs() {
    val sourceGlob =
      ProjectContext().apply {
        project("Demo", "BETA", "0.1.0")
      }
    assertFailsWith<ProjectConfigurationException> { sourceGlob.source { include("sources/**/*.xs") } }
    assertFailsWith<ProjectConfigurationException> { sourceGlob.module { include("Modules/*") } }
    assertFailsWith<ProjectConfigurationException> { sourceGlob.test { include("Tests/*.xs") } }
    sourceGlob.source { include("Sources") }
    sourceGlob.test {
      include("Tests")
      exclude("Tests/fixtures/**")
    }
    assertEquals(listOf("Tests/fixtures/**"), sourceGlob.build().testExcludes)
  }

  @Test
  fun customSourceExtensionReplacesXsDiscovery() {
    val root = Files.createTempDirectory("xs-project-extension-test-")
    val sources = Files.createDirectories(root.resolve("Sources"))
    val modules = Files.createDirectories(root.resolve("Modules"))
    val tests = Files.createDirectories(root.resolve("Tests"))
    Files.writeString(sources.resolve("main.xsharp"), "fn main() -> Long { return 0; }")
    Files.writeString(sources.resolve("ignored.xs"), "fn ignored() -> Long { return 1; }")
    Files.writeString(modules.resolve("math.xsharp"), "public fn add() -> Long { return 1; }")
    Files.writeString(modules.resolve("ignored.xs"), "public fn ignored() -> Long { return 2; }")
    Files.writeString(tests.resolve("smoke.xsharp"), "fn smoke() {}")
    val output = root.resolve("sources.bin")
    val context =
      ProjectContext().apply {
        project("Extension", "BETA", "0.1.0")
        set("XS_EXTENSION", "xsharp")
        source { include("Sources") }
        module {
          name("Math")
          add("Modules/*.xsharp")
        }
        test { include("Tests") }
      }
    val oldRoot = System.getProperty("xs.project.root")
    val oldOutput = System.getProperty("xs.project.output")
    val oldSources = System.getProperty("xs.project.sources")
    try {
      System.setProperty("xs.project.root", root.toString())
      System.setProperty("xs.project.output", "sources0")
      System.setProperty("xs.project.sources", output.toString())
      ProjectOutput.emit(context.build())
      val records =
        Files
          .readAllBytes(output)
          .toString(StandardCharsets.UTF_8)
          .split('\u0000')
          .filter(String::isNotEmpty)
      assertEquals(
        listOf("xs-project-sources-v5", "medium", "false", "true", "1", "1", "1"),
        records.take(7),
      )
      assertEquals(
        listOf(
          sources.resolve("main.xsharp").toString(),
          "Math",
          modules.resolve("math.xsharp").toString(),
          tests.resolve("smoke.xsharp").toString(),
        ),
        records.drop(7),
      )
    } finally {
      restoreProperty("xs.project.root", oldRoot)
      restoreProperty("xs.project.output", oldOutput)
      restoreProperty("xs.project.sources", oldSources)
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun acceptsJava25AndNewer() {
    assertTrue(!isSupportedJavaFeature(24))
    assertTrue(isSupportedJavaFeature(25))
    assertTrue(isSupportedJavaFeature(26))
    requireSupportedJava()
    assertTrue(Runtime.version().feature() >= 25)
  }

  @Test
  fun bsdBelongsToBothBsdAndUnixFamilies() {
    val host =
      Host(
        OperatingSystem.FREEBSD,
        OperatingSystemFamily.forOperatingSystem(OperatingSystem.FREEBSD),
        Architecture.X86_64,
      )
    assertTrue(host.family == OperatingSystemFamily.BSD)
    assertTrue(host.family == OperatingSystemFamily.UNIX)
  }

  @Test
  fun linuxAndMacosAreUnixButNotBsd() {
    val linux = OperatingSystemFamily.forOperatingSystem(OperatingSystem.LINUX)
    val macos = OperatingSystemFamily.forOperatingSystem(OperatingSystem.MACOS)
    assertTrue(linux == OperatingSystemFamily.UNIX)
    assertTrue(macos == OperatingSystemFamily.UNIX)
    assertTrue(linux != OperatingSystemFamily.BSD)
    assertTrue(macos != OperatingSystemFamily.BSD)
  }

  @Test
  fun reactOsIsNotWindowsButBelongsToTheWindowsFamily() {
    val os = operatingSystemFromName("ReactOS")
    val family = OperatingSystemFamily.forOperatingSystem(os)
    assertTrue(os != OperatingSystem.WINDOWS)
    assertTrue(family == OperatingSystemFamily.WINDOWS)
    assertTrue(family == OperatingSystem.WINDOWS)
    assertTrue(family != OperatingSystemFamily.UNIX)
    assertTrue(family != OperatingSystemFamily.BSD)
  }

  @Test
  fun expandsSourceDirectoriesRecursivelyWithoutApplyingPackageExcludes() {
    val root = Files.createTempDirectory("xs-project-glob-test-")
    val sources = Files.createDirectories(root.resolve("sources"))
    val tests = Files.createDirectories(sources.resolve("tests"))
    Files.writeString(sources.resolve("main.xs"), "fn main() -> Long { return helper(); }")
    Files.writeString(sources.resolve("helper.xs"), "fn helper() -> Long { return 0; }")
    Files.writeString(tests.resolve("ignored.xs"), "fn ignored() -> Long { return 1; }")
    val output = root.resolve("sources.bin")
    val context =
      ProjectContext().apply {
        project("Glob", "BETA", "0.1.0")
        source {
          include("sources")
          exclude("sources/tests/**")
        }
        compiler {
          warnings("all")
          werror(true)
          verbose(true)
        }
      }
    val oldRoot = System.getProperty("xs.project.root")
    val oldOutput = System.getProperty("xs.project.output")
    val oldSources = System.getProperty("xs.project.sources")
    try {
      System.setProperty("xs.project.root", root.toString())
      System.setProperty("xs.project.output", "sources0")
      System.setProperty("xs.project.sources", output.toString())
      ProjectOutput.emit(context.build())
      val paths =
        Files
          .readAllBytes(output)
          .toString(StandardCharsets.UTF_8)
          .split('\u0000')
          .filter(String::isNotEmpty)
      assertEquals(
        listOf("xs-project-sources-v5", "all", "true", "true", "3", "0", "0"),
        paths.take(7),
      )
      assertEquals(
        listOf(
          sources.resolve("main.xs").toString(),
          sources.resolve("helper.xs").toString(),
          tests.resolve("ignored.xs").toString(),
        ),
        paths.drop(7),
      )
    } finally {
      restoreProperty("xs.project.root", oldRoot)
      restoreProperty("xs.project.output", oldOutput)
      restoreProperty("xs.project.sources", oldSources)
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun resolvesExplicitModuleMemberships() {
    val root = Files.createTempDirectory("xs-project-module-test-")
    val sources = Files.createDirectories(root.resolve("Sources"))
    val modules = Files.createDirectories(root.resolve("Modules/Example/Utils"))
    Files.writeString(sources.resolve("main.xs"), "import MyModule;\nfn main() -> Long { return 0; }")
    Files.writeString(modules.resolve("math.xs"), "public fn math(a: Int, b: Int) { a + b }")
    Files.writeString(modules.resolve("topla.xs"), "public fn topla(a: Int, b: Int) { a + b }")
    val output = root.resolve("sources.bin")
    val context =
      ProjectContext().apply {
        project("Modules", "BETA", "0.1.0")
        source { include("Sources") }
        module { include("Modules") }
        module {
          name("MyModule")
          add("Modules/Example/Utils/math*.xs")
          submodule {
            name("util")
            add("Modules/Example/Utils/topla.xs")
          }
        }
      }
    val oldRoot = System.getProperty("xs.project.root")
    val oldOutput = System.getProperty("xs.project.output")
    val oldSources = System.getProperty("xs.project.sources")
    try {
      System.setProperty("xs.project.root", root.toString())
      System.setProperty("xs.project.output", "sources0")
      System.setProperty("xs.project.sources", output.toString())
      ProjectOutput.emit(context.build())
      val records =
        Files
          .readAllBytes(output)
          .toString(StandardCharsets.UTF_8)
          .split('\u0000')
          .filter(String::isNotEmpty)
      assertEquals(
        listOf("xs-project-sources-v5", "medium", "false", "true", "1", "2", "0"),
        records.take(7),
      )
      assertEquals(sources.resolve("main.xs").toString(), records[7])
      assertEquals(
        listOf(
          "MyModule",
          modules.resolve("math.xs").toString(),
          "MyModule::util",
          modules.resolve("topla.xs").toString(),
        ),
        records.drop(8),
      )
    } finally {
      restoreProperty("xs.project.root", oldRoot)
      restoreProperty("xs.project.output", oldOutput)
      restoreProperty("xs.project.sources", oldSources)
      root.toFile().deleteRecursively()
    }
  }

  private fun restoreProperty(
    name: String,
    value: String?,
  ) {
    if (value == null) System.clearProperty(name) else System.setProperty(name, value)
  }

  private fun readSourceRecords(path: java.nio.file.Path): List<String> =
    Files
      .readAllBytes(path)
      .toString(StandardCharsets.UTF_8)
      .split('\u0000')
      .filter(String::isNotEmpty)
}
