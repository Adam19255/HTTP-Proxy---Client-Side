# CProxy - Simple HTTP Proxy in C

CProxy is a lightweight HTTP proxy implemented in C. It allows users to fetch web resources, save them locally, and optionally open them in a browser.

## Features

- Fetches HTTP resources from specified URLs
- Saves fetched resources to the local filesystem
- Caches resources for faster subsequent access
- Supports opening fetched resources in a browser (Linux systems)
- Handles various HTTP status codes
- Parses URLs to extract host, port, and file path information

## Usage

Compile the program:
gcc -o cproxy cproxy.c

Run the program:
- `<URL>`: The HTTP URL to fetch (must start with "http://")
- `-s`: Optional flag to open the fetched resource in the default browser

## Examples

1. Fetch a resource: ./cproxy http://example.com/page.html
2. Fetch a resource and open it in the browser: ./cproxy http://example.com/page.html -s

## How It Works

1. Parses the provided URL to extract host, port, and file path
2. Checks if the resource is already cached locally
3. If cached, serves the resource from the local filesystem
4. If not cached, sends an HTTP request to the server
5. Receives the HTTP response and saves it to the local filesystem
6. Displays the HTTP headers and content on stdout
7. Optionally opens the resource in the default browser (Linux systems only)

## Dependencies

- Standard C libraries
- POSIX-compliant operating system (Linux, macOS, etc.)

## Limitations

- Only supports HTTP (not HTTPS)
- Browser opening feature is limited to Linux systems using `xdg-open`
- Basic error handling and limited support for complex HTTP scenarios
