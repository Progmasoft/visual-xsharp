// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// A small command-line todo database that persists tasks to a text file.



import stdio, fs, process;

enum data Command {
    Add: Str,
    Done: Int,
    List,
    Help,
}

data TodoItem {
    id: Int;
    title: Str;
    done: Bool;
}

class TodoStore {
    path: Str;
    next_id: Int;
    items: ArrayList<TodoItem>;

    TodoStore(path: Str) {
        self.path = path;
        self.next_id = 1;
        self.items = [];
    }

    static fn open(path: Str) -> Result<TodoStore, Error> {
        store: TodoStore = new TodoStore(path);
        store.load()@;
        return Ok(store);
    }

    fn add(title: Str) -> Result<()> {
        item: TodoItem = TodoItem {
            id: self.next_id,
            title: title,
            done: false,
        };

        self.next_id += 1;
        self.items.append(item);
        self.save()@;
        return Ok();
    }

    fn mark_done(id: Int) -> Result<()> {
        for (index: Int = 0; index < self.items.count; index = index + 1) {
            if (self.items[index].id == id) {
                self.items[index].done = true;
                self.save()@;
                return Ok();
            }
        }

        return Error(new Error("invalid todo id"));
    }

    fn print() -> Result<()> {
        if (self.items.count == 0) {
            println!("No tasks yet.");
            return Ok();
        }

        for (item: TodoItem in self.items) {
            status: &Str = if (item.done) {
                "x"
            }
            else {
                " "
            };
            println!("[{}] #{} {}", status, item.id, item.title);
        }
        return Ok();
    }

    fn load() -> Result<()> {
        if (!std::fs::exists(self.path)) {
            return Ok();
        }

        content: Str = std::fs::read_to_str(self.path);

        for (line: Str in content.lines()) {
            if (line.length() == 0) {
                continue;
            }

            item: TodoItem = TodoCodec::parse(line)@;
            self.items.append(item);

            if (item.id >= self.next_id) {
                self.next_id = item.id + 1;
            }
        }
        return Ok();
    }

    fn save() -> Result<()> {
        if (!std::fs::exists(self.path)) {
            std::fs::create_file(self.path);
        }

        opened: std::fs::File = new std::fs::OpenOptions()
            .truncate(true)
            .open(self.path);

        for (item: TodoItem in self.items) {
            std::fs::write(opened, format!("{}\n", TodoCodec::format(item)));
        }
        return Ok();
    }
}

class TodoCodec {
    static fn parse(line: Str) -> Result<TodoItem, Error> {
        parts: ArrayList<Str> = line.split("|");
        if (parts.count != 3) {
            return Error(new Error("invalid todo record"));
        }

        return Ok(TodoItem {
            id: Int::parse(parts[0]),
            title: parts[2],
            done: parts[1] == "done",
        });
    }

    static fn format(item: TodoItem) -> Str {
        state: &Str = if (item.done) {
            "done"
        }
        else {
            "open"
        };
        return item.id.to_string() + "|" + state + "|" + item.title;
    }
}

fn parse_command(args: ArrayList<Str>) -> Result<Command, Error> {
    if (args.count < 2) {
        return Ok(Command::Help);
    }

    return match (args[1]) {
        "add" -> {
            if (args.count < 3) {
                return Error(new Error("todo add <title>"));
            }
            Ok(Command::Add(args.slice(2).join(" ")));
        },
        "done" -> {
            if (args.count != 3) {
                return Error(new Error("todo done <id>"));
            }
            Ok(Command::Done(Int::parse(args[2])));
        },
        "list" -> {
            Ok(Command::List);
        },
        "help" -> {
            Ok(Command::Help);
        },
        else -> {
            Error(new Error("invalid command"));
        },
    };
}

fn print_help() -> Result<()> {
    println!("todo add <title>");
    println!("todo done <id>");
    println!("todo list");
    return Ok();
}

fn main(args: ArrayList<Str>) -> Result<Int, Error> {
    store: TodoStore = TodoStore::open("todo.db")@;
    command: Command = parse_command(args)@;

    match (command) {
        Command::Add(title) -> {
            store.add(title)@;
        },
        Command::Done(id) -> {
            store.mark_done(id)@;
        },
        Command::List -> {
            store.print()@;
        },
        Command::Help -> {
            print_help()@;
        },
    }

    return Ok(0);
}
