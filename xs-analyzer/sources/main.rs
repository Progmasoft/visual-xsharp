/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

mod server;

fn main() {
    if let Err(error) = server::run() {
        eprintln!("xs-analyzer: {error}");
        std::process::exit(1);
    }
}
