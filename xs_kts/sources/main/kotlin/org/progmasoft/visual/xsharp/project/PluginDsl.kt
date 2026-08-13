/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

@XsProjectDsl
class PluginDeclarationScope internal constructor(private val publisher: String) {
  var name: String? = null
  var version: String? = null
  var path: String? = null

  internal fun request(): PluginRequest {
    if (publisher == "local") {
      if (name != null || version != null) {
        throw ProjectConfigurationException(
          "local plugin accepts only path; name and version come from its JAR descriptor"
        )
      }
      return PluginRequest(
        publisher = "local",
        name = null,
        version = null,
        path = requireLocalArtifactPath(path, "jar", "local plugin path"),
      )
    }
    if (path != null) {
      throw ProjectConfigurationException("hosted plugin does not accept a local path")
    }
    return PluginRequest(
      requireModuleSegment(publisher, "plugin publisher"),
      requireModuleSegment(
        name ?: throw ProjectConfigurationException("plugin name is required"),
        "plugin name",
      ),
      version?.let(::requirePackageVersion),
    )
  }
}

@XsProjectDsl
class PluginsScope internal constructor() {
  private val requests = mutableListOf<PluginRequest>()

  fun plugin(
    publisher: String,
    block: PluginDeclarationScope.() -> Unit,
  ) {
    val request = PluginDeclarationScope(publisher).apply(block).request()
    if (requests.any { it.coordinate == request.coordinate }) {
      throw ProjectConfigurationException(
        "plugin '${request.coordinate}' is declared more than once"
      )
    }
    requests += request
  }

  internal fun apply() = requests.forEach(PluginRuntime::declare)
}

fun plugins(block: PluginsScope.() -> Unit) = PluginsScope().apply(block).apply()
