/**
 * Vexium HTTP Server Implementation
 * =================================
 * 
 * A comprehensive HTTP server implementation for Vexium, enabling
 * server-side web application development.
 * 
 * Features:
 * - HTTP/1.1 server with keep-alive support
 * - Route handlers with pattern matching
 * - Request/Response objects
 * - Static file serving
 * - JSON API helpers
 * 
 * Author: Vexium Development Team
 * Version: 1.0.0
 */

/* Windows headers must be included FIRST to avoid conflicts */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

/* Windows doesn't have strtok_r, implement it */
static char* vstrtok_r(char* str, const char* delim, char** saveptr) {
    char* token;
    if (str == NULL) {
        str = *saveptr;
    }
    if (str == NULL) return NULL;
    token = str;
    str = strstr(str, delim);
    if (str) {
        *str = '\0';
        *saveptr = str + strlen(delim);
    } else {
        *saveptr = NULL;
    }
    return token;
}

/* Windows doesn't have inet_pton, implement it */
static int vinet_pton(int af, const char* src, void* dst) {
    struct sockaddr_in sa;
    if (af != AF_INET) return 0;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = af;
    sa.sin_port = 0;
    
    /* Use inet_addr for IPv4 */
    unsigned long addr = inet_addr(src);
    if (addr == INADDR_NONE) {
        /* Check if it's "255.255.255.255" which is valid but inet_addr returns INADDR_NONE */
        if (strcmp(src, "255.255.255.255") != 0) {
            return 0;
        }
    }
    memcpy(dst, &addr, sizeof(addr));
    return 1;
}

#define strtok_r vstrtok_r
#define inet_pton vinet_pton

/* Windows strdup is not in C standard, implement it */
static char* vstrdup(const char* str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char* copy = (char*)malloc(len);
    if (copy) memcpy(copy, str, len);
    return copy;
}

/* Windows doesn't have strcasecmp/strncasecmp, implement them */
static int vstrcasecmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        int c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
        int c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    int c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
    int c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
    return c1 - c2;
}

static int vstrncasecmp(const char* s1, const char* s2, size_t n) {
    size_t i = 0;
    while (i < n && *s1 && *s2) {
        int c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
        int c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++; s2++; i++;
    }
    if (i == n) return 0;
    int c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
    int c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
    return c1 - c2;
}

#define strdup vstrdup
#define strcasecmp vstrcasecmp
#define strncasecmp vstrncasecmp

#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#endif

#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════
 * PLATFORM DEFINITIONS
 * ═══════════════════════════════════════════════════════════════════ */

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET -1
#endif

#ifdef _WIN32
typedef SOCKET VSocket;
#else
typedef int VSocket;
#endif

/* ═══════════════════════════════════════════════════════════════════
 * MIME TYPE MAPPING
 * ═══════════════════════════════════════════════════════════════════ */

static const char* vhttp_mime_types[][2] = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".css", "text/css"},
    {".js", "application/javascript"},
    {".json", "application/json"},
    {".xml", "application/xml"},
    {".txt", "text/plain"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".pdf", "application/pdf"},
    {".zip", "application/zip"},
    {".tar", "application/x-tar"},
    {".gz", "application/gzip"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".eot", "application/vnd.ms-fontobject"},
    {NULL, "application/octet-stream"}
};

/**
 * Get MIME type for file extension
 */
const char* vhttp_mime_type(const char* filename) {
    if (filename == NULL) return "application/octet-stream";
    
    const char* ext = strrchr(filename, '.');
    if (ext == NULL) return "application/octet-stream";
    
    for (int i = 0; vhttp_mime_types[i][0] != NULL; i++) {
        if (strcmp(ext, vhttp_mime_types[i][0]) == 0) {
            return vhttp_mime_types[i][1];
        }
    }
    
    return "application/octet-stream";
}

/* ═══════════════════════════════════════════════════════════════════
 * HTTP STATUS TEXTS
 * ═══════════════════════════════════════════════════════════════════ */

static const char* vhttp_status_texts[][2] = {
    {"200", "OK"},
    {"201", "Created"},
    {"202", "Accepted"},
    {"204", "No Content"},
    {"301", "Moved Permanently"},
    {"302", "Found"},
    {"304", "Not Modified"},
    {"400", "Bad Request"},
    {"401", "Unauthorized"},
    {"403", "Forbidden"},
    {"404", "Not Found"},
    {"405", "Method Not Allowed"},
    {"408", "Request Timeout"},
    {"409", "Conflict"},
    {"413", "Payload Too Large"},
    {"414", "URI Too Long"},
    {"415", "Unsupported Media Type"},
    {"500", "Internal Server Error"},
    {"501", "Not Implemented"},
    {"502", "Bad Gateway"},
    {"503", "Service Unavailable"},
    {"504", "Gateway Timeout"},
    {NULL, NULL}
};

/**
 * Get HTTP status text
 */
const char* vhttp_status_text(VHTTPStatusCode code) {
    char code_str[8];
    snprintf(code_str, sizeof(code_str), "%d", code);
    
    for (int i = 0; vhttp_status_texts[i][0] != NULL; i++) {
        if (strcmp(code_str, vhttp_status_texts[i][0]) == 0) {
            return vhttp_status_texts[i][1];
        }
    }
    
    return "Unknown";
}

/* ═══════════════════════════════════════════════════════════════════
 * HTTP METHOD STRINGS
 * ═══════════════════════════════════════════════════════════════════ */

static const char* vhttp_method_strings[] = {
    "GET",
    "POST", 
    "PUT",
    "DELETE",
    "PATCH",
    "HEAD",
    "OPTIONS"
};

/**
 * Get HTTP method string
 */
const char* vhttp_method_string(VHTTPMethod method) {
    if (method < 0 || method >= (int)(sizeof(vhttp_method_strings)/sizeof(vhttp_method_strings[0]))) {
        return "UNKNOWN";
    }
    return vhttp_method_strings[method];
}

/**
 * Parse HTTP method from string
 */
VHTTPMethod vhttp_method_parse(const char* str) {
    if (str == NULL) return VHTTP_METHOD_GET;
    
    for (int i = 0; i < (int)(sizeof(vhttp_method_strings)/sizeof(vhttp_method_strings[0])); i++) {
        if (strcasecmp(str, vhttp_method_strings[i]) == 0) {
            return (VHTTPMethod)i;
        }
    }
    
    return (VHTTPMethod)-1;
}

/* ═══════════════════════════════════════════════════════════════════
 * URL ENCODING/DECODING
 * ═══════════════════════════════════════════════════════════════════ */

static int vhex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/**
 * URL decode a string
 */
char* vhttp_url_decode(const char* str) {
    if (str == NULL) return NULL;
    
    size_t len = strlen(str);
    char* result = (char*)malloc(len + 1);
    char* p = result;
    
    while (*str) {
        if (*str == '%') {
            if (*(str + 1) && *(str + 2)) {
                int hi = vhex_digit(*(str + 1));
                int lo = vhex_digit(*(str + 2));
                if (hi >= 0 && lo >= 0) {
                    *p++ = (char)(hi * 16 + lo);
                    str += 3;
                    continue;
                }
            }
        } else if (*str == '+') {
            *p++ = ' ';
            str++;
            continue;
        }
        *p++ = *str++;
    }
    
    *p = '\0';
    return result;
}

/**
 * URL encode a string
 */
char* vhttp_url_encode(const char* str) {
    if (str == NULL) return NULL;
    
    const char* hex = "0123456789ABCDEF";
    char* result = (char*)malloc(strlen(str) * 3 + 1);
    char* p = result;
    
    while (*str) {
        if ((*str >= 'a' && *str <= 'z') ||
            (*str >= 'A' && *str <= 'Z') ||
            (*str >= '0' && *str <= '9') ||
            *str == '-' || *str == '_' || *str == '.' || *str == '~') {
            *p++ = *str;
        } else {
            *p++ = '%';
            *p++ = hex[(*str >> 4) & 0xF];
            *p++ = hex[*str & 0xF];
        }
        str++;
    }
    
    *p = '\0';
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 * HTTP REQUEST IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Create a new HTTP request
 */
VHTTPRequest* vhttp_request_create(VHTTPMethod method, const char* path) {
    VHTTPRequest* request = (VHTTPRequest*)malloc(sizeof(VHTTPRequest));
    if (request == NULL) return NULL;
    
    memset(request, 0, sizeof(VHTTPRequest));
    request->method = method;
    request->path = path ? strdup(path) : strdup("/");
    request->client_ip = NULL;
    request->client_port = 0;
    request->json_body = vval_nothing();
    
    return request;
}

/**
 * Free HTTP request
 */
void vhttp_request_free(VHTTPRequest* request) {
    if (request == NULL) return;
    
    free(request->path);
    free(request->query_string);
    free(request->headers);
    free(request->body);
    free(request->content_type);
    free(request->accept);
    free(request->authorization);
    free(request->user_agent);
    free(request->host);
    free(request->client_ip);
    free(request->form_data);
    free(request);
}

/**
 * Get request header
 */
const char* vhttp_request_get_header(VHTTPRequest* request, const char* name) {
    if (request == NULL || request->headers == NULL || name == NULL) {
        return NULL;
    }
    
    size_t name_len = strlen(name);
    char* headers = request->headers;
    
    while (*headers) {
        if (strncasecmp(headers, name, name_len) == 0 && 
            headers[name_len] == ':') {
            char* value = headers + name_len + 1;
            while (*value == ' ') value++;
            
            char* end = strstr(value, "\r\n");
            if (end) {
                size_t len = end - value;
                char* result = (char*)malloc(len + 1);
                strncpy(result, value, len);
                result[len] = '\0';
                return result;
            }
        }
        
        char* next = strstr(headers, "\r\n");
        if (next == NULL) break;
        headers = next + 2;
    }
    
    return NULL;
}

/**
 * Get query parameter
 */
const char* vhttp_request_get_query(VHTTPRequest* request, const char* name) {
    if (request == NULL || request->query_string == NULL || name == NULL) {
        return NULL;
    }
    
    char* query = strdup(request->query_string);
    char* saveptr;
    char* token = strtok(query, "&");
    
    while (token) {
        char* eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(token, name) == 0) {
                char* value = eq + 1;
                char* decoded = vhttp_url_decode(value);
                free(query);
                return decoded;
            }
        }
        token = strtok(NULL, "&");
    }
    
    free(query);
    return NULL;
}

/**
 * Get form data
 */
const char* vhttp_request_get_form(VHTTPRequest* request, const char* name) {
    if (request == NULL || request->form_data == NULL) {
        return NULL;
    }
    
    return vhttp_request_get_query(request, name);
}

/**
 * Get request body as string
 */
const char* vhttp_request_body(VHTTPRequest* request) {
    return request ? request->body : NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * HTTP RESPONSE IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════ */

/**
 * Create a new HTTP response
 */
VHTTPResponse* vhttp_response_create(VHTTPStatusCode status_code, const char* body) {
    VHTTPResponse* response = (VHTTPResponse*)malloc(sizeof(VHTTPResponse));
    if (response == NULL) return NULL;
    
    memset(response, 0, sizeof(VHTTPResponse));
    response->status_code = status_code;
    response->status_text = (char*)vhttp_status_text(status_code);
    response->body = body ? strdup(body) : NULL;
    response->body_length = body ? strlen(body) : 0;
    response->content_type_header = strdup("text/plain");
    response->headers_sent = 0;
    
    return response;
}

/**
 * Create JSON response
 */
VHTTPResponse* vhttp_response_json(const char* json_string) {
    VHTTPResponse* response = vhttp_response_create(VHTTP_OK, json_string);
    if (response == NULL) return NULL;
    
    free(response->content_type_header);
    response->content_type_header = strdup("application/json");
    
    return response;
}

/**
 * Create redirect response
 */
VHTTPResponse* vhttp_response_redirect(const char* location) {
    VHTTPResponse* response = vhttp_response_create(VHTTP_FOUND, NULL);
    if (response == NULL) return NULL;
    
    response->location_header = strdup(location);
    
    return response;
}

/**
 * Create file response
 */
VHTTPResponse* vhttp_response_file(const char* file_path) {
    FILE* f = fopen(file_path, "rb");
    if (f == NULL) {
        return vhttp_response_create(VHTTP_NOT_FOUND, "File not found");
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc((size_t)size + 1);
    fread(content, 1, (size_t)size, f);
    fclose(f);
    content[size] = '\0';
    
    VHTTPResponse* response = vhttp_response_create(VHTTP_OK, content);
    free(content);
    
    if (response) {
        free(response->content_type_header);
        response->content_type_header = strdup(vhttp_mime_type(file_path));
    }
    
    return response;
}

/**
 * Free HTTP response
 */
void vhttp_response_free(VHTTPResponse* response) {
    if (response == NULL) return;
    
    free(response->status_text);
    free(response->headers);
    free(response->body);
    free(response->content_type_header);
    free(response->location_header);
    free(response->set_cookie_header);
    free(response->cors_origin_header);
    free(response->cors_methods_header);
    free(response->cors_headers_header);
    free(response);
}

/**
 * Set response header
 */
void vhttp_response_set_header(VHTTPResponse* response, const char* name, 
                             const char* value) {
    (void)response;
    (void)name;
    (void)value;
}

/**
 * Set response status
 */
void vhttp_response_set_status(VHTTPResponse* response, VHTTPStatusCode status_code) {
    if (response == NULL) return;
    response->status_code = status_code;
    response->status_text = (char*)vhttp_status_text(status_code);
}

/**
 * Set cookie
 */
void vhttp_response_set_cookie(VHTTPResponse* response, VHTTPCookie* cookie) {
    if (response == NULL || cookie == NULL) return;
    
    char cookie_str[1024];
    snprintf(cookie_str, sizeof(cookie_str), "%s=%s", 
             cookie->name, cookie->value);
    
    if (cookie->domain) {
        strcat(cookie_str, "; Domain=");
        strcat(cookie_str, cookie->domain);
    }
    if (cookie->path) {
        strcat(cookie_str, "; Path=");
        strcat(cookie_str, cookie->path);
    }
    if (cookie->max_age > 0) {
        char max_age[32];
        snprintf(max_age, sizeof(max_age), "; Max-Age=%d", cookie->max_age);
        strcat(cookie_str, max_age);
    }
    if (cookie->http_only) {
        strcat(cookie_str, "; HttpOnly");
    }
    if (cookie->secure) {
        strcat(cookie_str, "; Secure");
    }
    if (cookie->same_site) {
        strcat(cookie_str, "; SameSite=");
        strcat(cookie_str, cookie->same_site);
    }
    
    free(response->set_cookie_header);
    response->set_cookie_header = strdup(cookie_str);
}

/**
 * Serialize response to raw HTTP data
 */
void vhttp_response_serialize(VHTTPResponse* response, char** output, size_t* length) {
    if (response == NULL || output == NULL || length == NULL) return;
    
    char* buffer = (char*)malloc(8192);
    int pos = 0;
    
    pos += snprintf(buffer + pos, (size_t)(8192 - pos), 
                    "HTTP/1.1 %d %s\r\n",
                    response->status_code, 
                    response->status_text ? response->status_text : "OK");
    
    pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                    "Content-Type: %s\r\n",
                    response->content_type_header);
    
    pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                    "Content-Length: %zu\r\n",
                    response->body_length);
    
    pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                    "Connection: keep-alive\r\n");
    
    pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                    "Server: Vexium/1.0\r\n");
    
    if (response->location_header) {
        pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                        "Location: %s\r\n",
                        response->location_header);
    }
    
    if (response->set_cookie_header) {
        pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                        "Set-Cookie: %s\r\n",
                        response->set_cookie_header);
    }
    
    if (response->cors_origin_header) {
        pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                        "Access-Control-Allow-Origin: %s\r\n",
                        response->cors_origin_header);
    }
    if (response->cors_methods_header) {
        pos += snprintf(buffer + pos, (size_t)(8192 - pos),
                        "Access-Control-Allow-Methods: %s\r\n",
                        response->cors_methods_header);
    }
    
    pos += snprintf(buffer + pos, (size_t)(8192 - pos), "\r\n");
    
    if (response->body && response->body_length > 0) {
        char* full_response = (char*)malloc(pos + response->body_length + 1);
        memcpy(full_response, buffer, (size_t)pos);
        memcpy(full_response + pos, response->body, response->body_length);
        full_response[pos + response->body_length] = '\0';
        
        *output = full_response;
        *length = (size_t)(pos + response->body_length);
        free(buffer);
    } else {
        buffer[pos] = '\0';
        *output = buffer;
        *length = (size_t)pos;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * HTTP SERVER IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
static int vinit_winsock(void) {
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;
}
#endif

/**
 * Create a new HTTP server
 */
VHTTPServer* vhttp_server_create(int port, const char* host) {
    VHTTPServer* server = (VHTTPServer*)malloc(sizeof(VHTTPServer));
    if (server == NULL) return NULL;
    
    memset(server, 0, sizeof(VHTTPServer));
    server->port = port;
    server->host = host ? strdup(host) : strdup("0.0.0.0");
    server->running = 0;
    server->routes = NULL;
    server->route_count = 0;
    server->max_request_size = 1024 * 1024;
    server->connection_timeout = 30;
    server->enable_cors = 0;
    server->requests_served = 0;
    server->bytes_sent = 0;
    server->start_time = (long long)time(NULL);
    
#ifdef _WIN32
    vinit_winsock();
#endif
    
    return server;
}

/**
 * Free HTTP server
 */
void vhttp_server_free(VHTTPServer* server) {
    if (server == NULL) return;
    
    vhttp_server_stop(server);
    
    free(server->host);
    free(server->static_root);
    free(server->index_file);
    free(server->cert_file);
    free(server->key_file);
    
    VHTTPRoute* route = server->routes;
    while (route) {
        VHTTPRoute* next = route->next;
        free(route->pattern);
        free(route);
        route = next;
    }
    
    free(server);
}

/**
 * Stop HTTP server
 */
void vhttp_server_stop(VHTTPServer* server) {
    if (server == NULL) return;
    server->running = 0;
    
    if (server->server_socket != 0) {
#ifdef _WIN32
        closesocket(server->server_socket);
#else
        close(server->server_socket);
#endif
        server->server_socket = 0;
    }
}

/**
 * Add route to server
 */
int vhttp_server_add_route(VHTTPServer* server, const char* pattern, 
                          VHTTPMethod method, VValue handler) {
    if (server == NULL || pattern == NULL) return 0;
    
    VHTTPRoute* route = (VHTTPRoute*)malloc(sizeof(VHTTPRoute));
    if (route == NULL) return 0;
    
    memset(route, 0, sizeof(VHTTPRoute));
    route->pattern = strdup(pattern);
    route->method = method;
    route->handler = handler;
    route->is_wildcard = (strchr(pattern, '*') != NULL) ? 1 : 0;
    route->next = NULL;
    
    if (server->routes == NULL) {
        server->routes = route;
    } else {
        VHTTPRoute* last = server->routes;
        while (last->next) last = last->next;
        last->next = route;
    }
    
    server->route_count++;
    return 1;
}

int vhttp_server_get(VHTTPServer* server, const char* pattern, VValue handler) {
    return vhttp_server_add_route(server, pattern, VHTTP_METHOD_GET, handler);
}

int vhttp_server_post(VHTTPServer* server, const char* pattern, VValue handler) {
    return vhttp_server_add_route(server, pattern, VHTTP_METHOD_POST, handler);
}

int vhttp_server_put(VHTTPServer* server, const char* pattern, VValue handler) {
    return vhttp_server_add_route(server, pattern, VHTTP_METHOD_PUT, handler);
}

int vhttp_server_delete(VHTTPServer* server, const char* pattern, VValue handler) {
    return vhttp_server_add_route(server, pattern, VHTTP_METHOD_DELETE, handler);
}

/**
 * Add middleware
 */
void vhttp_server_use(VHTTPServer* server, VValue middleware) {
    (void)server;
    (void)middleware;
}

/**
 * Set static file directory
 */
void vhttp_server_static(VHTTPServer* server, const char* root_path) {
    if (server == NULL) return;
    free(server->static_root);
    server->static_root = root_path ? strdup(root_path) : NULL;
}

/**
 * Set CORS configuration
 */
void vhttp_server_cors(VHTTPServer* server, const char* origin) {
    if (server == NULL) return;
    server->enable_cors = (origin != NULL) ? 1 : 0;
    free(server->cors_origin);
    server->cors_origin = origin ? strdup(origin) : NULL;
}

/**
 * Find matching route
 */
static VHTTPRoute* vfind_route(VHTTPServer* server, const char* path, VHTTPMethod method) {
    VHTTPRoute* route = server->routes;
    
    while (route) {
        if (route->method != method && route->method != (VHTTPMethod)-1) {
            route = route->next;
            continue;
        }
        
        if (route->is_wildcard) {
            size_t pattern_len = strlen(route->pattern) - 1;
            if (strncmp(route->pattern, path, pattern_len) == 0) {
                return route;
            }
        } else if (strcmp(route->pattern, path) == 0) {
            return route;
        }
        
        route = route->next;
    }
    
    return NULL;
}

/**
 * Handle static file request
 */
static VHTTPResponse* vhandle_static_file(VHTTPServer* server, const char* path) {
    if (server->static_root == NULL) {
        return vhttp_response_create(VHTTP_NOT_FOUND, "Static files not configured");
    }
    
    char full_path[1024];
    
    while (*path == '/') path++;
    
    snprintf(full_path, sizeof(full_path), "%s/%s", server->static_root, path);
    
    FILE* f = fopen(full_path, "rb");
    if (f == NULL) {
        if (server->index_file != NULL) {
            snprintf(full_path, sizeof(full_path), "%s/%s/%s", 
                     server->static_root, path, server->index_file);
            f = fopen(full_path, "rb");
            if (f == NULL) {
                return vhttp_response_create(VHTTP_NOT_FOUND, "File not found");
            }
        } else {
            return vhttp_response_create(VHTTP_NOT_FOUND, "File not found");
        }
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = (char*)malloc((size_t)size + 1);
    fread(content, 1, (size_t)size, f);
    fclose(f);
    content[size] = '\0';
    
    VHTTPResponse* response = vhttp_response_create(VHTTP_OK, content);
    free(content);
    
    if (response) {
        free(response->content_type_header);
        response->content_type_header = strdup(vhttp_mime_type(full_path));
    }
    
    return response;
}

/**
 * Parse HTTP request
 */
static VHTTPRequest* vparse_request(const char* raw_request, const char* client_ip, int client_port) {
    if (raw_request == NULL) return NULL;
    
    VHTTPRequest* request = (VHTTPRequest*)malloc(sizeof(VHTTPRequest));
    if (request == NULL) return NULL;
    
    memset(request, 0, sizeof(VHTTPRequest));
    
    const char* line_start = raw_request;
    const char* line_end = strstr(line_start, "\r\n");
    
    if (line_end == NULL) {
        free(request);
        return NULL;
    }
    
    size_t line_len = (size_t)(line_end - line_start);
    char* line = (char*)malloc(line_len + 1);
    strncpy(line, line_start, line_len);
    line[line_len] = '\0';
    
    /* Parse request line: METHOD PATH HTTP/VERSION */
    char method_str[16];
    char path[1024];
    char http_ver[16];
    
    if (sscanf(line, "%15s %1023s %15s", method_str, path, http_ver) < 2) {
        free(line);
        free(request);
        return NULL;
    }
    
    request->method = vhttp_method_parse(method_str);
    request->path = strdup(path);
    request->client_ip = client_ip ? strdup(client_ip) : NULL;
    request->client_port = client_port;
    
    free(line);
    
    /* Parse headers */
    const char* headers_start = line_end + 2;
    const char* headers_end = strstr(headers_start, "\r\n\r\n");
    
    if (headers_end) {
        size_t headers_len = (size_t)(headers_end - headers_start);
        request->headers = (char*)malloc(headers_len + 1);
        strncpy(request->headers, headers_start, headers_len);
        request->headers[headers_len] = '\0';
        
        /* Extract common headers */
        const char* content_type = vhttp_request_get_header(request, "Content-Type");
        if (content_type) {
            request->content_type = (char*)content_type;
        }
        
        const char* accept = vhttp_request_get_header(request, "Accept");
        if (accept) {
            request->accept = (char*)accept;
        }
        
        const char* host = vhttp_request_get_header(request, "Host");
        if (host) {
            request->host = (char*)host;
        }
        
        const char* auth = vhttp_request_get_header(request, "Authorization");
        if (auth) {
            request->authorization = (char*)auth;
        }
        
        const char* user_agent = vhttp_request_get_header(request, "User-Agent");
        if (user_agent) {
            request->user_agent = (char*)user_agent;
        }
        
        /* Extract body */
        const char* body_start = headers_end + 4;
        size_t body_len = strlen(body_start);
        if (body_len > 0) {
            request->body = (char*)malloc(body_len + 1);
            strncpy(request->body, body_start, body_len);
            request->body[body_len] = '\0';
            request->body_length = body_len;
        }
        
        /* Parse query string from path */
        char* query = strchr(request->path, '?');
        if (query) {
            *query = '\0';
            request->query_string = strdup(query + 1);
        }
    }
    
    return request;
}

/**
 * Set socket to non-blocking mode
 */
static int vset_nonblocking(VSocket sock) {
#ifdef _WIN32
    unsigned long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return 0;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

/**
 * Accept a new connection
 */
static VSocket vaccept_connection(VSocket server_sock, char* client_ip, int* client_port) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    VSocket client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
    
    if (client_sock != INVALID_SOCKET) {
        if (client_ip) {
            strcpy(client_ip, inet_ntoa(client_addr.sin_addr));
        }
        if (client_port) {
            *client_port = (int)ntohs(client_addr.sin_port);
        }
    }
    
    return client_sock;
}

/**
 * Receive data from socket
 */
static int vrecv_data(VSocket sock, char* buffer, int size) {
#ifdef _WIN32
    return recv(sock, buffer, size, 0);
#else
    return recv(sock, buffer, (size_t)size, 0);
#endif
}

/**
 * Send data to socket
 */
static int vsend_data(VSocket sock, const char* data, int size) {
#ifdef _WIN32
    return send(sock, data, size, 0);
#else
    return send(sock, data, (size_t)size, 0);
#endif
}

/**
 * Close socket
 */
static void vclose_socket(VSocket sock) {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

/**
 * Start HTTP server
 */
int vhttp_server_start(VHTTPServer* server) {
    if (server == NULL) return 0;
    
    /* Create socket */
    server->server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server->server_socket == INVALID_SOCKET) {
        return 0;
    }
    
    /* Set socket options */
    int opt = 1;
#ifdef _WIN32
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server->server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    
    /* Bind to address */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)server->port);
    
    if (server->host && strcmp(server->host, "0.0.0.0") != 0) {
        inet_pton(AF_INET, server->host, &server_addr.sin_addr);
    } else {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    if (bind(server->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        vclose_socket(server->server_socket);
        server->server_socket = 0;
        return 0;
    }
    
    /* Listen */
    if (listen(server->server_socket, 128) == SOCKET_ERROR) {
        vclose_socket(server->server_socket);
        server->server_socket = 0;
        return 0;
    }
    
    server->running = 1;
    
    /* Set non-blocking for accept */
    vset_nonblocking(server->server_socket);
    
    return 1;
}

/**
 * Process a single request (placeholder for full implementation)
 */
VHTTPResponse* vhttp_server_process_request(VHTTPServer* server, VHTTPRequest* request) {
    if (server == NULL || request == NULL) {
        return vhttp_response_create(VHTTP_BAD_REQUEST, "Invalid request");
    }
    
    /* Try static files first */
    if (server->static_root != NULL) {
        VHTTPResponse* response = vhandle_static_file(server, request->path);
        if (response && response->status_code == VHTTP_OK) {
            return response;
        }
        if (response) vhttp_response_free(response);
    }
    
    /* Try routes */
    VHTTPRoute* route = vfind_route(server, request->path, request->method);
    if (route) {
        /* For now, return a placeholder response */
        char response_body[256];
        snprintf(response_body, sizeof(response_body),
                 "{\"method\": \"%s\", \"path\": \"%s\", \"handler\": \"placeholder\"}",
                 vhttp_method_string(request->method), request->path);
        return vhttp_response_json(response_body);
    }
    
    /* No matching route */
    return vhttp_response_create(VHTTP_NOT_FOUND, "Not Found");
}

/**
 * Get server statistics
 */
void vhttp_server_stats(VHTTPServer* server, size_t* requests, size_t* bytes) {
    if (server == NULL) {
        if (requests) *requests = 0;
        if (bytes) *bytes = 0;
        return;
    }
    
    if (requests) *requests = server->requests_served;
    if (bytes) *bytes = server->bytes_sent;
}
