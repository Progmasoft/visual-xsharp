/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

import java.io.Serializable

class OperatingSystem private constructor(
  val name: String,
) {
  override fun toString() = name

  companion object {
    val LINUX = OperatingSystem("LINUX")
    val MACOS = OperatingSystem("MACOS")
    val WINDOWS = OperatingSystem("WINDOWS")
    val FREEBSD = OperatingSystem("FREEBSD")
    val OPENBSD = OperatingSystem("OPENBSD")
    val NETBSD = OperatingSystem("NETBSD")
    val UNKNOWN = OperatingSystem("UNKNOWN")
    internal val REACTOS_HOST = OperatingSystem("REACTOS")
  }
}

class OperatingSystemFamily private constructor(
  private val membership: Int,
  private val displayName: String,
) {
  override fun equals(other: Any?) =
    when {
      other is OperatingSystemFamily -> membership and other.membership != 0
      other === OperatingSystem.WINDOWS -> membership and 0b0100 != 0
      else -> false
    }

  override fun hashCode() = 0

  override fun toString() = displayName

  companion object {
    val UNIX = OperatingSystemFamily(0b0001, "UNIX")
    val BSD = OperatingSystemFamily(0b0010, "BSD")
    val WINDOWS = OperatingSystemFamily(0b0100, "WINDOWS")
    val UNKNOWN = OperatingSystemFamily(0b1000, "UNKNOWN")

    internal fun forOperatingSystem(os: OperatingSystem) =
      when (os) {
        OperatingSystem.FREEBSD, OperatingSystem.OPENBSD, OperatingSystem.NETBSD ->
          OperatingSystemFamily(0b0011, "BSD")
        OperatingSystem.LINUX, OperatingSystem.MACOS -> UNIX
        OperatingSystem.WINDOWS, OperatingSystem.REACTOS_HOST -> WINDOWS
        else -> UNKNOWN
      }
  }
}

enum class Architecture { X86_64, AARCH64, ARMV7H, RISCV64, UNKNOWN }

enum class WarningLevel { ALL, MEDIUM, LOW, NONE }

data class Host(
  val os: OperatingSystem,
  val family: OperatingSystemFamily,
  val architecture: Architecture,
)

data class ProjectIdentity(
  val name: String,
  val channel: String,
  val version: String,
) : Serializable

data class Author(
  val name: String,
  val email: String,
) : Serializable

data class CompilerSettings(
  var warningLevel: WarningLevel = WarningLevel.MEDIUM,
  var warningsAsErrors: Boolean = false,
  var verbose: Boolean = true,
) : Serializable

data class ModuleSource(
  val moduleName: String,
  val path: String,
) : Serializable

data class ModuleDependency(
  val name: String,
  val stability: String,
  val version: String,
) : Serializable

data class OptionalModuleDependency(
  val feature: String,
  val module: ModuleDependency,
) : Serializable

data class ModuleFeatureSelection(
  val moduleName: String,
  val feature: String,
  val enabled: Boolean,
) : Serializable

data class DependencyResolution(
  val required: List<ModuleDependency>,
  val optional: List<OptionalModuleDependency>,
  val features: List<ModuleFeatureSelection>,
) : Serializable {
  val activeModules: List<ModuleDependency>
    get() {
      val enabled = features.filter(ModuleFeatureSelection::enabled).mapTo(mutableSetOf()) { it.moduleName to it.feature }
      return (required + optional.filter { (it.module.name to it.feature) in enabled }.map(OptionalModuleDependency::module))
        .distinctBy(ModuleDependency::name)
        .sortedWith(moduleDependencyOrder)
    }
}

data class ArtifactTarget(
  val name: String,
  val path: String,
) : Serializable

data class ProjectState(
  val identity: ProjectIdentity?,
  val variables: Map<String, List<String>>,
  val binaries: List<ArtifactTarget>,
  val libraries: List<ArtifactTarget>,
  val authors: List<Author>,
  val modules: List<ModuleDependency>,
  val optionalModules: List<OptionalModuleDependency>,
  val dependencyFeatures: List<ModuleFeatureSelection>,
  val sourceIncludes: List<String>,
  val sourceExcludes: List<String>,
  val sourceFilters: List<String>?,
  val moduleIncludes: List<String>,
  val moduleExcludes: List<String>,
  val moduleFilters: List<String>?,
  val moduleSources: List<ModuleSource>,
  val testIncludes: List<String>,
  val testExcludes: List<String>,
  val testFilters: List<String>?,
  val testFramework: String?,
  val compiler: CompilerSettings,
) : Serializable

data class ProjectPlan(
  val identity: ProjectIdentity,
  val variables: Map<String, List<String>>,
  val binaries: List<ArtifactTarget>,
  val libraries: List<ArtifactTarget>,
  val authors: List<Author>,
  val requiredModules: List<ModuleDependency>,
  val modules: List<ModuleDependency>,
  val optionalModules: List<OptionalModuleDependency>,
  val dependencyFeatures: List<ModuleFeatureSelection>,
  val sourceIncludes: List<String>,
  val sourceExcludes: List<String>,
  val sourceFilters: List<String>?,
  val moduleIncludes: List<String>,
  val moduleExcludes: List<String>,
  val moduleFilters: List<String>?,
  val moduleSources: List<ModuleSource>,
  val testIncludes: List<String>,
  val testExcludes: List<String>,
  val testFilters: List<String>?,
  val testFramework: String?,
  val compiler: CompilerSettings,
)

internal val moduleDependencyOrder =
  compareBy(ModuleDependency::name, ModuleDependency::stability, ModuleDependency::version)

class ProjectConfigurationException(
  message: String,
) : IllegalArgumentException(message)

class ProjectAbort(
  message: String,
) : RuntimeException(message)

internal fun operatingSystemFromName(name: String): OperatingSystem {
  val osName = name.lowercase()
  return when {
    osName.contains("linux") -> OperatingSystem.LINUX
    osName.contains("mac") || osName.contains("darwin") -> OperatingSystem.MACOS
    osName.contains("reactos") -> OperatingSystem.REACTOS_HOST
    osName.contains("windows") -> OperatingSystem.WINDOWS
    osName.contains("freebsd") -> OperatingSystem.FREEBSD
    osName.contains("openbsd") -> OperatingSystem.OPENBSD
    osName.contains("netbsd") -> OperatingSystem.NETBSD
    else -> OperatingSystem.UNKNOWN
  }
}

internal fun detectHost(): Host {
  val architectureName = System.getProperty("os.arch", "").lowercase()
  val os = operatingSystemFromName(System.getProperty("os.name", ""))
  val family = OperatingSystemFamily.forOperatingSystem(os)
  val architecture =
    when (architectureName) {
      "amd64", "x86_64" -> Architecture.X86_64
      "aarch64", "arm64" -> Architecture.AARCH64
      "arm", "armv7", "armv7h" -> Architecture.ARMV7H
      "riscv64" -> Architecture.RISCV64
      else -> Architecture.UNKNOWN
    }
  return Host(os, family, architecture)
}
