/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual_xsharp.project

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

enum class Stability { STABLE, UNSTABLE, BETA, NIGHTLY, ALPHA, DEV }

enum class Backend { LLVM }

enum class BuildMode { DEBUG, RELEASE }

enum class Emit { BINARY, OBJECT, CORE, XPP, XMM, ASSEMBLY, LLVM_LL, LLVM_BC }

enum class Warnings { ALL, MEDIUM, LOW, NONE }

enum class LlvmOptLevel { O0, O1, O2, O3, OG }

enum class LlvmCompiler { AOT, ORC }

enum class LlvmLto { FAT, THIN, NONE }

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

data class Workspace(
  val name: String,
  val path: String,
) : Serializable

data class CompilerSettings(
  var warningLevel: WarningLevel = WarningLevel.MEDIUM,
  var warningsAsErrors: Boolean = false,
  var version: String = "latest",
  var standard: String = "latest",
  var backend: Backend = Backend.LLVM,
  var buildMode: BuildMode = BuildMode.DEBUG,
  var emit: Emit = Emit.BINARY,
  var experimentalWarnings: Boolean = false,
  var shadowWarnings: Boolean = false,
  var undefinedWarnings: Boolean = true,
  var typeSafeFormat: Boolean = true,
  var xppOptimizationPasses: Boolean = true,
  var xmmOptimizationPasses: Boolean = true,
  var llvmOptLevel: LlvmOptLevel? = null,
  var llvmCompiler: LlvmCompiler = LlvmCompiler.AOT,
  var llvmLto: LlvmLto = LlvmLto.NONE,
) : Serializable

data class PackageDependency(
  val publisher: String,
  val name: String,
  val version: String,
  val stability: Stability,
) : Serializable

data class OptionalPackageDependency(
  val feature: String,
  val dependency: PackageDependency,
) : Serializable

data class PackageFeatureSelection(
  val packageName: String,
  val feature: String,
  val enabled: Boolean,
) : Serializable

// This is the validated project declaration, not a solved registry graph. Transitive
// versions, artifact identities and repository metadata belong to the future resolver.
data class DependencyManifest(
  val required: List<PackageDependency>,
  val optional: List<OptionalPackageDependency>,
  val features: List<PackageFeatureSelection>,
) : Serializable

data class ProjectPlan(
  val identity: ProjectIdentity?,
  val authors: List<Author>,
  val requiredDependencies: List<PackageDependency>,
  val optionalDependencies: List<OptionalPackageDependency>,
  val dependencyFeatures: List<PackageFeatureSelection>,
  val entry: String,
  val releaseOutputDirectory: String,
  val debugOutputDirectory: String,
  val targets: List<String>,
  val workspaces: List<Workspace>,
  val pmlEnabled: Boolean,
  val publishSources: Boolean,
  val publishExcludes: List<String>,
  val sourceIncludes: List<String>,
  val sourceExcludes: List<String>,
  val testIncludes: List<String>,
  val testExcludes: List<String>,
  val testFramework: String?,
  val compiler: CompilerSettings,
)

internal val packageDependencyOrder =
  compareBy(PackageDependency::publisher, PackageDependency::name, PackageDependency::stability, PackageDependency::version)

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
