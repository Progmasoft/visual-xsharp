/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.0
 */

package com.progmasoft.visual.analyzer.config

@DslMarker annotation class AnalyzerDsl

@AnalyzerDsl
class DiagnosticsScope {
  var compiler: Boolean = true
  var linter: Boolean = true
  var onChange: Boolean = true
  var onSave: Boolean = true

  internal fun snapshot() = DiagnosticsConfiguration(compiler, linter, onChange, onSave)
}

@AnalyzerDsl
class WorkspaceScope {
  var indexDependencies: Boolean = true

  internal fun snapshot() = WorkspaceConfiguration(indexDependencies)
}

@AnalyzerDsl
class PerformanceScope {
  var workerThreads: Int = 0

  internal fun snapshot(): PerformanceConfiguration {
    if (workerThreads < 0) {
      throw AnalyzerConfigurationException(
        "performance.workerThreads must be zero or a positive integer"
      )
    }
    return PerformanceConfiguration(workerThreads)
  }
}

/**
 * Typed state for `Visual.Analyzer.kts`.
 *
 * This scope only defines defaults, validation, and an immutable snapshot. It does not discover,
 * compile, load, or execute Kotlin scripts; that evaluator boundary remains separate.
 */
@AnalyzerDsl
class AnalyzerScope {
  private val diagnosticsScope = DiagnosticsScope()
  private val workspaceScope = WorkspaceScope()
  private val performanceScope = PerformanceScope()

  var version: String = "latest"
  var analysisMode: AnalysisMode = AnalysisMode.FULL
  var inlayHints: Boolean = true
  var formatting: Boolean = true

  fun diagnostics(block: DiagnosticsScope.() -> Unit) = diagnosticsScope.apply(block)

  fun workspace(block: WorkspaceScope.() -> Unit) = workspaceScope.apply(block)

  fun performance(block: PerformanceScope.() -> Unit) = performanceScope.apply(block)

  fun build() =
    AnalyzerConfiguration(
      validateAnalyzerVersion(version),
      analysisMode,
      diagnosticsScope.snapshot(),
      inlayHints,
      formatting,
      workspaceScope.snapshot(),
      performanceScope.snapshot(),
    )
}

fun analyzerConfiguration(block: AnalyzerScope.() -> Unit = {}): AnalyzerConfiguration =
  AnalyzerScope().apply(block).build()

private val semanticVersion =
  Regex("(?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)\\.(?:0|[1-9][0-9]*)(?:-[0-9A-Za-z.-]+)?")

internal fun validateAnalyzerVersion(value: String): String {
  if (value == "latest" || semanticVersion.matches(value)) return value
  throw AnalyzerConfigurationException("version must be 'latest' or a semantic version: $value")
}
