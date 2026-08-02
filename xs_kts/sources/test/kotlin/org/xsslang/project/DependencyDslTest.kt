/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.sql.DriverManager
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class DependencyDslTest {
  private fun project(block: ProjectContext.() -> Unit = {}) =
    ProjectContext().apply {
      project("DependencyTest", "BETA", "0.2.5")
      block()
    }

  @Test
  fun normalizesRequiredCoordinates() {
    val plan =
      project {
        dependencies {
          addModule("XSharp.JSON", "stable", "1.2.3")
          addModule("XSharp.XML", "BETA", "2.0.0-alpha.1")
        }
      }.build()

    assertEquals(
      listOf(
        ModuleDependency("XSharp.JSON", "STABLE", "1.2.3"),
        ModuleDependency("XSharp.XML", "BETA", "2.0.0-alpha.1"),
      ),
      plan.requiredModules,
    )
    assertEquals(plan.requiredModules, plan.modules)
  }

  @Test
  fun optionalModuleIsInactiveByDefault() {
    val plan =
      project {
        dependencies {
          addOptionalModule("JSON", "XSharp.JSON", "STABLE", "1.0.0")
        }
      }.build()

    assertTrue(plan.modules.isEmpty())
    assertEquals(
      listOf(OptionalModuleDependency("JSON", ModuleDependency("XSharp.JSON", "STABLE", "1.0.0"))),
      plan.optionalModules,
    )
    assertEquals(
      listOf(ModuleFeatureSelection("XSharp.JSON", "JSON", false)),
      plan.dependencyFeatures,
    )
  }

  @Test
  fun enabledOptionalModuleJoinsActiveRegistry() {
    val plan =
      project {
        dependencies {
          addModule("XSharp.Core", "STABLE", "1.0.0")
          addOptionalModule("JSON", "XSharp.JSON", "BETA", "2.1.0")
        }
        features {
          dependency("XSharp.JSON") {
            feature("JSON", true)
          }
        }
      }.build()

    assertEquals(
      listOf(
        ModuleDependency("XSharp.Core", "STABLE", "1.0.0"),
        ModuleDependency("XSharp.JSON", "BETA", "2.1.0"),
      ),
      plan.modules,
    )
    assertEquals(ModuleFeatureSelection("XSharp.JSON", "JSON", true), plan.dependencyFeatures.single())
  }

  @Test
  fun enableAndDisableHelpersUseLastSelection() {
    val context =
      project {
        dependencies {
          addOptionalModule("TLS", "XSharp.Network", "ALPHA", "0.4.0")
        }
        features {
          dependency("XSharp.Network") {
            enable("TLS")
            disable("TLS")
          }
        }
      }

    assertTrue(context.build().modules.isEmpty())
    context.features {
      dependency("XSharp.Network") {
        enable("TLS")
      }
    }
    assertEquals("XSharp.Network", context.build().modules.single().name)
  }

  @Test
  fun severalFeaturesCanActivateOneCoordinate() {
    val plan =
      project {
        dependencies {
          addOptionalModule("JSON", "XSharp.Serialization", "STABLE", "3.0.0")
          addOptionalModule("XML", "XSharp.Serialization", "STABLE", "3.0.0")
        }
        features {
          dependency("XSharp.Serialization") {
            enable("XML")
          }
        }
      }.build()

    assertEquals(listOf(ModuleDependency("XSharp.Serialization", "STABLE", "3.0.0")), plan.modules)
    assertEquals(2, plan.optionalModules.size)
    assertEquals(
      listOf(
        ModuleFeatureSelection("XSharp.Serialization", "JSON", false),
        ModuleFeatureSelection("XSharp.Serialization", "XML", true),
      ),
      plan.dependencyFeatures,
    )
  }

  @Test
  fun rejectsUnknownFeatureSelection() {
    val error =
      assertFailsWith<ProjectConfigurationException> {
        project {
          dependencies {
            addOptionalModule("JSON", "XSharp.JSON", "STABLE", "1.0.0")
          }
          features {
            dependency("XSharp.JSON") {
              enable("YAML")
            }
          }
        }
      }
    assertTrue(error.message.orEmpty().contains("not declared"))
  }

  @Test
  fun rejectsRequiredAndOptionalCoordinateCollision() {
    val error =
      assertFailsWith<ProjectConfigurationException> {
        project {
          dependencies {
            addModule("XSharp.JSON", "STABLE", "1.0.0")
            addOptionalModule("JSON", "XSharp.JSON", "STABLE", "1.0.0")
          }
        }
      }
    assertTrue(error.message.orEmpty().contains("both required and optional"))
  }

  @Test
  fun rejectsConflictingOptionalCoordinates() {
    val error =
      assertFailsWith<ProjectConfigurationException> {
        project {
          dependencies {
            addOptionalModule("JSON", "XSharp.JSON", "STABLE", "1.0.0")
            addOptionalModule("Schema", "XSharp.JSON", "BETA", "2.0.0")
          }
        }
      }
    assertTrue(error.message.orEmpty().contains("conflicting coordinates"))
  }

  @Test
  fun duplicateIdenticalDeclarationsAreIdempotent() {
    val plan =
      project {
        dependencies {
          addModule("XSharp.Core", "STABLE", "1.0.0")
          addModule("XSharp.Core", "stable", "1.0.0")
          addOptionalModule("JSON", "XSharp.JSON", "BETA", "2.0.0")
          addOptionalModule("JSON", "XSharp.JSON", "beta", "2.0.0")
        }
      }.build()

    assertEquals(1, plan.requiredModules.size)
    assertEquals(1, plan.optionalModules.size)
  }

  @Test
  fun validatesPublisherNameCoordinate() {
    listOf("JSON", "XSharp", ".JSON", "XSharp.", "XSharp-Tools.JSON").forEach { name ->
      assertFailsWith<ProjectConfigurationException>(name) {
        project {
          dependencies {
            addModule(name, "STABLE", "1.0.0")
          }
        }
      }
    }
    val nested =
      project {
        dependencies {
          addModule("XSharp.Web.JSON", "STABLE", "1.0.0")
        }
      }.build()
    assertEquals("XSharp.Web.JSON", nested.modules.single().name)
  }

  @Test
  fun validatesStabilityVocabulary() {
    listOf("stable", "BETA", "Alpha").forEach { stability ->
      val plan =
        project {
          dependencies {
            addModule("XSharp.$stability", stability, "1.0.0")
          }
        }.build()
      assertEquals(stability.uppercase(), plan.modules.single().stability)
    }
    assertFailsWith<ProjectConfigurationException> {
      project {
        dependencies {
          addModule("XSharp.Nightly", "NIGHTLY", "1.0.0")
        }
      }
    }
  }

  @Test
  fun validatesExactSemanticVersions() {
    listOf("1", "1.0", "01.0.0", "v1.0.0", "1.0.*", "").forEach { version ->
      assertFailsWith<ProjectConfigurationException>(version) {
        project {
          dependencies {
            addModule("XSharp.Version", "STABLE", version)
          }
        }
      }
    }
    val prerelease =
      project {
        dependencies {
          addModule("XSharp.Version", "ALPHA", "0.3.0-rc.2")
        }
      }.build()
    assertEquals("0.3.0-rc.2", prerelease.modules.single().version)
  }

  @Test
  fun validatesFeatureIdentifiers() {
    listOf("", "json-parser", "2D", "with space", "A::B").forEach { feature ->
      assertFailsWith<ProjectConfigurationException>(feature) {
        project {
          dependencies {
            addOptionalModule(feature, "XSharp.JSON", "STABLE", "1.0.0")
          }
        }
      }
    }
  }

  @Test
  fun snapshotPreservesOptionalDeclarationsAndSelections() {
    val settings =
      project {
        dependencies {
          addOptionalModule("JSON", "XSharp.JSON", "STABLE", "1.0.0")
        }
        features {
          dependency("XSharp.JSON") {
            enable("JSON")
          }
        }
      }
    val restored = ProjectContext(state = settings.snapshot()).build()

    assertEquals(settings.build().modules, restored.modules)
    assertEquals(settings.build().optionalModules, restored.optionalModules)
    assertEquals(settings.build().dependencyFeatures, restored.dependencyFeatures)
  }

  @Test
  fun planWriterEmitsActiveAndOptionalRegistries() {
    val plan =
      project {
        dependencies {
          addOptionalModule("JSON", "XSharp.JSON", "STABLE", "1.0.0")
        }
        features {
          dependency("XSharp.JSON") {
            enable("JSON")
          }
        }
      }.build()
    val text = PlanWriter.write(plan)

    assertTrue(text.contains("\"modules\":[{\"name\":\"XSharp.JSON\""))
    assertTrue(text.contains("\"optionalModules\":[{\"feature\":\"JSON\""))
    assertTrue(text.contains("\"dependencyFeatures\":[{\"module\":\"XSharp.JSON\""))
    assertTrue(text.contains("\"enabled\":true"))
  }

  @Test
  fun lockFileRoundTripsOptionalResolution() {
    val root = Files.createTempDirectory("xs-project-optional-lock-")
    try {
      val resolution =
        resolveDependencies(
          listOf(ModuleDependency("XSharp.Core", "STABLE", "1.0.0")),
          listOf(
            OptionalModuleDependency("JSON", ModuleDependency("XSharp.JSON", "BETA", "2.0.0")),
            OptionalModuleDependency("XML", ModuleDependency("XSharp.XML", "ALPHA", "0.5.0")),
          ),
          listOf(
            ModuleFeatureSelection("XSharp.JSON", "JSON", true),
            ModuleFeatureSelection("XSharp.XML", "XML", false),
          ),
        )
      ModuleLockFile.write(root, resolution)
      val path = root.resolve(ModuleLockFile.FILE_NAME)

      assertEquals(resolution, ModuleLockFile.readResolution(path))
      assertEquals(
        listOf(
          ModuleDependency("XSharp.Core", "STABLE", "1.0.0"),
          ModuleDependency("XSharp.JSON", "BETA", "2.0.0"),
        ),
        ModuleLockFile.read(path),
      )
      val header = Files.readAllBytes(path).copyOfRange(0, 16).toString(StandardCharsets.UTF_8)
      assertEquals("SQLite format 3\u0000", header)
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun lockFileSupportsSeveralFeaturesForOneModule() {
    val root = Files.createTempDirectory("xs-project-multi-feature-lock-")
    try {
      val coordinate = ModuleDependency("XSharp.Serialization", "STABLE", "3.0.0")
      val resolution =
        resolveDependencies(
          emptyList(),
          listOf(
            OptionalModuleDependency("JSON", coordinate),
            OptionalModuleDependency("XML", coordinate),
          ),
          listOf(
            ModuleFeatureSelection(coordinate.name, "JSON", true),
            ModuleFeatureSelection(coordinate.name, "XML", false),
          ),
        )
      ModuleLockFile.write(root, resolution)

      assertEquals(resolution, ModuleLockFile.readResolution(root.resolve(ModuleLockFile.FILE_NAME)))
      assertEquals(listOf(coordinate), ModuleLockFile.read(root.resolve(ModuleLockFile.FILE_NAME)))
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun lockFileSchemaStoresNoTimestampsOrReadableManifest() {
    val root = Files.createTempDirectory("xs-project-lock-schema-")
    try {
      val resolution =
        resolveDependencies(
          emptyList(),
          listOf(OptionalModuleDependency("JSON", ModuleDependency("XSharp.JSON", "STABLE", "1.0.0"))),
          listOf(ModuleFeatureSelection("XSharp.JSON", "JSON", false)),
        )
      ModuleLockFile.write(root, resolution)
      val path = root.resolve(ModuleLockFile.FILE_NAME)

      DriverManager.getConnection("jdbc:sqlite:${path.toAbsolutePath()}").use { connection ->
        val tables =
          connection.prepareStatement("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name").use { statement ->
            statement.executeQuery().use { rows -> buildList { while (rows.next()) add(rows.getString(1)) } }
          }
        assertEquals(listOf("features", "metadata", "modules"), tables)
        val columns =
          connection.createStatement().use { statement ->
            statement.executeQuery("PRAGMA table_info(modules)").use { rows ->
              buildList { while (rows.next()) add(rows.getString("name")) }
            }
          }
        assertFalse(columns.any { it.contains("time", ignoreCase = true) })
      }
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun lockWritesAreDeterministicForEquivalentInputOrder() {
    val first = Files.createTempDirectory("xs-project-lock-first-")
    val second = Files.createTempDirectory("xs-project-lock-second-")
    try {
      val required =
        listOf(
          ModuleDependency("XSharp.XML", "STABLE", "2.0.0"),
          ModuleDependency("XSharp.JSON", "BETA", "1.0.0"),
        )
      ModuleLockFile.write(first, required)
      ModuleLockFile.write(second, required.reversed())

      assertTrue(
        Files.readAllBytes(first.resolve(ModuleLockFile.FILE_NAME)).contentEquals(
          Files.readAllBytes(second.resolve(ModuleLockFile.FILE_NAME)),
        ),
      )
    } finally {
      first.toFile().deleteRecursively()
      second.toFile().deleteRecursively()
    }
  }
}
