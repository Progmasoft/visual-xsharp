/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.xsslang.project

object PlanWriter {
  fun write(plan: ProjectPlan): String =
    buildString {
      append("{\"format\":\"xs-project-plan\",\"version\":0")
      field("name", plan.identity.name)
      field("channel", plan.identity.channel)
      field("projectVersion", plan.identity.version)
      mapField("variables", plan.variables)
      artifactField("binaries", plan.binaries)
      artifactField("libraries", plan.libraries)
      append(",\"authors\":[")
      plan.authors.forEachIndexed { index, author ->
        if (index > 0) append(',')
        append("{\"name\":")
          .quoted(author.name)
          .append(",\"email\":")
          .quoted(author.email)
          .append('}')
      }
      append(']')
      append(",\"modules\":[")
      plan.modules.forEachIndexed { index, module ->
        if (index > 0) append(',')
        append("{\"name\":")
          .quoted(module.name)
          .append(",\"stability\":")
          .quoted(module.stability)
          .append(",\"version\":")
          .quoted(module.version)
          .append('}')
      }
      append(']')
      arrayField("sourceIncludes", plan.sourceIncludes)
      arrayField("sourceExcludes", plan.sourceExcludes)
      arrayField("sourceFilters", plan.sourceFilters.orEmpty())
      arrayField("moduleIncludes", plan.moduleIncludes)
      arrayField("moduleExcludes", plan.moduleExcludes)
      arrayField("moduleFilters", plan.moduleFilters.orEmpty())
      append(",\"moduleSources\":[")
      plan.moduleSources.forEachIndexed { index, source ->
        if (index > 0) append(',')
        append("{\"name\":")
          .quoted(source.moduleName)
          .append(",\"path\":")
          .quoted(source.path)
          .append('}')
      }
      append(']')
      arrayField("testIncludes", plan.testIncludes)
      arrayField("testExcludes", plan.testExcludes)
      arrayField("testFilters", plan.testFilters.orEmpty())
      append(",\"testFramework\":")
      plan.testFramework?.let { quoted(it) } ?: append("null")
      append(",\"compiler\":{\"warnings\":")
      quoted(
        plan.compiler.warningLevel.name
          .lowercase(),
      )
      append(",\"werror\":${plan.compiler.warningsAsErrors},\"verbose\":${plan.compiler.verbose}}}")
    }

  private fun StringBuilder.field(
    name: String,
    value: String,
  ) = append(",\"").append(name).append("\":").quoted(value)

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

  private fun StringBuilder.mapField(
    name: String,
    values: Map<String, List<String>>,
  ) {
    append(",\"").append(name).append("\":{")
    values.entries.forEachIndexed { index, entry ->
      if (index > 0) append(',')
      quoted(entry.key).append(':')
      if (entry.value.size == 1) {
        quoted(entry.value.single())
      } else {
        append('[')
        entry.value.forEachIndexed { valueIndex, value ->
          if (valueIndex > 0) append(',')
          quoted(value)
        }
        append(']')
      }
    }
    append('}')
  }

  private fun StringBuilder.artifactField(
    name: String,
    targets: List<ArtifactTarget>,
  ) {
    append(",\"").append(name).append("\":[")
    targets.forEachIndexed { index, target ->
      if (index > 0) append(',')
      append("{\"name\":")
        .quoted(target.name)
        .append(",\"path\":")
        .quoted(target.path)
        .append('}')
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
        else -> if (character.code < 0x20) append("\\u%04x".format(character.code)) else append(character)
      }
    }
    return append('"')
  }
}
