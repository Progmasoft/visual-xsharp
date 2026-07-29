// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Process module and error handling:

//
// Process execution is provided by std::process::
//
// X# uses Result<T, Error> for recoverable process and I/O failures.
//
// Raw shell execution is disabled by default. Shell execution must be
// explicitly enabled with .shell(true).
//

import process, result;


// process execution

fn pull_repository() -> Result<()> {
    std::process::execute("git", ["pull"])@;
    return Ok();
}


// process execution with arguments

fn install_package(name: Str) -> Result<()> {
    std::process::execute("pacman", ["-S", name])@;
    return Ok();
}


// shell execution

fn upgrade_system() -> Result<()> {
    std::process::execute("pacman -Syu")
        .shell(true)@;

    return Ok();
}


// INVALID

fn invalid_shell_execution() -> Result<()> {
    std::process::execute("pacman -Syu")@;
    return Ok();
}

// shell execution requires .shell(true)


// explicit Result match

fn print_git_status() -> Result<()> {
    status = match (std::process::execute("git", ["status"])) {
        Ok(value) -> value,
        Error(error) -> return Error(error),
    };

    println!("{:?}", status);
    return Ok();
}


// propagation with @

fn run_formatter() -> Result<()> {
    std::process::execute("xsfmt", ["--check"])@;
    std::process::execute("xstidy", ["--check"])@;
    return Ok();
}


// custom error model

fn login(user: Str) -> Result<()> {
    if (user.length() == 0) {
        return Error(new Error("invalid user"));
    }

    if (user == "root") {
        return Error(new Error("locked user"));
    }

    return Ok();
}


// built-in error-style values

fn read_required_line() -> Result<Str, Error> {
    input: Optional<Str> = Some("");

    std::stdin()
        .read_line(&mut input)@;

    if (input == None) {
        return Error(new Error("no input"));
    }

    return Ok(input!);
}


// deterministic cleanup still applies

class File {
    File::Drop() {
        println!("File closed");
    }
}

fn use_file() -> Result<()> {
    file: File = new File();
    std::process::execute("git", ["rev-parse", "--is-inside-work-tree"])@;
    return Ok();
}

// File::Drop() runs before returning Error from @ propagation.
