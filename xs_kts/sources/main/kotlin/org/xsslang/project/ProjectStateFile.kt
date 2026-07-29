/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.io.ObjectInputStream
import java.io.ObjectOutputStream
import java.nio.file.Files
import java.nio.file.Path

object ProjectStateFile {
  fun readConfigured(): ProjectState? {
    val value = System.getProperty("xs.project.state")?.takeIf(String::isNotBlank) ?: return null
    val path = Path.of(value)
    if (!Files.exists(path)) return null
    ObjectInputStream(Files.newInputStream(path)).use { input ->
      return input.readObject() as? ProjectState
        ?: throw ProjectConfigurationException("invalid xs.settings.kts state")
    }
  }

  fun writeConfigured(state: ProjectState) {
    val value =
      System.getProperty("xs.project.state")?.takeIf(String::isNotBlank)
        ?: throw ProjectConfigurationException("missing split-project state path")
    ObjectOutputStream(Files.newOutputStream(Path.of(value))).use { output -> output.writeObject(state) }
  }
}
