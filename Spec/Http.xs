// SPDX-FileCopyrightText: 2026 Leitwolf <support@xsharp-lang.xyz>
// SPDX-License-Identifier: MPL-2.0

// Http standard library module:

//
// The Http module provides synchronous and asynchronous HTTP requests.
//
// Confirmed features:
//
// - std::http::Client
// - std::http::Request
// - std::http::Response<T>
// - std::http::Uri::create(...)
// - Request builder methods
// - Str request bodies
// - Str response body handlers
// - Synchronous send(...)
// - Asynchronous send_async(...)
// - Error payloads through Result
//
// No additional HTTP API is defined in this file.
//

import http;


// ============================================================
// Synchronous request
// ============================================================

fn main() -> Result<()> {
    client: std::http::Client = new std::http::Client();

    request: std::http::Request = std::http::Request::builder()
        .uri(std::http::Uri::create("https://httpbin.org/post"))
        .header("Content-Type", "application/json")
        .post(
            std::http::BodyPublishers::of_str(
                """
                {
                    "name": "Alpha"
                }
                """
            )
        )
        .build();

    response: std::http::Response<String> =
        client.send(
            request,
            std::http::BodyHandlers::of_str()
        )@;

    print!("{}", response.body());
    return Ok();
}


// ============================================================
// Request builder
// ============================================================

fn build_get_request() {
    request: std::http::Request = std::http::Request::builder()
        .uri(std::http::Uri::create("https://example.com"))
        .build();
}


// Request construction uses a builder chain.
//
// Confirmed builder operations:
//
// - uri(...)
// - header(...)
// - POST(...)
// - build()


// ============================================================
// Str request body
// ============================================================

fn str_request_body() {
    request: std::http::Request = std::http::Request::builder()
        .uri(std::http::Uri::create("https://httpbin.org/post"))
        .header("Content-Type", "application/json")
        .post(
            std::http::BodyPublishers::of_str(
                """
                {
                    "name": "Alpha"
                }
                """
            )
        )
        .build();
}


// std::http::BodyPublishers::of_str(...):
//
// - Creates a request body from Str.
// - May receive a multiline string.


// ============================================================
// Synchronous send
// ============================================================

fn send_request() -> Result<()> {
    client: std::http::Client = new std::http::Client();

    request: std::http::Request = std::http::Request::builder()
        .uri(std::http::Uri::create("https://example.com"))
        .build();

    response: std::http::Response<String> =
        client.send(
            request,
            std::http::BodyHandlers::of_str()
        )@;

    body: String = response.body();
    return Ok();
}


// client.send(...):
//
// - Sends the request synchronously.
// - Blocks until a response is available.
// - Returns std::http::Response<String> when used with of_str().
// - Returns the standard Error payload on network failure.


// ============================================================
// Asynchronous send
// ============================================================

async fn send_request_async() -> Task<()> {
    client: std::http::Client = new std::http::Client();

    request: std::http::Request = std::http::Request::builder()
        .uri(std::http::Uri::create("https://example.com"))
        .build();

    response: std::http::Response<String> =
        await client.send_async(
            request,
            std::http::BodyHandlers::of_str()
        );

    print!("{}", response.body());
}


// client.send_async(...):
//
// - Sends the request asynchronously.
// - Returns Task<std::http::Response<String>> when used with of_str().
// - Must be awaited according to Task<T> rules.
// - A network failure is represented as the standard Error payload.


// ============================================================
// Response
// ============================================================

fn read_response_body(
    response: std::http::Response<String>
) -> String {
    return response.body();
}


// std::http::Response<T>:
//
// - Represents an HTTP response.
// - body() returns the response body as T.


// ============================================================
// Body handlers
// ============================================================

fn str_body_handler() {
    handler = std::http::BodyHandlers::of_str();
}


// The canonical Str body handler is std::http::BodyHandlers::of_str().


// ============================================================
// Result error handling
// ============================================================

fn handle_network_error() -> Result<()> {
    client: std::http::Client = new std::http::Client();

    request: std::http::Request = std::http::Request::builder()
        .uri(std::http::Uri::create("https://example.com"))
        .build();

    result: Result<std::http::Response<String>, Error> =
        client.send(
            request,
            std::http::BodyHandlers::of_str()
        );

    match (result) {
        Ok(response) -> {
            println!("{}", response.body());
        },
        Error(error) -> {
            eprintln!("Network request failed");
            return Error(error);
        },
    }

    return Ok();
}


// Network failure:
//
// - Represents HTTP or network request failure.
// - Is carried through the standard Error payload.


// ============================================================
// Confirmed type summary
// ============================================================

//
// std::http::Client
// std::http::Request
// std::http::Response<T>
// Task<std::http::Response<String>>
// Network failure
//
