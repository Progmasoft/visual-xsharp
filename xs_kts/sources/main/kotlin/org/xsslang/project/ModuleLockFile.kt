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
  private const val FORMAT_VERSION = 1
  private const val LEGACY_FORMAT_VERSION = 0

  fun write(
    root: Path,
    modules: List<ModuleDependency>,
  ) = write(root, resolveDependencies(modules, emptyList(), emptyList()))

  fun write(
    root: Path,
    resolution: DependencyResolution,
  ) {
    val resolved = resolveDependencies(resolution.required, resolution.optional, resolution.features)
    Files.createDirectories(root)
    val temporary = Files.createTempFile(root, ".xs-lock-", ".sqlite3")
    try {
      DriverManager.getConnection("jdbc:sqlite:${temporary.toAbsolutePath()}").use { connection ->
        connection.autoCommit = false
        try {
          createSchema(connection)
          writeMetadata(connection)
          writeModules(connection, resolved)
          writeFeatures(connection, resolved.features)
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

  fun read(path: Path): List<ModuleDependency> = readResolution(path).activeModules

  fun readResolution(path: Path): DependencyResolution =
    DriverManager.getConnection("jdbc:sqlite:${path.toAbsolutePath()}").use { connection ->
      when (val version = formatVersion(connection)) {
        LEGACY_FORMAT_VERSION -> readLegacy(connection)
        FORMAT_VERSION -> readCurrent(connection)
        else -> throw ProjectConfigurationException("unsupported xs.lock.sqlite3 format version '$version'")
      }
    }

  private fun createSchema(connection: Connection) {
    connection.createStatement().use { statement ->
      statement.execute("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL) WITHOUT ROWID")
      statement.execute(
        """CREATE TABLE modules (
          name TEXT NOT NULL,
          stability TEXT NOT NULL,
          version TEXT NOT NULL,
          optional_feature TEXT NOT NULL,
          enabled INTEGER NOT NULL CHECK(enabled IN (0, 1)),
          required INTEGER NOT NULL CHECK(required IN (0, 1)),
          PRIMARY KEY(name, optional_feature),
          CHECK((required = 1 AND optional_feature = '') OR (required = 0 AND optional_feature != ''))
        ) WITHOUT ROWID""".trimIndent(),
      )
      statement.execute(
        """CREATE TABLE features (
          module_name TEXT NOT NULL,
          feature TEXT NOT NULL,
          enabled INTEGER NOT NULL CHECK(enabled IN (0, 1)),
          PRIMARY KEY(module_name, feature),
          FOREIGN KEY(module_name, feature) REFERENCES modules(name, optional_feature)
        ) WITHOUT ROWID""".trimIndent(),
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
    resolution: DependencyResolution,
  ) {
    val enabled = resolution.features.filter(ModuleFeatureSelection::enabled).mapTo(mutableSetOf()) { it.moduleName to it.feature }
    connection
      .prepareStatement(
        "INSERT INTO modules(name, stability, version, optional_feature, enabled, required) VALUES (?, ?, ?, ?, ?, ?)",
      ).use { statement ->
        resolution.required.forEach { module ->
          statement.setString(1, module.name)
          statement.setString(2, module.stability)
          statement.setString(3, module.version)
          statement.setString(4, "")
          statement.setInt(5, 1)
          statement.setInt(6, 1)
          statement.addBatch()
        }
        resolution.optional.forEach { declaration ->
          statement.setString(1, declaration.module.name)
          statement.setString(2, declaration.module.stability)
          statement.setString(3, declaration.module.version)
          statement.setString(4, declaration.feature)
          statement.setInt(5, if ((declaration.module.name to declaration.feature) in enabled) 1 else 0)
          statement.setInt(6, 0)
          statement.addBatch()
        }
        statement.executeBatch()
      }
  }

  private fun writeFeatures(
    connection: Connection,
    features: List<ModuleFeatureSelection>,
  ) {
    connection.prepareStatement("INSERT INTO features(module_name, feature, enabled) VALUES (?, ?, ?)").use { statement ->
      features.forEach { feature ->
        statement.setString(1, feature.moduleName)
        statement.setString(2, feature.feature)
        statement.setInt(3, if (feature.enabled) 1 else 0)
        statement.addBatch()
      }
      statement.executeBatch()
    }
  }

  private fun readCurrent(connection: Connection): DependencyResolution {
    val required = mutableListOf<ModuleDependency>()
    val optional = mutableListOf<OptionalModuleDependency>()
    connection
      .prepareStatement(
        "SELECT name, stability, version, optional_feature, required FROM modules ORDER BY name",
      ).use { statement ->
        statement.executeQuery().use { rows ->
          while (rows.next()) {
            val module = ModuleDependency(rows.getString(1), rows.getString(2), rows.getString(3))
            if (rows.getInt(5) == 1) {
              required += module
            } else {
              optional += OptionalModuleDependency(rows.getString(4), module)
            }
          }
        }
      }
    val features =
      connection.prepareStatement("SELECT module_name, feature, enabled FROM features ORDER BY module_name, feature").use {
        statement ->
        statement.executeQuery().use { rows ->
          buildList {
            while (rows.next()) {
              add(ModuleFeatureSelection(rows.getString(1), rows.getString(2), rows.getInt(3) == 1))
            }
          }
        }
      }
    return resolveDependencies(required, optional, features)
  }

  private fun readLegacy(connection: Connection): DependencyResolution {
    val modules =
      connection.prepareStatement("SELECT name, stability, version FROM modules ORDER BY name, stability, version").use {
        statement ->
        statement.executeQuery().use { rows ->
          buildList {
            while (rows.next()) add(ModuleDependency(rows.getString(1), rows.getString(2), rows.getString(3)))
          }
        }
      }
    return DependencyResolution(modules, emptyList(), emptyList())
  }

  private fun formatVersion(connection: Connection): Int? =
    connection.prepareStatement("SELECT value FROM metadata WHERE key = 'format_version'").use { statement ->
      statement.executeQuery().use { rows -> if (rows.next()) rows.getString(1).toIntOrNull() else null }
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
