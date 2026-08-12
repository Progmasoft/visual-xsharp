/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.xsharp.project

object PlanWriter {
  fun write(plan: ProjectPlan): String = buildString {
    append("{\"format\":\"visual-xsharp-project-plan\",\"version\":1")
    append(",\"project\":")
    plan.identity?.let { identity ->
      append("{\"name\":").quoted(identity.name)
      append(",\"version\":").quoted(identity.version)
      append(",\"stability\":").quoted(identity.channel)
      append('}')
    } ?: append("null")
    append(",\"compiler\":{")
    append("\"version\":").quoted(plan.compiler.version)
    append(",\"standard\":").quoted(plan.compiler.standard)
    append(",\"backend\":").quoted(plan.compiler.backend.name.lowercase())
    append(",\"buildMode\":").quoted(plan.compiler.buildMode.name.lowercase())
    append(",\"emit\":").quoted(plan.compiler.emit.name.lowercase())
    append(",\"warnings\":").quoted(plan.compiler.warningLevel.name.lowercase())
    append(",\"warningsAsErrors\":${plan.compiler.warningsAsErrors}")
    append(",\"experimentalWarnings\":${plan.compiler.experimentalWarnings}")
    append(",\"shadowWarnings\":${plan.compiler.shadowWarnings}")
    append(",\"undefinedWarnings\":${plan.compiler.undefinedWarnings}")
    append(",\"unsafe\":{")
    append("\"xppOptimizationPasses\":${plan.compiler.xppOptimizationPasses}")
    append(",\"xmmOptimizationPasses\":${plan.compiler.xmmOptimizationPasses}")
    append(",\"typeSafeFormat\":${plan.compiler.typeSafeFormat}}")
    append(",\"llvm\":{")
    val optLevel =
      plan.compiler.llvmOptLevel
        ?: if (plan.compiler.buildMode == BuildMode.DEBUG) LlvmOptLevel.O0 else LlvmOptLevel.O3
    append("\"optLevel\":").quoted(optLevel.name.lowercase())
    append(",\"compiler\":").quoted(plan.compiler.llvmCompiler.name.lowercase())
    append(",\"lto\":").quoted(plan.compiler.llvmLto.name.lowercase())
    append("}}")
    append(",\"outdirs\":{")
    append("\"release\":").quoted(plan.releaseOutputDirectory)
    append(",\"debug\":").quoted(plan.debugOutputDirectory)
    append('}')
    arrayField("targets", plan.targets)
    append(",\"authors\":[")
    plan.authors.forEachIndexed { index, author ->
      if (index > 0) append(',')
      append("{\"name\":").quoted(author.name)
      append(",\"email\":").quoted(author.email).append('}')
    }
    append(']')
    append(",\"pml\":{")
    append("\"enabled\":${plan.pmlEnabled}}")
    append(",\"dependencies\":[")
    plan.requiredDependencies.forEachIndexed { index, dependency ->
      if (index > 0) append(',')
      dependency(dependency, null, false)
    }
    plan.optionalDependencies.forEach { declaration ->
      if (
        plan.requiredDependencies.isNotEmpty() || declaration != plan.optionalDependencies.first()
      )
        append(',')
      val enabled =
        plan.dependencyFeatures.any {
          it.packageName == declaration.dependency.coordinate &&
            it.feature == declaration.feature &&
            it.enabled
        }
      dependency(declaration.dependency, declaration.feature, enabled)
    }
    append(']')
    append(",\"workspaces\":[")
    plan.workspaces.forEachIndexed { index, workspace ->
      if (index > 0) append(',')
      append("{\"name\":")
        .quoted(workspace.name)
        .append(",\"path\":")
        .quoted(workspace.path)
        .append('}')
    }
    append(']')
    append(",\"sources\":{")
    append("\"viget\":{\"publish\":${plan.publishSources}")
    arrayField("exclude", plan.publishExcludes)
    append('}')
    append(",\"main\":{")
    append("\"srcDir\":").quoted(plan.sourceIncludes.singleOrNull() ?: "Sources")
    append(",\"entry\":").quoted(plan.entry)
    arrayField("exclude", plan.sourceExcludes)
    append('}')
    append(",\"test\":{")
    append("\"testDir\":").quoted(plan.testIncludes.singleOrNull() ?: "Tests")
    append(",\"framework\":")
    plan.testFramework?.let { framework -> quoted(framework) } ?: append("null")
    arrayField("exclude", plan.testExcludes)
    append("}}}")
  }

  private fun StringBuilder.dependency(
    packageDependency: PackageDependency,
    feature: String?,
    enabled: Boolean,
  ) {
    append("{\"publisher\":").quoted(packageDependency.publisher)
    append(",\"name\":").quoted(packageDependency.name)
    append(",\"version\":").quoted(packageDependency.version)
    append(",\"stability\":").quoted(packageDependency.stability.name)
    if (feature != null) {
      append(",\"optional\":").quoted(feature)
      append(",\"features\":{").quoted(feature).append(":$enabled}")
    }
    append('}')
  }

  private fun StringBuilder.arrayField(
    name: String,
    values: List<String>,
  ) {
    append(",\"").append(name).append("\":[")
    values.forEachIndexed { index, value ->
      if (index > 0) append(',')
      quoted(value)
    }
    append(']')
  }

  private fun StringBuilder.quoted(value: String): StringBuilder {
    append('"')
    value.forEach { character ->
      when (character) {
        '"' -> append("\\\"")
        '\\' -> append("\\\\")
        '\n' -> append("\\n")
        '\r' -> append("\\r")
        '\t' -> append("\\t")
        else ->
          if (character.code < 0x20) append("\\u%04x".format(character.code)) else append(character)
      }
    }
    return append('"')
  }
}
