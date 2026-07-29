/*
 * SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
 * SPDX-License-Identifier: MPL-2.0
 */

use criterion::{Criterion, criterion_group, criterion_main};
use xstidy::lint_source;

fn benchmark_lint_source(criterion: &mut Criterion)
{
  let source = "fn main() { value := Type::new(); value := new Type(); }\n".repeat(256);
  criterion.bench_function("lint 256 functions", |bencher| bencher.iter(|| lint_source(&source)));
}

criterion_group!(benches, benchmark_lint_source);
criterion_main!(benches);
