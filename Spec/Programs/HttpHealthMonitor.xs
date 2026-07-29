// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Complete-language example program:
// Concurrently checks HTTP endpoints and prints a compact health report.



import http, thread, sync, stdio;

data Endpoint {
    name: String;
    url: String;
}

data HealthResult {
    endpoint: Endpoint;
    ok: Bool;
    status_code: Int;
    body_preview: String;
}

interface HealthReporter {
    fn report(result: HealthResult) -> Result<()>;
}

class ConsoleReporter : HealthReporter {

    fn report(result: HealthResult) -> Result<()> {
        state: &Str = if (result.ok) {
            "OK"
        }
        else {
            "FAIL"
        };
        println!(
            "{} {} status={} preview=\"{}\"",
            state,
            result.endpoint.name,
            result.status_code,
            result.body_preview
        );
        return Ok();
    }
}

class HealthClient {
    client: std::http::Client;

    HealthClient() {
        self.client = new std::http::Client();
    }

    async fn check(endpoint: Endpoint) -> Task<Result<HealthResult, Error>> {
        request: std::http::Request = std::http::Request::builder()
            .uri(std::http::Uri::create(&endpoint.url))
            .header("Accept", "text/plain")
            .build();

        response: std::http::Response<String> =
            await self.client.send_async(
                request,
                std::http::BodyHandlers::of_str()
            );

        body: String = response.body();
        preview: String = if (body.length() > 80) {
            body.substring(0, 80)
        }
        else {
            body
        };

        return Ok(HealthResult {
            endpoint: endpoint,
            ok: response.status_code() >= 200 && response.status_code() < 300,
            status_code: response.status_code(),
            body_preview: preview,
        });
    }
}

async fn check_all(
    endpoints: ArrayList<Endpoint>,
    reporter: HealthReporter
) -> Task<Result<Int, Error>> {
    client: HealthClient = new HealthClient();
    tasks: ArrayList<Task<Result<HealthResult, Error>>> = [];

    for (endpoint: Endpoint in endpoints) {
        tasks.append(client.check(endpoint));
    }

    failures: Int = 0;
    for (task: Task<HealthResult> in tasks) {
        result: HealthResult = await task@;
        reporter.report(result)@;

        if (!result.ok) {
            failures += 1;
        }
    }

    return Ok(failures);
}

fn default_endpoints() -> ArrayList<Endpoint> {
    endpoints: ArrayList<Endpoint> = [];

    endpoints.append(Endpoint {
        name: new String("example"),
        url: new String("https://example.com"),
    });

    endpoints.append(Endpoint {
        name: new String("httpbin"),
        url: new String("https://httpbin.org/get"),
    });

    endpoints.append(Endpoint {
        name: new String("iana"),
        url: new String("https://www.iana.org/domains/reserved"),
    });

    return endpoints;
}

async fn main() -> Task<Int> {
    reporter: ConsoleReporter = new ConsoleReporter();

    result: Result<Int, Error> = await check_all(default_endpoints(), reporter);
    match (result) {
        Ok(failures) -> {
            if (failures == 0) {
                println!("All endpoints are healthy.");
            } else {
                eprintln!("{} endpoint(s) failed.", failures);
            }
            return failures;
        },
        Error(error) -> {
            eprintln!("Health check failed: {}", error.to_string());
            return 1;
        },
    }
}
