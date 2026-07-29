// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Resolves package dependencies with a topological order and cycle diagnostics.



import stdio, process;

enum VisitState {
    White,
    Gray,
    Black,
}

data Package {
    name: String;
    version: String;
    dependencies: ArrayList<String>;
}

class PackageGraph {
    packages: [String: Package];
    states: [String: VisitState];
    ordered: ArrayList<Package>;

    PackageGraph() {
        self.packages = [];
        self.states = [];
        self.ordered = [];
    }

    fn add(package: Package) {
        self.packages[package.name] = package;
        self.states[package.name] = VisitState::White;
    }

    fn resolve(root: &Str) -> Result<ArrayList<Package>, Error> {
        self.visit(root)@;
        return Ok(self.ordered);
    }

    fn visit(name: &Str) -> Result<()> {
        if (!self.packages.contains(name)) {
            return Error(new Error("unknown package"));
        }

        state: VisitState = self.states[name];
        match (state) {
            VisitState::Black -> {
                return;
            },
            VisitState::Gray -> {
                return Error(new Error("dependency cycle"));
            },
            VisitState::White -> {
            },
        }

        self.states[name] = VisitState::Gray;
        package: Package = self.packages[name];

        for (dependency: String in package.dependencies) {
            self.visit(&dependency)@;
        }

        self.states[name] = VisitState::Black;
        self.ordered.append(package);
        return Ok();
    }
}

fn package_of(name: &Str, version: &Str, dependencies: ArrayList<&Str>) -> Package {
    return Package {
        name: new String(name),
        version: new String(version),
        dependencies: dependencies.map(fn(dependency) { new String(dependency) }),
    };
}

fn main() -> Result<Int, Error> {
    graph: PackageGraph = new PackageGraph();

    graph.add(package_of("app", "1.0.0", ["net", "json"]));
    graph.add(package_of("net", "2.1.0", ["runtime"]));
    graph.add(package_of("json", "3.0.0", ["runtime"]));
    graph.add(package_of("runtime", "1.4.0", []));

    for (package: Package in graph.resolve("app")@) {
        println!("{}@{}", package.name, package.version);
    }

    return Ok(0);
}
