/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

// @ts-check

// @ts-ignore
import json from "@eslint/json";
// @ts-ignore
import { defineConfig } from "eslint/config";
// @ts-ignore
import tseslint from "typescript-eslint";

const javaScriptFiles = ["**/*.js"];
const typeScriptFiles = ["**/*.ts", "**/*.d.ts"];
const sourceFiles = [...javaScriptFiles, ...typeScriptFiles];

const jsonFiles = ["**/*.json", "**/*.geojson", "**/*.webmanifest", "**/*.json.example"];
const jsoncFiles = [
  "**/*.jsonc",
  "**/*.code-workspace",
  "**/*.code-snippets",
  "**/.vscode/**/*.json",
  "**/tsconfig*.json",
];
const json5Files = ["**/*.json5"];
const lintedFiles = [...sourceFiles, ...jsonFiles, ...jsoncFiles, ...json5Files];

const commonGlobals = {
  AbortController: "readonly",
  Blob: "readonly",
  console: "readonly",
  crypto: "readonly",
  document: "readonly",
  fetch: "readonly",
  FormData: "readonly",
  globalThis: "readonly",
  Headers: "readonly",
  localStorage: "readonly",
  navigator: "readonly",
  performance: "readonly",
  process: "readonly",
  queueMicrotask: "readonly",
  Request: "readonly",
  Response: "readonly",
  sessionStorage: "readonly",
  setInterval: "readonly",
  setTimeout: "readonly",
  structuredClone: "readonly",
  URL: "readonly",
  URLSearchParams: "readonly",
  window: "readonly",
};

const typeScriptConfigs = [...tseslint.configs.strict, ...tseslint.configs.stylistic].map((config) => ({
  ...config,
  files: typeScriptFiles,
}));

export default defineConfig([
  {
    name: "project/ignores",
    ignores: [
      "**/build/**",
      "**/coverage/**",
      "**/dist/**",
      "**/out/**",
      "**/node_modules/**",
      "**/.cache/**",
      "**/.pnpm-store/**",
      "**/vendor/**",
      "**/third_party/**",
      "**/external/**",
      "**/*.min.js",
      "**/package-lock.json",
    ],
  },
  {
    name: "project/linter-options",
    files: lintedFiles,
    linterOptions: {
      noInlineConfig: false,
      reportUnusedDisableDirectives: "error",
      reportUnusedInlineConfigs: "error",
    },
  },
  {
    name: "project/javascript",
    files: javaScriptFiles,

    languageOptions: {
      ecmaVersion: "latest",
      sourceType: "module",
      globals: commonGlobals,
    },

    rules: {
      "array-callback-return": ["error", { allowImplicit: false, checkForEach: true }],
      "constructor-super": "error",
      "default-case-last": "error",
      "default-param-last": "error",
      eqeqeq: ["error", "always", { null: "ignore" }],
      "for-direction": "error",
      "getter-return": ["error", { allowImplicit: false }],
      "logical-assignment-operators": ["warn", "always", { enforceForIfStatements: true }],
      "no-array-constructor": "error",
      "no-async-promise-executor": "error",
      "no-case-declarations": "error",
      "no-class-assign": "error",
      "no-compare-neg-zero": "error",
      "no-cond-assign": ["error", "except-parens"],
      "no-const-assign": "error",
      "no-constant-binary-expression": "error",
      "no-constant-condition": ["error", { checkLoops: "allExceptWhileTrue" }],
      "no-constructor-return": "error",
      "no-control-regex": "error",
      "no-debugger": "warn",
      "no-delete-var": "error",
      "no-dupe-args": "error",
      "no-dupe-class-members": "error",
      "no-dupe-else-if": "error",
      "no-dupe-keys": "error",
      "no-duplicate-case": "error",
      "no-duplicate-imports": ["error", { allowSeparateTypeImports: true }],
      "no-empty": ["error", { allowEmptyCatch: true }],
      "no-empty-character-class": "error",
      "no-empty-pattern": "error",
      "no-empty-static-block": "error",
      "no-eval": "error",
      "no-ex-assign": "error",
      "no-extra-boolean-cast": "error",
      "no-fallthrough": ["error", { allowEmptyCase: true, reportUnusedFallthroughComment: true }],
      "no-func-assign": "error",
      "no-global-assign": "error",
      "no-implied-eval": "error",
      "no-import-assign": "error",
      "no-inner-declarations": "error",
      "no-invalid-regexp": "error",
      "no-irregular-whitespace": "error",
      "no-loss-of-precision": "error",
      "no-misleading-character-class": "error",
      "no-new-native-nonconstructor": "error",
      "no-new-wrappers": "error",
      "no-nonoctal-decimal-escape": "error",
      "no-obj-calls": "error",
      "no-object-constructor": "error",
      "no-octal": "error",
      "no-promise-executor-return": "error",
      "no-proto": "error",
      "no-prototype-builtins": "error",
      "no-redeclare": ["error", { builtinGlobals: true }],
      "no-regex-spaces": "error",
      "no-return-assign": ["error", "always"],
      "no-self-assign": ["error", { props: true }],
      "no-self-compare": "error",
      "no-sequences": ["error", { allowInParentheses: true }],
      "no-setter-return": "error",
      "no-shadow-restricted-names": ["error", { reportGlobalThis: true }],
      "no-sparse-arrays": "error",
      "no-template-curly-in-string": "warn",
      "no-this-before-super": "error",
      "no-unassigned-vars": "error",
      "no-undef": ["error", { typeof: true }],
      "no-unexpected-multiline": "error",
      "no-unmodified-loop-condition": "warn",
      "no-unreachable": "error",
      "no-unreachable-loop": "error",
      "no-unsafe-finally": "error",
      "no-unsafe-negation": ["error", { enforceForOrderingRelations: true }],
      "no-unsafe-optional-chaining": ["error", { disallowArithmeticOperators: true }],
      "no-unused-labels": "error",
      "no-unused-private-class-members": "error",
      "no-unused-vars": [
        "warn",
        {
          args: "after-used",
          argsIgnorePattern: "^_",
          caughtErrors: "all",
          caughtErrorsIgnorePattern: "^_",
          destructuredArrayIgnorePattern: "^_",
          ignoreRestSiblings: true,
          reportUsedIgnorePattern: true,
          varsIgnorePattern: "^_",
        },
      ],
      "no-useless-assignment": "error",
      "no-useless-backreference": "error",
      "no-useless-catch": "error",
      "no-useless-escape": "error",
      "no-var": "error",
      "no-with": "error",
      "object-shorthand": ["warn", "always"],
      "prefer-const": ["error", { destructuring: "all" }],
      "prefer-object-has-own": "error",
      "prefer-promise-reject-errors": "error",
      "prefer-rest-params": "error",
      "prefer-spread": "error",
      "prefer-template": "warn",
      "preserve-caught-error": "error",
      radix: "error",
      "require-atomic-updates": "warn",
      "require-yield": "error",
      "symbol-description": "error",
      "use-isnan": ["error", { enforceForIndexOf: true, enforceForSwitchCase: true }],
      "valid-typeof": ["error", { requireStringLiterals: true }],
      yoda: ["error", "never", { exceptRange: true }],
    },
  },
  {
    name: "project/json",
    files: jsonFiles,
    plugins: { json },
    language: "json/json",
    extends: ["json/recommended"],
  },
  {
    name: "project/jsonc",
    files: jsoncFiles,
    plugins: { json },
    language: "json/jsonc",
    languageOptions: {
      allowTrailingCommas: true,
    },
    extends: ["json/recommended"],
  },
  {
    name: "project/json5",
    files: json5Files,
    plugins: { json },
    language: "json/json5",
    extends: ["json/recommended"],
  },
  ...typeScriptConfigs,
  {
    name: "project/typescript",
    files: typeScriptFiles,

    languageOptions: {
      globals: commonGlobals,
      parserOptions: {
        ecmaFeatures: {
          jsx: false,
        },
      },
    },

    rules: {
      "@typescript-eslint/consistent-type-imports": [
        "warn",
        {
          disallowTypeAnnotations: false,
          fixStyle: "inline-type-imports",
          prefer: "type-imports",
        },
      ],
      "@typescript-eslint/default-param-last": "error",
      "@typescript-eslint/method-signature-style": ["error", "property"],
      "@typescript-eslint/no-import-type-side-effects": "error",
      "@typescript-eslint/no-require-imports": "error",
      "@typescript-eslint/no-unused-vars": [
        "warn",
        {
          args: "after-used",
          argsIgnorePattern: "^_",
          caughtErrors: "all",
          caughtErrorsIgnorePattern: "^_",
          destructuredArrayIgnorePattern: "^_",
          ignoreRestSiblings: true,
          reportUsedIgnorePattern: true,
          varsIgnorePattern: "^_",
        },
      ],
      "@typescript-eslint/prefer-enum-initializers": "warn",
      "@typescript-eslint/prefer-for-of": "warn",
      "@typescript-eslint/prefer-function-type": "error",
      "@typescript-eslint/unified-signatures": "error",
      "default-param-last": "off",
      "no-undef": "off",
      "no-unused-vars": "off",
    },
  },
  {
    name: "project/declaration-files",
    files: ["**/*.d.ts"],
    rules: {
      "@typescript-eslint/no-explicit-any": "off",
      "@typescript-eslint/no-empty-object-type": "off",
      "@typescript-eslint/no-unused-vars": "off",
    },
  },
]);
