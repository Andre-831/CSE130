
cse130 asgn2 http server. HTTP server written in C, listens on a port and handles client requests using sockets. This server supports PUT and GET methods to read and write to files.

Handles HTTP/1.1 requests
Supports:
GET: returns file contents
PUT: creates or overwrites files
Sends HTTP responses (200, 201, 400, 403, 404, 500, 501, 505)
uses fstat to get file size

made a helper function send_response()
that sends an HTTP response using the given status code, message, and body
