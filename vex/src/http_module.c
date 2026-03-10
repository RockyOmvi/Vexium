#include "http_module.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#endif

/* ══════════════════════════════════════════════════════════════
 *  Simple HTTP Client Implementation
 * ══════════════════════════════════════════════════════════════ */

/* Simple URL parsing - extracts host, port, and path from URL */
static bool parse_url(const char* url, char* host, int* port, char* path) {
    /* Expect http://host[:port]/path */
    if (strncmp(url, "http://", 7) != 0) {
        return false;  /* Only HTTP supported, not HTTPS */
    }
    
    const char* p = url + 7;
    
    /* Extract host */
    int i = 0;
    while (*p && *p != ':' && *p != '/') {
        host[i++] = *p++;
    }
    host[i] = '\0';
    
    /* Extract port (default 80) */
    *port = 80;
    if (*p == ':') {
        p++;
        *port = 0;
        while (*p && *p >= '0' && *p <= '9') {
            *port = *port * 10 + (*p++ - '0');
        }
    }
    
    /* Extract path (default /) */
    if (*p == '/') {
        i = 0;
        while (*p) {
            path[i++] = *p++;
        }
        path[i] = '\0';
    } else {
        strcpy(path, "/");
    }
    
    return true;
}

/* Perform HTTP request and return response body */
static char* http_request(const char* method, const char* url, const char* body) {
    char host[256];
    int port;
    char path[512];
    
    if (!parse_url(url, host, &port, path)) {
        return NULL;
    }
    
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return NULL;
    }
#endif
    
    /* Create socket */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return NULL;
    }
    
    /* Resolve hostname */
    struct hostent* server = gethostbyname(host);
    if (server == NULL) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return NULL;
    }
    
    /* Connect */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return NULL;
    }
    
    /* Build request */
    char request[4096];
    int body_len = body ? (int)strlen(body) : 0;
    
    snprintf(request, sizeof(request),
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "User-Agent: Vexium/1.0\r\n"
        "Connection: close\r\n",
        method, path, host);
    
    if (body && body_len > 0) {
        char content_header[256];
        snprintf(content_header, sizeof(content_header),
            "Content-Type: application/x-www-form-urlencoded\r\n"
            "Content-Length: %d\r\n", body_len);
        strcat(request, content_header);
    }
    
    strcat(request, "\r\n");
    
    /* Send request */
    if (send(sock, request, (int)strlen(request), 0) < 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return NULL;
    }
    
    /* Send body for POST */
    if (body && body_len > 0) {
        if (send(sock, body, body_len, 0) < 0) {
#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
            return NULL;
        }
    }
    
    /* Receive response */
    char* response = malloc(65536);
    int total = 0;
    int capacity = 65536;
    
    while (1) {
        if (total + 4096 > capacity) {
            capacity *= 2;
            response = realloc(response, capacity);
        }
        
        int n = recv(sock, response + total, 4096, 0);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
    
#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
    
    /* Find body (after \r\n\r\n) */
    char* body_start = strstr(response, "\r\n\r\n");
    if (body_start) {
        size_t body_len = strlen(body_start + 4);
        char* result = (char*)malloc(body_len + 1);
        strcpy(result, body_start + 4);
        free(response);
        return result;
    }
    
    return response;
}

/* ══════════════════════════════════════════════════════════════
 *  VM Native Functions
 * ══════════════════════════════════════════════════════════════ */

/* http_get(url) → returns response body as string */
static Value native_http_get(int argCount, Value* args) {
    if (argCount != 1 || !is_obj(args[0])) {
        return val_nothing_v();
    }
    
    Obj* obj = (Obj*)as_obj(args[0]);
    if (obj->type != OBJ_STRING) {
        return val_nothing_v();
    }
    
    ObjString* url = (ObjString*)obj;
    char* response = http_request("GET", url->chars, NULL);
    
    if (!response) {
        return val_nothing_v();
    }
    
    ObjString* result = obj_string_new(response, (int)strlen(response));
    free(response);
    return val_obj(result);
}

/* http_post(url, body) → returns response body as string */
static Value native_http_post(int argCount, Value* args) {
    if (argCount != 2 || !is_obj(args[0]) || !is_obj(args[1])) {
        return val_nothing_v();
    }
    
    Obj* obj0 = (Obj*)as_obj(args[0]);
    Obj* obj1 = (Obj*)as_obj(args[1]);
    if (obj0->type != OBJ_STRING || obj1->type != OBJ_STRING) {
        return val_nothing_v();
    }
    
    ObjString* url = (ObjString*)obj0;
    ObjString* body = (ObjString*)obj1;
    
    char* response = http_request("POST", url->chars, body->chars);
    
    if (!response) {
        return val_nothing_v();
    }
    
    ObjString* result = obj_string_new(response, (int)strlen(response));
    free(response);
    return val_obj(result);
}

/* ══════════════════════════════════════════════════════════════
 *  Registration
 * ══════════════════════════════════════════════════════════════ */

void vm_register_http_module(VM* vm) {
    vm_define_native(vm, "http_get", native_http_get, 1);
    vm_define_native(vm, "http_post", native_http_post, 2);
}
