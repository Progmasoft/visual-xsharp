// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Runs a configurable text-processing pipeline through nominal plugin types.



import stdio, process;

interface TextPlugin {
    fn name() -> &Str;
    fn run(input: Str) -> Result<Str, Error>;
}

class TrimPlugin : TextPlugin {

    fn name() -> &Str {
        return "trim";
    }

    fn run(input: Str) -> Result<Str, Error> {
        return Ok(input.trim());
    }
}

class UppercasePlugin : TextPlugin {

    fn name() -> &Str {
        return "upper";
    }

    fn run(input: Str) -> Result<Str, Error> {
        return Ok(input.uppercase());
    }
}

class ReplacePlugin : TextPlugin {

    from_text: Str;
    to_text: Str;

    ReplacePlugin(from_text: Str, to_text: Str) {
        self.from_text = from_text;
        self.to_text = to_text;
    }

    fn name() -> &Str {
        return "replace";
    }

    fn run(input: Str) -> Result<Str, Error> {
        return Ok(input.replace(self.from_text, self.to_text));
    }
}

class Pipeline {
    stages: ArrayList<TextPlugin>;

    Pipeline() {
        self.stages = [];
    }

    fn add(stage: TextPlugin) {
        self.stages.append(stage);
    }

    fn run(input: Str) -> Result<Str, Error> {
        output: Str = input;

        for (stage: TextPlugin in self.stages) {
            output = stage.run(output)@;
        }

        return Ok(output);
    }
}

fn main(args: ArrayList<Str>) -> Result<Int, Error> {
    input: &Str = if (args.count > 1) {
        args[1]
    }
    else {
        "  hello x#  "
    };
    pipeline: Pipeline = new Pipeline();

    pipeline.add(new TrimPlugin());
    pipeline.add(new ReplacePlugin("x#", "X#"));
    pipeline.add(new UppercasePlugin());

    println!("{}", pipeline.run(input)@);
    return Ok(0);
}
