/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xdc

import java.nio.file.Files
import java.sql.DriverManager
import kotlin.io.path.readText
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFailsWith
import kotlin.test.assertTrue
import org.progmasoft.visual.xsharp.project.DependencyManifest
import org.progmasoft.visual.xsharp.project.LocalPackageDependency
import org.progmasoft.visual.xsharp.project.OptionalPackageDependency
import org.progmasoft.visual.xsharp.project.PackageDependency
import org.progmasoft.visual.xsharp.project.PackageFeatureSelection
import org.progmasoft.visual.xsharp.project.ProjectConfigurationException
import org.progmasoft.visual.xsharp.project.ProjectLockFile
import org.progmasoft.visual.xsharp.project.Stability

class LockfileDumpTest {
  @Test
  fun writesDeterministicReplayableSqlText() {
    val root = Files.createTempDirectory("visual-xsharp-vxdc-")
    try {
      val manifest =
        DependencyManifest(
          required = listOf(PackageDependency("Progmasoft", "Core", "0.3.1", Stability.STABLE)),
          optional =
            listOf(
              OptionalPackageDependency(
                "Tracing",
                PackageDependency("Progmasoft", "Trace", "0.3.1", Stability.BETA),
              )
            ),
          features = listOf(PackageFeatureSelection("Progmasoft.Trace", "Tracing", true)),
          local = listOf(LocalPackageDependency("packages/o'neil.vipkg")),
        )
      ProjectLockFile.write(root, manifest)
      val first = root.resolve("first.sqlite3.dump")
      val second = root.resolve("second.sqlite3.dump")

      LockfileDump.write(root.resolve(ProjectLockFile.FILE_NAME), first)
      LockfileDump.write(root.resolve(ProjectLockFile.FILE_NAME), second)

      val text = first.readText()
      assertEquals(text, second.readText())
      assertTrue(text.startsWith("-- Visual X# lockfile SQL dump\nPRAGMA foreign_keys=OFF;\n"))
      assertTrue(text.contains("CREATE TABLE metadata"))
      assertTrue(text.contains("INSERT INTO \"metadata\" VALUES('format_version','3');"))
      assertTrue(text.contains("INSERT INTO \"local_packages\" VALUES('packages/o''neil.vipkg');"))
      assertTrue(text.endsWith("COMMIT;\n"))

      val restored = root.resolve("restored.sqlite3")
      DriverManager.getConnection("jdbc:sqlite:$restored").use { connection ->
        connection.createStatement().use { statement ->
          val sql = StringBuilder()
          text.lineSequence().forEach { line ->
            if (sql.isEmpty() && (line.isEmpty() || line.startsWith("--"))) return@forEach
            sql.appendLine(line)
            if (line.trimEnd().endsWith(';')) {
              statement.execute(sql.toString())
              sql.setLength(0)
            }
          }
          assertTrue(sql.isEmpty(), "dump must end on a complete SQL statement")
        }
      }
      assertEquals(manifest, ProjectLockFile.read(restored))
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun rejectsBinaryLockfileAsItsOwnOutput() {
    val root = Files.createTempDirectory("visual-xsharp-vxdc-overwrite-")
    try {
      ProjectLockFile.write(root, DependencyManifest(emptyList(), emptyList(), emptyList()))
      val lockfile = root.resolve(ProjectLockFile.FILE_NAME)
      assertFailsWith<ProjectConfigurationException> { LockfileDump.write(lockfile, lockfile) }
    } finally {
      root.toFile().deleteRecursively()
    }
  }

  @Test
  fun parsesCaseSensitiveOptionsWithoutRestrictingTheOutputExtension() {
    val root = Files.createTempDirectory("visual-xsharp-vxdc-command-")
    try {
      val project = root.resolve("Visual.XSharp.kts")
      Files.writeString(project, "sources { main { entry = \"Example.Main\" } }")
      val command =
        parseVxdcCommand(
          arrayOf("-Projectfile", project.toString(), "-Output", "Example.lock-report")
        )
      assertEquals(project.toFile().canonicalFile, command.projectFile)
      assertEquals("Example.lock-report", command.output.toString())
      assertEquals(
        "dump.json",
        parseVxdcCommand(arrayOf("-Projectfile", project.toString(), "-Output", "dump.json"))
          .output
          .toString(),
      )
      assertFailsWith<ProjectConfigurationException> {
        parseVxdcCommand(
          arrayOf("-projectfile", project.toString(), "-Output", "Example.lock-report")
        )
      }
    } finally {
      root.toFile().deleteRecursively()
    }
  }
}
