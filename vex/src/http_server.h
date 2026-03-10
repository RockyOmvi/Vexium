/**
 * Vexium HTTP Server Header
 * ========================
 * 
 * HTTP server types and function declarations for Vexium.
 * This module provides HTTP server functionality for web applications.
 * 
 * Author: Vexium Development Team
 * Version: 1.0.0
 */

#ifndef VEXIUM_HTTP_SERVER_H
#define VEXIUM_HTTP_SERVER_H

/* NOTE: This header is designed to be standalone.
 * Do NOT include any system headers here.
 * Include system headers in http_server.c BEFORE including this header.
 */

/* Basic definitions */
#ifndef NULL
#define NULL ((void*)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════
 * FORWARD DECLARATIONS
 * ═══════════════════════════════════════════════════════════════════ */

struct VHTTPRequest;
struct VHTTPResponse;
struct VHTTPServer;
struct VHTTPRoute;
struct VHTTPCookie;

typedef struct VHTTPRequest VHTTPRequest;
typedef struct VHTTPResponse VHTTPResponse;
typedef struct VHTTPServer VHTTPServer;
typedef struct VHTTPRoute VHTTPRoute;
typedef struct VHTTPCookie VHTTPCookie;

/* ═══════════════════════════════════════════════════════════════════
 * HTTP STATUS CODES
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    VHTTP_CONTINUE = 100,
    VHTTP_SWITCHING_PROTOCOLS = 101,
    VHTTP_OK = 200,
    VHTTP_CREATED = 201,
    VHTTP_ACCEPTED = 202,
    VHTTP_NO_CONTENT = 204,
    VHTTP_MOVED_PERMANENTLY = 301,
    VHTTP_FOUND = 302,
    VHTTP_NOT_MODIFIED = 304,
    VHTTP_BAD_REQUEST = 400,
    VHTTP_UNAUTHORIZED = 401,
    VHTTP_FORBIDDEN = 403,
    VHTTP_NOT_FOUND = 404,
    VHTTP_METHOD_NOT_ALLOWED = 405,
    VHTTP_REQUEST_TIMEOUT = 408,
    VHTTP_CONFLICT = 409,
    VHTTP_PAYLOAD_TOO_LARGE = 413,
    VHTTP_URI_TOO_LONG = 414,
    VHTTP_UNSUPPORTED_MEDIA_TYPE = 415,
    VHTTP_INTERNAL_SERVER_ERROR = 500,
    VHTTP_NOT_IMPLEMENTED = 501,
    VHTTP_BAD_GATEWAY = 502,
    VHTTP_SERVICE_UNAVAILABLE = 503,
    VHTTP_GATEWAY_TIMEOUT = 504
} VHTTPStatusCode;

/* ═══════════════════════════════════════════════════════════════════
 * HTTP METHODS
 * ═══════════════════════════════════════════════════════════════════ */

typedef enum {
    VHTTP_METHOD_GET = 0,
    VHTTP_METHOD_POST = 1,
    VHTTP_METHOD_PUT = 2,
    VHTTP_METHOD_DELETE = 3,
    VHTTP_METHOD_PATCH = 4,
    VHTTP_METHOD_HEAD = 5,
    VHTTP_METHOD_OPTIONS = 6
} VHTTPMethod;

/* ═══════════════════════════════════════════════════════════════════
 * VALUE TYPE (for handler callbacks) - minimal version
 * ═══════════════════════════════════════════════════════════════════ */

typedef struct VValue {
    int type;
    union {
        void* ptr;
        long long int_val;
        double double_val;
        int bool_val;
    } data;
} VValue;

static inline VValue vval_nothing(void) {
    VValue v;
    v.type = 0;
    v.data.ptr = NULL;
    return v;
}

/* ═══════════════════════════════════════════════════════════════════
 * HTTP COOKIE
 * ═══════════════════════════════════════════════════════════════════ */

struct VHTTPCookie {
    char* name;
    char* value;
    char* domain;
    char* path;
    int max_age;
    int http_only;
    int secure;
    char* same_site;
};

/* ═══════════════════════════════════════════════════════════════════
 * HTTP REQUEST
 * ═══════════════════════════════════════════════════════════════════ */

struct VHTTPRequest {
    VHTTPMethod method;
    char* path;
    char* query_string;
    char* headers;
    char* body;
    unsigned long body_length;
    char* content_type;
    char* accept;
    char* authorization;
    char* user_agent;
    char* host;
    char* client_ip;
    int client_port;
    char* form_data;
    VValue json_body;
};

/* ═══════════════════════════════════════════════════════════════════
 * HTTP RESPONSE
 * ═══════════════════════════════════════════════════════════════════ */

struct VHTTPResponse {
    VHTTPStatusCode status_code;
    char* status_text;
    char* headers;
    char* body;
    unsigned long body_length;
    char* content_type_header;
    char* location_header;
    char* set_cookie_header;
    char* cors_origin_header;
    char* cors_methods_header;
    char* cors_headers_header;
    int headers_sent;
};

/* ═══════════════════════════════════════════════════════════════════
 * HTTP ROUTE
 * ═══════════════════════════════════════════════════════════════════ */

struct VHTTPRoute {
    char* pattern;
    VHTTPMethod method;
    VValue handler;
    int is_wildcard;
    VHTTPRoute* next;
};

/* ═══════════════════════════════════════════════════════════════════
 * HTTP SERVER
 * ═══════════════════════════════════════════════════════════════════ */

struct VHTTPServer {
    int port;
    char* host;
    int server_socket;
    int running;
    VHTTPRoute* routes;
    unsigned long route_count;
    char* static_root;
    char* index_file;
    char* cert_file;
    char* key_file;
    size_t max_request_size;
    int connection_timeout;
    int enable_cors;
    char* cors_origin;
    unsigned long requests_served;
    unsigned long bytes_sent;
    long long start_time;
};

/* ═══════════════════════════════════════════════════════════════════
 * MIME TYPE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

const char* vhttp_mime_type(const char* filename);

/* ═══════════════════════════════════════════════════════════════════
 * STATUS AND METHOD FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

const char* vhttp_status_text(VHTTPStatusCode code);
const char* vhttp_method_string(VHTTPMethod method);
VHTTPMethod vhttp_method_parse(const char* str);

/* ═══════════════════════════════════════════════════════════════════
 * URL ENCODING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

char* vhttp_url_decode(const char* str);
char* vhttp_url_encode(const char* str);

/* ═══════════════════════════════════════════════════════════════════
 * REQUEST FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

VHTTPRequest* vhttp_request_create(VHTTPMethod method, const char* path);
void vhttp_request_free(VHTTPRequest* request);
const char* vhttp_request_get_header(VHTTPRequest* request, const char* name);
const char* vhttp_request_get_query(VHTTPRequest* request, const char* name);
const char* vhttp_request_get_form(VHTTPRequest* request, const char* name);
const char* vhttp_request_body(VHTTPRequest* request);

/* ═══════════════════════════════════════════════════════════════════
 * RESPONSE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

VHTTPResponse* vhttp_response_create(VHTTPStatusCode status_code, const char* body);
VHTTPResponse* vhttp_response_json(const char* json_string);
VHTTPResponse* vhttp_response_redirect(const char* location);
VHTTPResponse* vhttp_response_file(const char* file_path);
void vhttp_response_free(VHTTPResponse* response);
void vhttp_response_set_header(VHTTPResponse* response, const char* name, const char* value);
void vhttp_response_set_status(VHTTPResponse* response, VHTTPStatusCode status_code);
void vhttp_response_set_cookie(VHTTPResponse* response, VHTTPCookie* cookie);
void vhttp_response_serialize(VHTTPResponse* response, char** output, size_t* length);

/* ═══════════════════════════════════════════════════════════════════
 * SERVER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════ */

VHTTPServer* vhttp_server_create(int port, const char* host);
void vhttp_server_free(VHTTPServer* server);
void vhttp_server_stop(VHTTPServer* server);

/* Route registration */
int vhttp_server_add_route(VHTTPServer* server, const char* pattern, VHTTPMethod method, VValue handler);
int vhttp_server_get(VHTTPServer* server, const char* pattern, VValue handler);
int vhttp_server_post(VHTTPServer* server, const char* pattern, VValue handler);
int vhttp_server_put(VHTTPServer* server, const char* pattern, VValue handler);
int vhttp_server_delete(VHTTPServer* server, const char* pattern, VValue handler);

/* Middleware and configuration */
void vhttp_server_use(VHTTPServer* server, VValue middleware);
void vhttp_server_static(VHTTPServer* server, const char* root_path);
void vhttp_server_cors(VHTTPServer* server, const char* origin);

/* Server lifecycle */
int vhttp_server_start(VHTTPServer* server);
VHTTPResponse* vhttp_server_process_request(VHTTPServer* server, VHTTPRequest* request);
void vhttp_server_stats(VHTTPServer* server, size_t* requests, size_t* bytes);

#ifdef __cplusplus
}
#endif

#endif /* VEXIUM_HTTP_SERVER_H */
