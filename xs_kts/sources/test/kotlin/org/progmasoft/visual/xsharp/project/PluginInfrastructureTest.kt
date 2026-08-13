/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import java.nio.file.Files
import java.nio.file.Path
import java.security.MessageDigest
import java.util.jar.JarEntry
import java.util.jar.JarOutputStream
import kotlin.io.path.outputStream
import kotlin.test.AfterTest
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertSame

class PluginInfrastructureTest {
  @BeforeTest
  fun reset() {
    ProjectRuntime.reset()
    PluginResolver.clearCache()
  }

  @AfterTest
  fun cleanupProperties() {
    System.clearProperty("xs.project.root")
    System.clearProperty("xs.project.output")
    System.clearProperty("xs.project.pluginManifest")
    ProjectRuntime.reset()
  }

  @Test
  fun parsesPluginPreambleWithoutExecutingProjectCode() {
    val requests =
      PluginPreamble.parse(
        """
        // plugins { plugin("Ignored") { name = "Comment" } }
        val raw = ${"\"\"\""}plugins { plugin("Ignored") { name = "RawString" } }${"\"\"\""}
        val character = '}'
        plugins {
          plugin("Progmasoft") {
            name = "CMake"
            version = "1.2.3"
            stability = Stability.BETA
          }
        }
        panic("must not execute")
        """
          .trimIndent()
      )

    assertEquals(
      PluginRequest("Progmasoft", "CMake", "1.2.3", Stability.BETA),
      requests.single(),
    )
  }

  @Test
  fun rejectsDuplicateAndUnsafeDeclarations() {
    assertFailsWith<ProjectConfigurationException> {
      PluginPreamble.parse(
        "plugins { plugin(\"Progmasoft\") { name=\"CMake\" }; plugin(\"Progmasoft\") { name=\"CMake\" } }"
      )
    }
    assertFailsWith<ProjectConfigurationException> {
      PluginPreamble.parse("plugins { plugin(\"../bad\") { name=\"CMake\" } }")
    }
  }

  @Test
  fun resolvesDescriptorAndVerifiesOptionalChecksum() {
    val root = Files.createTempDirectory("visual-xsharp-plugin-resolver-")
    try {
      val directory = Files.createDirectories(root.resolve(".visual-xsharp/plugins"))
      val artifact = createPluginJar(directory, version = "1.2.3")
      Files.writeString(artifact.resolveSibling("${artifact.fileName}.sha256"), sha256(artifact))

      val resolved =
        PluginResolver.resolve(
          root,
          listOf(PluginRequest("Progmasoft", "Fixture", "1.2.3", Stability.STABLE)),
        )

      assertEquals("Progmasoft.Fixture", resolved.single().coordinate)
      assertEquals(listOf("org.progmasoft.visual.xsharp.project.*"), resolved.single().imports)
      assertEquals(64, resolved.single().sha256.length)
      assertSame(
        resolved.single(),
        PluginResolver.resolve(
            root,
            listOf(PluginRequest("Progmasoft", "Fixture", "1.2.3", Stability.STABLE)),
          )
          .single(),
      )
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun rejectsChecksumMismatchAndApiMismatch() {
    val root = Files.createTempDirectory("visual-xsharp-plugin-integrity-")
    try {
      val directory = Files.createDirectories(root.resolve(".visual-xsharp/plugins"))
      val artifact = createPluginJar(directory)
      Files.writeString(artifact.resolveSibling("${artifact.fileName}.sha256"), "0".repeat(64))
      assertFailsWith<ProjectConfigurationException> {
        PluginResolver.resolve(root, listOf(PluginRequest("Progmasoft", "Fixture", null, null)))
      }
      Files.delete(artifact.resolveSibling("${artifact.fileName}.sha256"))
      Files.delete(artifact)
      createPluginJar(directory, apiVersion = 99)
      PluginResolver.clearCache()
      assertFailsWith<ProjectConfigurationException> {
        PluginResolver.resolve(root, listOf(PluginRequest("Progmasoft", "Fixture", null, null)))
      }
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun activatesLifecycleAndRejectsExtensionCollisions() {
    val root = Files.createTempDirectory("visual-xsharp-plugin-runtime-")
    try {
      val first = resolved(root, "First")
      val second = resolved(root, "Second")
      val firstPlugin =
        fixturePlugin("First") { context ->
          context.registerExtension("native")
          context.contribute("enabled", "true")
        }
      val secondPlugin = fixturePlugin("Second") { context -> context.registerExtension("native") }

      assertFailsWith<ProjectConfigurationException> {
        PluginRuntime.activate(listOf(first, second), listOf(firstPlugin, secondPlugin), root)
      }

      ProjectRuntime.reset()
      PluginRuntime.activate(listOf(first), listOf(firstPlugin), root)
      PluginRuntime.declare(PluginRequest("Progmasoft", "First", "1.0.0", Stability.STABLE))
      sources { main { entry = "Demo.Main" } }
      val plan = ProjectRuntime.build()
      assertEquals(listOf("native"), plan.plugins.single().extensions)
      assertEquals("true", plan.plugins.single().contributions["enabled"])
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun lockfilePersistsResolvedPluginIdentityAndDigest() {
    val root = Files.createTempDirectory("visual-xsharp-plugin-lock-")
    try {
      val plugin =
        PluginPlanEntry(
          "Progmasoft",
          "CMake",
          "1.0.0",
          Stability.STABLE,
          PROJECT_PLUGIN_API_VERSION,
          "a".repeat(64),
          listOf("cmake"),
          mapOf("generator" to "Ninja"),
        )
      ProjectLockFile.write(
        root,
        DependencyManifest(emptyList(), emptyList(), emptyList()),
        listOf(plugin),
      )
      val restored = ProjectLockFile.readPlugins(root.resolve(ProjectLockFile.FILE_NAME)).single()
      assertEquals(plugin.coordinate, restored.coordinate)
      assertEquals(plugin.sha256, restored.sha256)
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun manifestRejectsArtifactChangedAfterResolution() {
    val root = Files.createTempDirectory("visual-xsharp-plugin-manifest-")
    try {
      val directory = Files.createDirectories(root.resolve(".visual-xsharp/plugins"))
      val artifact = createPluginJar(directory)
      val resolved =
        PluginResolver.resolve(
          root,
          listOf(PluginRequest("Progmasoft", "Fixture", null, null)),
        )
      val manifest = root.resolve("plugins.manifest")
      PluginManifest.write(manifest, resolved)
      Files.writeString(artifact, "replaced")

      assertFailsWith<ProjectConfigurationException> { PluginManifest.read(manifest) }
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  private fun createPluginJar(
    directory: Path,
    version: String = "1.0.0",
    apiVersion: Int = PROJECT_PLUGIN_API_VERSION,
  ): Path {
    val path = directory.resolve("fixture-$version.jar")
    JarOutputStream(path.outputStream()).use { jar ->
      jar.putNextEntry(JarEntry("META-INF/visual-xsharp-plugin.properties"))
      jar.write(
        """
        publisher=Progmasoft
        name=Fixture
        version=$version
        stability=STABLE
        apiVersion=$apiVersion
        imports=org.progmasoft.visual.xsharp.project.*
        """
          .trimIndent()
          .toByteArray()
      )
      jar.closeEntry()
    }
    return path
  }

  private fun resolved(
    root: Path,
    name: String,
  ) =
    ResolvedPlugin(
      "Progmasoft",
      name,
      "1.0.0",
      Stability.STABLE,
      PROJECT_PLUGIN_API_VERSION,
      "a".repeat(64),
      emptyList(),
      root.resolve("$name.jar"),
    )

  private fun fixturePlugin(
    name: String,
    action: (ProjectPluginContext) -> Unit,
  ) =
    object : ProjectPlugin {
      override val publisher = "Progmasoft"
      override val name = name
      override val version = "1.0.0"

      override fun apply(context: ProjectPluginContext) = action(context)
    }

  private fun sha256(path: Path): String {
    val digest = MessageDigest.getInstance("SHA-256").digest(Files.readAllBytes(path))
    return digest.joinToString("") { "%02x".format(it) }
  }
}
