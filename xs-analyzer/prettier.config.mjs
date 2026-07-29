/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

// @ts-nocheck

/** @type {import("prettier").Config} */
const config = {
  printWidth: 120,
  tabWidth: 2,
  useTabs: false,

  semi: true,
  singleQuote: false,
  quoteProps: "as-needed",

  trailingComma: "all",
  bracketSpacing: true,
  arrowParens: "always",
  experimentalOperatorPosition: "end",
  experimentalTernaries: false,

  objectWrap: "preserve",
  embeddedLanguageFormatting: "auto",
  checkIgnorePragma: true,
  endOfLine: "lf",

  overrides: [
    {
      files: [
        "**/*.json",
        "**/*.jsonc",
        "**/*.json5",
        "**/*.geojson",
        "**/*.webmanifest",
        "**/*.json.example",
        "**/*.code-workspace",
        "**/*.code-snippets",
        "**/tsconfig*.json",
      ],
      options: {
        tabWidth: 2,
        trailingComma: "none",
        quoteProps: "preserve",
      },
    },
  ],
};

export default config;
