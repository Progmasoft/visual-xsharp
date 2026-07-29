// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Aggregates asynchronous sensor readings with Optional values and cancellation.



import hardware, thread, sync, stdio;

data Reading {
    sensor_id: String;
    value: Float;
    unit: String;
}

data Average {
    sensor_id: String;
    count: Int;
    value: Float;
}

class Sensor {
    id: String;
    unit: String;

    Sensor(id: &Str, unit: &Str) {
        self.id = new String(id);
        self.unit = new String(unit);
    }

    async fn read() -> Task<Result<Optional<Reading>, Error>> {
        sample: Optional<Float> = await hardware::read_float(&self.id);

        return Ok(sample?.map(fn(value) {
            Reading {
                sensor_id: self.id,
                value: value,
                unit: self.unit,
            }
        }));
    }
}

class Aggregator {
    totals: [String: Optional<Float>];
    counts: [String: Optional<Int>];

    Aggregator() {
        self.totals = [];
        self.counts = [];
    }

    fn add(reading: Reading) {
        self.totals[reading.sensor_id] = Some((self.totals[reading.sensor_id] ?? 0.0) + reading.value);
        self.counts[reading.sensor_id] = Some((self.counts[reading.sensor_id] ?? 0) + 1);
    }

    fn averages() -> ArrayList<Average> {
        result: ArrayList<Average> = [];

        for ((sensor_id, total): (String, Optional<Float>) in self.totals) {
            count: Int = self.counts[sensor_id]!;
            result.append(Average {
                sensor_id: sensor_id,
                count: count,
                value: total! / Float::from(count),
            });
        }

        return result;
    }
}

async fn main() -> Task<Result<Int, Error>> {
    sensors: ArrayList<Sensor> = [
        new Sensor("temperature", "C"),
        new Sensor("humidity", "%"),
        new Sensor("pressure", "Pa")
    ];
    aggregator: Aggregator = new Aggregator();
    cancellation: std::sync::CancellationToken = std::sync::CancellationToken::timeout(5.seconds());

    while (!cancellation.is_cancelled()) {
        for (sensor: Sensor in sensors) {
            reading: Optional<Reading> = await sensor.read()@;
            if (reading != None) {
                aggregator.add(reading!);
            }
        }
    }

    for (average: Average in aggregator.averages()) {
        println!("{} average={}", average.sensor_id, average.value);
    }

    return Ok(0);
}
