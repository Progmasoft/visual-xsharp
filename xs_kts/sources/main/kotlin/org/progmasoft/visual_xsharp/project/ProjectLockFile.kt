/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual_xsharp.project

import java.nio.file.AtomicMoveNotSupportedException
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.StandardCopyOption
import java.sql.Connection
import java.sql.DriverManager

object ProjectLockFile {
  const val FILE_NAME = "Visual.XSharp.Lockfile.sqlite3"
  private const val FORMAT_VERSION = 1

  fun write(root: Path, resolution: DependencyResolution) {
    val resolved = resolveDependencies(resolution.required, resolution.optional, resolution.features)
    Files.createDirectories(root)
    val temporary = Files.createTempFile(root, ".visual-xsharp-lock-", ".sqlite3")
    try {
      DriverManager.getConnection("jdbc:sqlite:${temporary.toAbsolutePath()}").use { connection ->
        connection.autoCommit = false
        try {
          createSchema(connection)
          writeMetadata(connection)
          writePackages(connection, resolved)
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

  fun read(path: Path): DependencyResolution =
    DriverManager.getConnection("jdbc:sqlite:${path.toAbsolutePath()}").use { connection ->
      val version = connection.prepareStatement("SELECT value FROM metadata WHERE key = 'format_version'").use { statement ->
        statement.executeQuery().use { rows -> if (rows.next()) rows.getString(1).toIntOrNull() else null }
      }
      if (version != FORMAT_VERSION) {
        throw ProjectConfigurationException("unsupported Visual.XSharp.Lockfile.sqlite3 format version '$version'")
      }
      readPackages(connection)
    }

  private fun createSchema(connection: Connection) {
    connection.createStatement().use { statement ->
      statement.execute("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL) WITHOUT ROWID")
      statement.execute(
        """CREATE TABLE packages (
          name TEXT NOT NULL,
          stability TEXT NOT NULL,
          version TEXT NOT NULL,
          optional_feature TEXT NOT NULL,
          required INTEGER NOT NULL CHECK(required IN (0, 1)),
          PRIMARY KEY(name, optional_feature),
          CHECK((required = 1 AND optional_feature = '') OR (required = 0 AND optional_feature != ''))
        ) WITHOUT ROWID""".trimIndent(),
      )
      statement.execute(
        """CREATE TABLE package_features (
          package_name TEXT NOT NULL,
          feature TEXT NOT NULL,
          enabled INTEGER NOT NULL CHECK(enabled IN (0, 1)),
          PRIMARY KEY(package_name, feature),
          FOREIGN KEY(package_name, feature) REFERENCES packages(name, optional_feature)
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

  private fun writePackages(connection: Connection, resolution: DependencyResolution) {
    connection.prepareStatement(
      "INSERT INTO packages(name, stability, version, optional_feature, required) VALUES (?, ?, ?, ?, ?)",
    ).use { statement ->
      resolution.required.forEach { dependency ->
        statement.setString(1, dependency.name)
        statement.setString(2, dependency.stability)
        statement.setString(3, dependency.version)
        statement.setString(4, "")
        statement.setInt(5, 1)
        statement.addBatch()
      }
      resolution.optional.forEach { declaration ->
        statement.setString(1, declaration.dependency.name)
        statement.setString(2, declaration.dependency.stability)
        statement.setString(3, declaration.dependency.version)
        statement.setString(4, declaration.feature)
        statement.setInt(5, 0)
        statement.addBatch()
      }
      statement.executeBatch()
    }
  }

  private fun writeFeatures(connection: Connection, features: List<PackageFeatureSelection>) {
    connection.prepareStatement(
      "INSERT INTO package_features(package_name, feature, enabled) VALUES (?, ?, ?)",
    ).use { statement ->
      features.forEach { feature ->
        statement.setString(1, feature.packageName)
        statement.setString(2, feature.feature)
        statement.setInt(3, if (feature.enabled) 1 else 0)
        statement.addBatch()
      }
      statement.executeBatch()
    }
  }

  private fun readPackages(connection: Connection): DependencyResolution {
    val required = mutableListOf<PackageDependency>()
    val optional = mutableListOf<OptionalPackageDependency>()
    connection.prepareStatement(
      "SELECT name, stability, version, optional_feature, required FROM packages ORDER BY name, optional_feature",
    ).use { statement ->
      statement.executeQuery().use { rows ->
        while (rows.next()) {
          val dependency = PackageDependency(rows.getString(1), rows.getString(2), rows.getString(3))
          if (rows.getInt(5) == 1) required += dependency
          else optional += OptionalPackageDependency(rows.getString(4), dependency)
        }
      }
    }
    val features = connection.prepareStatement(
      "SELECT package_name, feature, enabled FROM package_features ORDER BY package_name, feature",
    ).use { statement ->
      statement.executeQuery().use { rows ->
        buildList {
          while (rows.next()) add(PackageFeatureSelection(rows.getString(1), rows.getString(2), rows.getInt(3) == 1))
        }
      }
    }
    return resolveDependencies(required, optional, features)
  }

  private fun replace(source: Path, target: Path) {
    try {
      Files.move(source, target, StandardCopyOption.ATOMIC_MOVE, StandardCopyOption.REPLACE_EXISTING)
    } catch (_: AtomicMoveNotSupportedException) {
      Files.move(source, target, StandardCopyOption.REPLACE_EXISTING)
    }
  }
}
