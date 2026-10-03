#!/usr/bin/env python3
"""
Simple HTTP server for GENOVA dashboard that injects the classification value
and the cluster percentage into the HTML page.
"""
import os
import sys
from http.server import HTTPServer, SimpleHTTPRequestHandler
import urllib.parse


class GenovaRequestHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        # Parse the URL to get query parameters
        parsed_path = urllib.parse.urlparse(self.path)
        path = parsed_path.path

        # If requesting the root, serve index.html with values injected
        if path == '/' or path == '/index.html':
            self.serve_genova_index()
        else:
            # For all other files, serve them normally
            super().do_GET()

    def serve_genova_index(self):
        """Serve the index.html file with classification and cluster percent injected."""
        try:
            # Read values from environment variables (set by the Bash script)
            classification = os.environ.get('GENOVA_CLASSIFICATION', 'Healthy')
            cluster_percent = os.environ.get('GENOVA_CLUSTER_PERCENT', '0')

            # Read the HTML template
            with open('index.html', 'r', encoding='utf-8') as f:
                html_content = f.read()

            # Replace the placeholders with the actual values
            html_content = html_content.replace('{{CLASSIFICATION}}', classification)
            html_content = html_content.replace('{{CLUSTER_PERCENT}}', cluster_percent)

            # Send the response
            self.send_response(200)
            self.send_header('Content-type', 'text/html; charset=utf-8')
            self.send_header('Cache-Control', 'no-store')
            self.end_headers()
            self.wfile.write(html_content.encode('utf-8'))
        except FileNotFoundError:
            self.send_error(404, "File not found: index.html")
        except Exception as e:
            self.send_error(500, f"Internal server error: {str(e)}")


def run_server(port=8080):
    """Run the HTTP server on 0.0.0.0:port."""
    server_address = ('0.0.0.0', port)
    httpd = HTTPServer(server_address, GenovaRequestHandler)
    print(f"Serving on http://0.0.0.0:{port}")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped.")
        httpd.server_close()


if __name__ == '__main__':
    run_server()
