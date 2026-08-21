/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

package org.progmasoft.visual.analyzer.config

enum class AnalysisMode {
  SYNTAX,
  SEMANTIC,
  FULL,
}

data class DiagnosticsConfiguration(
  val compiler: Boolean,
  val linter: Boolean,
  val onChange: Boolean,
  val onSave: Boolean,
)

data class WorkspaceConfiguration(val indexDependencies: Boolean)

data class PerformanceConfiguration(val workerThreads: Int)

/** Immutable configuration produced by the typed analyzer DSL. */
data class AnalyzerConfiguration(
  val version: String,
  val analysisMode: AnalysisMode,
  val diagnostics: DiagnosticsConfiguration,
  val inlayHints: Boolean,
  val formatting: Boolean,
  val workspace: WorkspaceConfiguration,
  val performance: PerformanceConfiguration,
)

class AnalyzerConfigurationException(message: String) : IllegalArgumentException(message)
