// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Recursively copies changed files from one directory into a backup directory.



import stdio, fs, process;

data FileEntry {
    path: Str;
    relative: Str;
    bytes: Int;
    modified_ticks: Int;
}

class BackupPlan {
    source_root: Str;
    target_root: Str;
    files: ArrayList<FileEntry>;

    BackupPlan(source_root: Str, target_root: Str) {
        self.source_root = source_root;
        self.target_root = target_root;
        self.files = [];
    }

    fn discover() -> Result<()> {
        if (!std::fs::is_dir(self.source_root)) {
            return Error(new Error("invalid backup source"));
        }

        for (path: Str in std::fs::walk_dir(self.source_root)) {
            if (std::fs::is_dir(path)) {
                continue;
            }

            relative: Str = std::fs::relative_path(self.source_root, path);
            self.files.append(FileEntry {
                path: path,
                relative: relative,
                bytes: std::fs::size(path),
                modified_ticks: std::fs::modified_ticks(path),
            });
        }
        return Ok();
    }

    fn execute() -> Result<()> {
        for (entry: FileEntry in self.files) {
            destination: Str = std::fs::join_path(self.target_root, entry.relative);

            if (self.should_copy(entry, destination)) {
                std::fs::create_dir(std::fs::parent_dir(destination));
                std::fs::copy_file(entry.path, destination);
                println!("copied {}", entry.relative);
            }
        }
        return Ok();
    }

    fn should_copy(entry: FileEntry, destination: Str) -> Bool {
        if (!std::fs::exists(destination)) {
            return true;
        }

        return std::fs::size(destination) != entry.bytes || std::fs::modified_ticks(destination) < entry.modified_ticks;
    }
}

fn main(args: ArrayList<Str>) -> Result<Int, Error> {
    if (args.count != 3) {
        eprintln!("usage: backup <source> <target>");
        return 2;
    }

    plan: BackupPlan = new BackupPlan(args[1], args[2]);
    plan.discover()@;
    plan.execute()@;
    return Ok(0);
}
