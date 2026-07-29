// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Scans a content directory and emits a small static-site health report.



import fs, stdio, process;

data PageInfo {
    path: String;
    title: String;
    word_count: Int;
}

class Markdown {
    static fn title(path: &Str, text: &Str) -> Result<String, Error> {
        for (line: &Str in text.lines()) {
            if (line.starts_with("# ")) {
                return Ok(new String(line.trim_start("# ").trim()));
            }
        }

        return Error(new Error("missing page title"));
    }

    static fn count_words(text: &Str) -> Int {
        return text.split_whitespace().count;
    }
}

class SiteReport {
    pages: ArrayList<PageInfo>;

    SiteReport() {
        self.pages = [];
    }

    fn add_markdown(path: String) -> Result<()> {
        text: String = std::fs::read_to_str(&path);
        title: String = Markdown::title(&path, &text)@;
        word_count: Int = Markdown::count_words(&text);
        self.pages.append(PageInfo {
            path: path,
            title: title,
            word_count: word_count,
        });
        return Ok();
    }

    fn print() -> Result<()> {
        println!("pages: {}", self.pages.count);

        for (page: PageInfo in self.pages) {
            println!("{:<32} {:>6} {}", page.title, page.word_count, page.path);
        }
        return Ok();
    }
}

fn main(args: std::process::Args) -> Result<Int, Error> {
    root: &Str = if (args.length() == 2) {
        &args[1]
    }
    else {
        "."
    };

    report: SiteReport = new SiteReport();
    for (path: String in std::fs::walk(root)) {
        if (path.ends_with(".md")) {
            report.add_markdown(path)@;
        }
    }

    report.print()@;
    return Ok(0);
}
