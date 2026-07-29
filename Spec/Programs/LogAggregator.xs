// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Reads an application log and prints level counts plus the newest error line.



import fs, stdio, process;

data LogEntry {
    level: String;
    message: String;
}

class LogParser {
    static fn parse(line: &Str) -> Result<LogEntry, Error> {
        parts: ArrayList<&Str> = line.split(" ", 2);
        if (parts.count != 2) {
            return Error(new Error("invalid log line"));
        }

        return Ok(LogEntry {
            level: new String(parts[0].trim()),
            message: new String(parts[1].trim()),
        });
    }
}

class Report {
    counts: [String: Optional<Int>];
    newest_error: Optional<String>;

    Report() {
        self.counts = [];
        self.newest_error = None;
    }

    fn add(entry: LogEntry) {
        current: Int = self.counts[entry.level] ?? 0;
        self.counts[entry.level] = Some(current + 1);

        if (entry.level == "ERROR") {
            self.newest_error = Some(entry.message);
        }
    }

    fn print() -> Result<()> {
        for ((level, count): (String, Optional<Int>) in self.counts) {
            println!("{:<8} {}", level, count!);
        }

        if (self.newest_error != None) {
            println!("newest error: {}", self.newest_error!);
        }

        return Ok();
    }
}

fn main(args: std::process::Args) -> Result<Int, Error> {
    if (args.length() != 2) {
        eprintln!("usage: log-aggregator <log-file>");
        return 2;
    }

    report: Report = new Report();
    content: String = std::fs::read_to_str(&args[1]);

    for (line: &Str in content.lines()) {
        if (line.trim().length() == 0) {
            continue;
        }

        report.add(LogParser::parse(line)@);
    }

    report.print()@;
    return Ok(0);
}
