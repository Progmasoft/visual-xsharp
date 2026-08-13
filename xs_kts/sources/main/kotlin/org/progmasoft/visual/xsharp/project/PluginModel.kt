/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

import java.io.Serializable
import java.nio.file.Path

const val PROJECT_PLUGIN_API_VERSION = 1

data class PluginRequest(
  val publisher: String,
  val name: String,
  val version: String?,
  val stability: Stability?,
) : Serializable {
  val coordinate: String
    get() = "$publisher.$name"
}

data class ResolvedPlugin(
  val publisher: String,
  val name: String,
  val version: String,
  val stability: Stability,
  val apiVersion: Int,
  val sha256: String,
  val imports: List<String>,
  val artifact: Path,
) {
  val coordinate: String
    get() = "$publisher.$name"
}

data class PluginPlanEntry(
  val publisher: String,
  val name: String,
  val version: String,
  val stability: Stability,
  val apiVersion: Int,
  val sha256: String,
  val extensions: List<String>,
  val contributions: Map<String, String>,
) : Serializable {
  val coordinate: String
    get() = "$publisher.$name"
}

/**
 * A project plugin runs with the same JVM permissions as the project runtime. This API does not
 * sandbox plugin code; integrity and compatibility checks happen before activation.
 */
interface ProjectPlugin {
  val publisher: String
  val name: String
  val version: String
  val apiVersion: Int
    get() = PROJECT_PLUGIN_API_VERSION

  fun apply(context: ProjectPluginContext)
}

interface ProjectPluginContext {
  val projectRoot: Path

  fun registerExtension(name: String)

  fun contribute(
    key: String,
    value: String,
  )

  fun onFinalize(action: (ProjectPlan) -> Unit)
}
