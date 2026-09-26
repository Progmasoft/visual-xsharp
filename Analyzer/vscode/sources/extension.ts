/*
 * SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
 * SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1
 */

import * as vscode from "vscode";
import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
} from "vscode-languageclient/node";

let client: LanguageClient | undefined;

/**
 * Starts the shared Visual X# analyzer for file-backed `.vxs` documents.
 *
 * @remarks
 * This extension is a thin host. The Haskell process owns language facts;
 * this client only resolves a configurable executable and connects VS Code's
 * standard LSP transport. It never reimplements parsing or diagnostics.
 *
 * @param context - VS Code's lifetime and disposable registry.
 */
export async function activate(context: vscode.ExtensionContext): Promise<void> {
  const config = vscode.workspace.getConfiguration("visualXsharp.analyzer");
  const command = config.get<string>("path", "visual-analyzer").trim();
  if (command.length === 0) {
    void vscode.window.showErrorMessage(
      "Visual X# Analyzer path is empty. Set visualXsharp.analyzer.path.",
    );
    return;
  }

  const serverOptions: ServerOptions = { command, args: [] };
  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: "file", language: "visual-xsharp" }],
    outputChannelName: "Visual X# Analyzer",
  };

  client = new LanguageClient(
    "visual-xsharp-analyzer",
    "Visual X# Analyzer",
    serverOptions,
    clientOptions,
  );
  context.subscriptions.push(client);

  try {
    await client.start();
  } catch (error) {
    client = undefined;
    const detail = error instanceof Error ? error.message : String(error);
    void vscode.window.showErrorMessage(
      `Could not start visual-analyzer (${command}): ${detail}`,
    );
  }
}

/**
 * Stops the LSP client when VS Code deactivates this extension.
 *
 * @remarks
 * Keeping shutdown in the client lifecycle lets the server receive standard
 * LSP shutdown/exit messages instead of being abandoned as a child process.
 */
export async function deactivate(): Promise<void> {
  const runningClient = client;
  client = undefined;
  if (runningClient) {
    await runningClient.stop();
  }
}
