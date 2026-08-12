/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import java.nio.file.Files
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertNull
import kotlin.test.assertTrue

class ProjectDslTest {
  @BeforeTest
  fun resetRuntime() {
    ProjectRuntime.reset()
  }

  @AfterTest
  fun clearProperties() {
    System.clearProperty("xs.project.root")
    System.clearProperty("xs.project.output")
    System.clearProperty("xs.project.sources")
  }

  @Test
  fun requiresOnlyQualifiedMainEntry() {
    sources {
      main { entry = "Demo.Main" }
    }
    val plan = ProjectRuntime.build()

    assertNull(plan.identity)
    assertEquals(listOf("Sources"), plan.sourceIncludes)
    assertEquals("Demo.Main", plan.entry)
    assertEquals(BuildMode.DEBUG, plan.compiler.buildMode)
    assertEquals(LlvmOptLevel.O0, effectiveOptLevel(plan.compiler))
    assertTrue(plan.compiler.xppOptimizationPasses)
    assertTrue(plan.compiler.xmmOptimizationPasses)
  }

  @Test
  fun rejectsMissingOrUnqualifiedEntry() {
    assertFailsWith<ProjectConfigurationException> { ProjectRuntime.build() }

    sources { main { entry = "Main" } }
    assertFailsWith<ProjectConfigurationException> { ProjectRuntime.build() }
  }

  @Test
  fun configuresRenewedProjectAndCompilerSurface() {
    project {
      name = "Compiler"
      version = "0.3.0"
      stability = Stability.NIGHTLY
    }
    compiler {
      version = "0.3.0"
      standard = "26"
      backend = Backend.LLVM
      buildMode = BuildMode.RELEASE
      emit = Emit.XMM
      warningsAsErrors = true
      warnings = Warnings.ALL
      experimentalWarnings = true
      shadowWarnings = true
      undefinedWarnings = false
      unsafe {
        xppOptimizationPasses = false
        xmmOptimizationPasses = false
        typeSafeFormat = false
      }
      llvm {
        optLevel = LlvmOptLevel.OG
        compiler = LlvmCompiler.ORC
        lto = LlvmLto.THIN
      }
    }
    outdirs {
      release = "out/release"
      debug = "out/debug"
    }
    targets { target("x86_64-pc-windows-msvc", "aarch64-apple-darwin") }
    authors { author("Leitwolf", "leitwolf@example.me") }
    pml { enabled = false }
    workspaces { workspace("core") { path = "./Core" } }
    sources {
      viget {
        publish = true
        exclude("build/**")
      }
      main {
        srcDir = "Source"
        exclude("Generated/**")
        entry = "Compiler.Main"
      }
      test {
        testDir = "Tests"
        exclude("Fixtures/**")
        framework = "tests"
      }
    }

    val plan = ProjectRuntime.build()
    assertEquals(ProjectIdentity("Compiler", "NIGHTLY", "0.3.0"), plan.identity)
    assertEquals(Emit.XMM, plan.compiler.emit)
    assertEquals(LlvmCompiler.ORC, plan.compiler.llvmCompiler)
    assertEquals(LlvmLto.THIN, plan.compiler.llvmLto)
    assertEquals(listOf("x86_64-pc-windows-msvc", "aarch64-apple-darwin"), plan.targets)
    assertEquals(listOf(Workspace("core", "./Core")), plan.workspaces)
    assertFalse(plan.pmlEnabled)
    assertTrue(plan.publishSources)
    assertEquals(listOf("build/**"), plan.publishExcludes)
    assertEquals(listOf("Generated/**"), plan.sourceExcludes)
    assertEquals(listOf("Fixtures/**"), plan.testExcludes)
  }

  @Test
  fun derivesLlvmOptimizationFromBuildMode() {
    sources { main { entry = "Debug.Main" } }
    assertEquals(LlvmOptLevel.O0, effectiveOptLevel(ProjectRuntime.build().compiler))

    ProjectRuntime.reset()
    compiler { buildMode = BuildMode.RELEASE }
    sources { main { entry = "Release.Main" } }
    assertEquals(LlvmOptLevel.O3, effectiveOptLevel(ProjectRuntime.build().compiler))
  }

  @Test
  fun planWriterPublishesOnlyRenewedCatalog() {
    project {
      name = "Plan"
      version = "1.0.0"
      stability = Stability.STABLE
    }
    sources { main { entry = "Plan.Main" } }
    val text = PlanWriter.write(ProjectRuntime.build())

    assertTrue(text.startsWith("{\"format\":\"visual-xsharp-project-plan\",\"version\":1"))
    assertTrue(text.contains("\"entry\":\"Plan.Main\""))
    assertTrue(text.contains("\"xmmOptimizationPasses\":true"))
    assertFalse(text.contains("module"))
    assertFalse(text.contains("\"variables\""))
    assertFalse(text.contains("XLIL"))
  }

  @Test
  fun emitsRenewedSourceRegistryFromMainAndTestRoots() {
    val root = Files.createTempDirectory("visual-xsharp-project-")
    val output = Files.createTempFile("visual-xsharp-sources-", ".bin")
    try {
      Files.createDirectories(root.resolve("Sources"))
      Files.createDirectories(root.resolve("Tests"))
      Files.writeString(
        root.resolve("Sources/main.vxs"),
        "namespace Demo; public class Main { public static void Main() {} }",
      )
      Files.writeString(root.resolve("Tests/smoke.vxs"), "namespace Demo; class Tests {}")
      sources {
        main { entry = "Demo.Main" }
        test { framework = "tests" }
      }
      System.setProperty("xs.project.root", root.toString())
      System.setProperty("xs.project.output", "sources0")
      System.setProperty("xs.project.sources", output.toString())

      ProjectOutput.emit(ProjectRuntime.build())
      val records = readRecords(output)
      assertEquals("visual-xsharp-sources-v1", records[0])
      assertEquals("Demo.Main", records[1])
      assertEquals("1", records[18])
      assertEquals("1", records[19])
      assertTrue(
        records.any { it.endsWith("Sources\\main.vxs") || it.endsWith("Sources/main.vxs") }
      )
      assertTrue(records.any { it.endsWith("Tests\\smoke.vxs") || it.endsWith("Tests/smoke.vxs") })
    } finally {
      root.toFile().deleteRecursively()
      Files.deleteIfExists(output)
    }
  }

  @Test
  fun keepsCfgHostPredicates() {
    assertTrue(cfg(true))
    assertFalse(cfg(false))
    assertTrue(OS.toString().isNotBlank())
    assertTrue(FAMILY.toString().isNotBlank())
    assertTrue(ARCH.toString().isNotBlank())
    assertEquals(
      OperatingSystemFamily.BSD,
      Host(OperatingSystem.FREEBSD, OperatingSystemFamily.BSD, Architecture.X86_64).family,
    )
    assertFalse(OperatingSystemFamily.BSD == OperatingSystemFamily.UNIX)
    assertTrue(setOf(OperatingSystemFamily.BSD, OperatingSystemFamily.UNIX).size == 2)
  }

  private fun effectiveOptLevel(settings: CompilerSettings): LlvmOptLevel =
    settings.llvmOptLevel
      ?: if (settings.buildMode == BuildMode.DEBUG) LlvmOptLevel.O0 else LlvmOptLevel.O3

  private fun readRecords(path: java.nio.file.Path): List<String> =
    Files.readAllBytes(path).toString(Charsets.UTF_8).split('\u0000').dropLastWhile(String::isEmpty)
}
