/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
 */

package com.progmasoft.visual.xsharp.analyzer.intellij

import com.intellij.execution.configurations.GeneralCommandLine
import com.intellij.openapi.project.Project
import com.intellij.openapi.vfs.VirtualFile
import com.intellij.platform.lsp.api.LspIntegrationProvider
import com.intellij.platform.lsp.api.ProjectWideLspClientDescriptor

/**
 * IntelliJ is a host for the shared analyzer process, never a second frontend.
 * The first host slice starts one project-wide LSP client when a .vxs file is
 * opened. Configuration and deeper IDE features can be layered on this later.
 *
 * @see VisualXsharpLspClientDescriptor
 */
internal class VisualXsharpLspIntegrationProvider : LspIntegrationProvider {
    override fun fileOpened(
        project: Project,
        file: VirtualFile,
        clientStarter: LspIntegrationProvider.LspClientStarter,
    ) {
        if (file.extension == "vxs") {
            clientStarter.ensureClientStarted(VisualXsharpLspClientDescriptor(project))
        }
    }
}

/**
 * Launches the analyzer exactly once per project through IntelliJ's LSP API.
 *
 * The environment override is intentionally process-local for this first
 * slice; the default executable is resolved through the user's `PATH`.
 *
 * @param project The IntelliJ project owning the LSP client.
 */
private class VisualXsharpLspClientDescriptor(project: Project) :
    ProjectWideLspClientDescriptor(project, "Visual X# Analyzer") {
    override fun isSupportedFile(file: VirtualFile): Boolean = file.extension == "vxs"

    override fun createCommandLine(): GeneralCommandLine {
        val configured = System.getenv("VISUAL_ANALYZER_PATH")?.trim().orEmpty()
        val executable = configured.ifEmpty { "visual-analyzer" }
        return GeneralCommandLine(executable)
    }
}
