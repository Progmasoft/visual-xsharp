/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual_xsharp.project

import java.nio.file.Files
import java.sql.DriverManager
import kotlin.test.BeforeTest
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class DependencyDslTest {
  @BeforeTest
  fun resetRuntime() {
    ProjectRuntime.reset()
  }

  @Test
  fun resolvesPublisherNameCoordinates() {
    dependencies {
      dependency("Publisher") {
        name = "Name"
        version = "1.2.3"
        stability = Stability.STABLE
      }
    }
    sources { main { entry = "Demo.Main" } }

    val dependency = ProjectRuntime.build().activeDependencies.single()
    assertEquals(PackageDependency("Publisher.Name", "STABLE", "1.2.3"), dependency)
  }

  @Test
  fun acceptsEveryRenewedStability() {
    Stability.entries.forEach { stability ->
      ProjectRuntime.reset()
      dependencies {
        dependency("Publisher") {
          name = "Package"
          version = "1.0.0"
          this.stability = stability
        }
      }
      sources { main { entry = "Demo.Main" } }
      assertEquals(stability.name, ProjectRuntime.build().activeDependencies.single().stability)
    }
  }

  @Test
  fun keepsOptionalFeatureDisabledByDefault() {
    dependencies {
      dependency("Publisher") {
        name = "Toml"
        version = "1.0.0"
        optional = "TOML"
        feature("TOML") { enabled = false }
      }
    }
    sources { main { entry = "Demo.Main" } }
    val plan = ProjectRuntime.build()

    assertTrue(plan.activeDependencies.isEmpty())
    assertEquals("Publisher.Toml", plan.optionalDependencies.single().dependency.name)
    assertFalse(plan.dependencyFeatures.single().enabled)
  }

  @Test
  fun activatesOptionalDependencyThroughItsFeature() {
    dependencies {
      dependency("Publisher") {
        name = "Toml"
        version = "1.0.0"
        optional = "TOML"
        feature("TOML") { enabled = true }
      }
    }
    sources { main { entry = "Demo.Main" } }
    val plan = ProjectRuntime.build()

    assertEquals("Publisher.Toml", plan.activeDependencies.single().name)
    assertTrue(plan.dependencyFeatures.single().enabled)
  }

  @Test
  fun validatesDependencyCoordinatesAndVersions() {
    assertFailsWith<ProjectConfigurationException> {
      dependencies { dependency("bad.publisher") { name = "Name"; version = "1.0.0" } }
    }
    ProjectRuntime.reset()
    assertFailsWith<ProjectConfigurationException> {
      dependencies { dependency("Publisher") { name = "bad-name"; version = "1.0.0" } }
    }
    ProjectRuntime.reset()
    assertFailsWith<ProjectConfigurationException> {
      dependencies { dependency("Publisher") { name = "Name"; version = "latest" } }
    }
  }

  @Test
  fun rejectsConflictingCoordinates() {
    assertFailsWith<ProjectConfigurationException> {
      dependencies {
        dependency("Publisher") { name = "Name"; version = "1.0.0" }
        dependency("Publisher") { name = "Name"; version = "2.0.0" }
      }
    }
  }

  @Test
  fun writesBinaryLockfileWithoutDump() {
    val root = Files.createTempDirectory("visual-xsharp-lock-")
    try {
      dependencies {
        dependency("Publisher") { name = "Name"; version = "1.0.0" }
      }
      sources { main { entry = "Demo.Main" } }
      val plan = ProjectRuntime.build()
      ProjectLockFile.write(root, resolveDependencies(plan.requiredDependencies, plan.optionalDependencies, plan.dependencyFeatures))
      val lock = root.resolve(ProjectLockFile.FILE_NAME)

      assertTrue(Files.isRegularFile(lock))
      assertFalse(Files.exists(root.resolve("Visual.XSharp.Lockfile.sqlite3.dump")))
      DriverManager.getConnection("jdbc:sqlite:$lock").use { connection ->
        connection.createStatement().use { statement ->
          statement.executeQuery("SELECT name, version FROM packages").use { rows ->
            assertTrue(rows.next())
            assertEquals("Publisher.Name", rows.getString("name"))
            assertEquals("1.0.0", rows.getString("version"))
          }
        }
      }
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun planContainsViGetDependencyShape() {
    dependencies {
      dependency("Publisher") { name = "Name"; version = "0.1.0"; stability = Stability.BETA }
    }
    sources { main { entry = "Demo.Main" } }
    val text = PlanWriter.write(ProjectRuntime.build())

    assertTrue(text.contains("\"publisher\":\"Publisher\""))
    assertTrue(text.contains("\"name\":\"Name\""))
    assertTrue(text.contains("\"stability\":\"BETA\""))
    assertFalse(text.contains("XSharp.JSON"))
  }
}
