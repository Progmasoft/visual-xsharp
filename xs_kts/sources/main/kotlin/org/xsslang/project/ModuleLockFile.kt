/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.sql.Connection
import java.sql.DriverManager

object ModuleLockFile {
  const val FILE_NAME = "xs.lock.sqlite3"
  private const val FORMAT_VERSION = 0

  fun write(
    root: Path,
    modules: List<ModuleDependency>,
  ) {
    val resolvedModules = resolveModuleDependencies(modules)
    Files.createDirectories(root)
    val temporary = Files.createTempFile(root, ".xs-lock-", ".sqlite3")
    try {
      DriverManager.getConnection("jdbc:sqlite:${temporary.toAbsolutePath()}").use { connection ->
        connection.autoCommit = false
        try {
          createSchema(connection)
          writeMetadata(connection)
          writeModules(connection, resolvedModules)
          connection.commit()
        } catch (error: Exception) {
          connection.rollback()
          throw error
        }
      }
      replace(temporary, root.resolve(FILE_NAME))
    } finally {
      Files.deleteIfExists(temporary)
    }
  }

  fun read(path: Path): List<ModuleDependency> =
    DriverManager.getConnection("jdbc:sqlite:${path.toAbsolutePath()}").use { connection ->
      verifyFormat(connection)
      connection
        .prepareStatement("SELECT name, stability, version FROM modules ORDER BY name, stability, version")
        .use { statement ->
          statement.executeQuery().use { rows ->
            buildList {
              while (rows.next()) {
                add(ModuleDependency(rows.getString(1), rows.getString(2), rows.getString(3)))
              }
            }
          }
        }
    }

  private fun createSchema(connection: Connection) {
    connection.createStatement().use { statement ->
      statement.execute("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL) WITHOUT ROWID")
      statement.execute(
        "CREATE TABLE modules (name TEXT PRIMARY KEY, stability TEXT NOT NULL, version TEXT NOT NULL) WITHOUT ROWID",
      )
    }
  }

  private fun writeMetadata(connection: Connection) {
    connection.prepareStatement("INSERT INTO metadata(key, value) VALUES ('format_version', ?)").use { statement ->
      statement.setString(1, FORMAT_VERSION.toString())
      statement.executeUpdate()
    }
  }

  private fun writeModules(
    connection: Connection,
    modules: List<ModuleDependency>,
  ) {
    connection.prepareStatement("INSERT INTO modules(name, stability, version) VALUES (?, ?, ?)").use { statement ->
      modules
        .sortedWith(compareBy(ModuleDependency::name, ModuleDependency::stability, ModuleDependency::version))
        .forEach { module ->
          statement.setString(1, module.name)
          statement.setString(2, module.stability)
          statement.setString(3, module.version)
          statement.addBatch()
        }
      statement.executeBatch()
    }
  }

  private fun verifyFormat(connection: Connection) {
    val version =
      connection.prepareStatement("SELECT value FROM metadata WHERE key = 'format_version'").use { statement ->
        statement.executeQuery().use { rows -> if (rows.next()) rows.getString(1) else null }
      }
    if (version != FORMAT_VERSION.toString()) {
      throw ProjectConfigurationException("unsupported xs.lock.sqlite3 format version '$version'")
    }
  }

  private fun replace(
    source: Path,
    target: Path,
  ) {
    try {
      Files.move(source, target, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
    } catch (_: AtomicMoveNotSupportedException) {
      Files.move(source, target, StandardCopyOption.REPLACE_EXISTING)
    }
  }
}
