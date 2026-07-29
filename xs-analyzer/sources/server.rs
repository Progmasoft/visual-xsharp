/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use std::error::Error;

use lsp_server::{Connection, Message};
use lsp_types::{
    PositionEncodingKind, ServerCapabilities, TextDocumentSyncCapability, TextDocumentSyncKind,
};
use serde::{Deserialize, Serialize};

type ServerResult<T> = Result<T, Box<dyn Error + Send + Sync>>;

#[derive(Debug, Default, Deserialize, Serialize)]
#[serde(default)]
struct XsAnalyzerConfig {
    trace: bool,
}

pub fn run() -> ServerResult<()> {
    let (connection, io_threads) = Connection::stdio();
    let initialization = serde_json::json!({
      "capabilities": capabilities(),
      "serverInfo": {
        "name": "xs-analyzer",
        "version": env!("CARGO_PKG_VERSION")
      }
    });
    let parameters = connection.initialize(initialization)?;
    let _configuration = read_configuration(&parameters);

    for message in &connection.receiver {
        if let Message::Request(request) = message
            && connection.handle_shutdown(&request)?
        {
            break;
        }
    }
    io_threads.join()?;
    Ok(())
}

fn capabilities() -> ServerCapabilities {
    ServerCapabilities {
        position_encoding: Some(PositionEncodingKind::UTF16),
        text_document_sync: Some(TextDocumentSyncCapability::Kind(TextDocumentSyncKind::FULL)),
        ..ServerCapabilities::default()
    }
}

fn read_configuration(parameters: &serde_json::Value) -> XsAnalyzerConfig {
    parameters
        .get("initializationOptions")
        .cloned()
        .and_then(|value| serde_json::from_value(value).ok())
        .unwrap_or_default()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn advertises_full_utf16_document_sync() {
        let capabilities = capabilities();
        assert_eq!(
            capabilities.position_encoding,
            Some(PositionEncodingKind::UTF16)
        );
        assert_eq!(
            capabilities.text_document_sync,
            Some(TextDocumentSyncCapability::Kind(TextDocumentSyncKind::FULL))
        );
    }

    #[test]
    fn reads_optional_initialization_configuration() {
        let parameters = serde_json::json!({ "initializationOptions": { "trace": true } });
        assert!(read_configuration(&parameters).trace);
    }
}
