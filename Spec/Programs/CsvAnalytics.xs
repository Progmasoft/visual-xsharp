// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Reads CSV sales records, groups them by region, and prints totals.



import stdio, fs, process;

data Sale {
    region: Str;
    product: Str;
    quantity: Int;
    revenue: Float;
}

data RegionTotal {
    region: Str;
    quantity: Int;
    revenue: Float;
}

class CsvParser {
    static fn parse_line(line: Str) -> Result<Sale, Error> {
        fields: ArrayList<Str> = line.split(",");
        if (fields.count != 4) {
            return Error(new Error("bad CSV row"));
        }

        return Ok(Sale {
            region: fields[0],
            product: fields[1],
            quantity: Int::parse(fields[2]),
            revenue: Float::parse(fields[3]),
        });
    }
}

class Analytics {
    totals: [Str: RegionTotal];

    Analytics() {
        self.totals = [];
    }

    fn add(sale: Sale) {
        if (!self.totals.contains(sale.region)) {
            self.totals[sale.region] = RegionTotal {
                region: sale.region,
                quantity: 0,
                revenue: 0.0,
            };
        }

        self.totals[sale.region].quantity += sale.quantity;
        self.totals[sale.region].revenue += sale.revenue;
    }

    fn print() -> Result<()> {
        for ((else, total): (Str, RegionTotal) in self.totals) {
            println!(
                "{}: units={} revenue={}",
                total.region,
                total.quantity,
                total.revenue
            );
        }
        return Ok();
    }
}

fn load_sales(path: Str) -> Result<ArrayList<Sale>, Error> {
    rows: ArrayList<Sale> = [];
    content: Str = std::fs::read_to_str(path);

    for (line: Str in content.lines().skip(1)) {
        if (line.length() == 0) {
            continue;
        }
        rows.append(CsvParser::parse_line(line)@);
    }

    return Ok(rows);
}

fn main(args: ArrayList<Str>) -> Result<Int, Error> {
    path: &Str = if (args.count > 1) {
        args[1]
    }
    else {
        "sales.csv"
    };
    analytics: Analytics = new Analytics();

    for (sale: Sale in load_sales(path)@) {
        analytics.add(sale);
    }

    analytics.print()@;
    return Ok(0);
}
