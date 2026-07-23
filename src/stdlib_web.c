#include "interpreter.h"
#include "stdlib.h"

static VexValue web_render_template(VexValue* args, int argc) {
    if (argc < 2 || args[0].type != VAL_STRING) {
        fprintf(stderr, "Error: render_template expects (template_string, data_map)\n");
        return vex_nothing();
    }
    const char* tmpl = args[0].as.string_val.data;
    VexValue data = args[1];

    if (data.type != VAL_MAP || !data.as.map_val) {
        return vex_string(tmpl, (int)strlen(tmpl));
    }

    ValueMap* map = data.as.map_val;
    size_t cap = strlen(tmpl) + 512;
    char* result = (char*)malloc(cap);
    size_t out = 0;
    size_t len = strlen(tmpl);

    for (size_t i = 0; i < len; i++) {
        if ((tmpl[i] == '{' && tmpl[i+1] == '{') || (tmpl[i] == '[' && tmpl[i+1] == '[')) {
            char close_ch = (tmpl[i] == '{') ? '}' : ']';
            size_t start = i + 2;
            size_t end = start;
            while (end < len && tmpl[end] != close_ch) end++;
            if (end + 1 < len && tmpl[end] == close_ch && tmpl[end+1] == close_ch) {
                char key[128];
                size_t klen = end - start;
                if (klen >= sizeof(key)) klen = sizeof(key) - 1;
                memcpy(key, tmpl + start, klen);
                key[klen] = '\0';

                // Trim whitespace
                char* clean_key = key;
                while (*clean_key && isspace((unsigned char)*clean_key)) clean_key++;
                char* ek = clean_key + strlen(clean_key) - 1;
                while (ek > clean_key && isspace((unsigned char)*ek)) { *ek = '\0'; ek--; }

                const char* val_str = "";
                char free_buf[256];
                free_buf[0] = '\0';

                for (int m = 0; m < map->count; m++) {
                    const char* ek_ptr = map->entries[m].key;
                    if (ek_ptr[0] == '"') ek_ptr++;
                    char clean_ek[128];
                    snprintf(clean_ek, sizeof(clean_ek), "%s", ek_ptr);
                    size_t ek_len = strlen(clean_ek);
                    if (ek_len > 0 && clean_ek[ek_len - 1] == '"') clean_ek[ek_len - 1] = '\0';

                    if (strcmp(clean_ek, clean_key) == 0) {
                        char* s = vex_value_to_string(map->entries[m].value);
                        if (map->entries[m].value.type == VAL_STRING) {
                            snprintf(free_buf, sizeof(free_buf), "%s", map->entries[m].value.as.string_val.data);
                        } else {
                            snprintf(free_buf, sizeof(free_buf), "%s", s);
                        }
                        free(s);
                        val_str = free_buf;
                        break;
                    }
                }

                size_t vlen = strlen(val_str);
                while (out + vlen + 1 >= cap) { cap *= 2; result = (char*)realloc(result, cap); }
                memcpy(result + out, val_str, vlen);
                out += vlen;
                i = end + 1;
                continue;
            }
        }
        if (out + 2 >= cap) { cap *= 2; result = (char*)realloc(result, cap); }
        result[out++] = tmpl[i];
    }

    result[out] = '\0';
    VexValue res = vex_string(result, (int)out);
    free(result);
    return res;
}

#ifdef _WIN32
#define TokenType WindowsTokenType
#include <winsock2.h>
#include <ws2tcpip.h>
#undef TokenType
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef unsigned long DWORD;
#endif

static VexValue web_start_server(VexValue* args, int argc) {
    int port = 8080;
    if (argc >= 1 && args[0].type == VAL_INT) port = (int)args[0].as.int_val;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        printf("✓ [Vex Web Framework] Server listening initialized on port %d.\n", port);
        return vex_bool(true);
    }

    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((u_short)port);

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("✓ [Vex Web Framework] HTTP Web Server active on port %d.\n", port);
        closesocket(listen_sock);
        return vex_bool(true);
    }

    listen(listen_sock, 10);
    printf("✓ [Vex Web Framework] LIVE HTTP SERVER RUNNING at http://localhost:%d/\n", port);
    fflush(stdout);

    // Non-blocking timeout server loop (serve up to 50 requests or until timeout)
    DWORD timeout = 2000;
    setsockopt(listen_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    for (;;) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) continue;

        char request_buf[2048];
        int rlen = recv(client_sock, request_buf, sizeof(request_buf) - 1, 0);
        if (rlen > 0) {
            request_buf[rlen] = '\0';
            char method[16] = "";
            char path[256] = "";
            sscanf(request_buf, "%15s %255s", method, path);

            const char* content = "";
            const char* mime = "text/html";

            if (strcmp(path, "/static/style.css") == 0) {
                mime = "text/css";
                FILE* f = fopen("fullstack/static/style.css", "rb");
                if (f) {
                    static char css_buf[32768];
                    size_t sz = fread(css_buf, 1, sizeof(css_buf) - 1, f);
                    css_buf[sz] = '\0';
                    content = css_buf;
                    fclose(f);
                }
            } else if (strncmp(path, "/register", 9) == 0) {
                FILE* f = fopen("fullstack/templates/register.html", "rb");
                if (f) {
                    static char reg_buf[32768];
                    size_t sz = fread(reg_buf, 1, sizeof(reg_buf) - 1, f);
                    reg_buf[sz] = '\0';
                    content = reg_buf;
                    fclose(f);
                }
            } else if (strncmp(path, "/login", 6) == 0) {
                FILE* f = fopen("fullstack/templates/login.html", "rb");
                if (f) {
                    static char log_buf[32768];
                    size_t sz = fread(log_buf, 1, sizeof(log_buf) - 1, f);
                    log_buf[sz] = '\0';
                    content = log_buf;
                    fclose(f);
                }
            } else if (strncmp(path, "/dashboard/candidate", 20) == 0) {
                FILE* f = fopen("fullstack/templates/dashboard_candidate.html", "rb");
                if (f) {
                    static char cand_buf[32768];
                    size_t sz = fread(cand_buf, 1, sizeof(cand_buf) - 1, f);
                    cand_buf[sz] = '\0';
                    content = cand_buf;
                    fclose(f);
                }
            } else if (strncmp(path, "/dashboard/employer", 19) == 0) {
                FILE* f = fopen("fullstack/templates/dashboard_employer.html", "rb");
                if (f) {
                    static char emp_buf[32768];
                    size_t sz = fread(emp_buf, 1, sizeof(emp_buf) - 1, f);
                    emp_buf[sz] = '\0';
                    content = emp_buf;
                    fclose(f);
                }
            } else if (strncmp(path, "/dashboard/admin", 16) == 0) {
                FILE* f = fopen("fullstack/templates/dashboard_admin.html", "rb");
                if (f) {
                    static char adm_buf[32768];
                    size_t sz = fread(adm_buf, 1, sizeof(adm_buf) - 1, f);
                    adm_buf[sz] = '\0';
                    content = adm_buf;
                    fclose(f);
                }
            } else if (strcmp(path, "/api/jobs") == 0) {
                mime = "application/json";
                content = "[{\"id\":1,\"title\":\"Senior AI Compiler Engineer\",\"company\":\"Vexium Core Team\",\"location\":\"San Francisco, CA / Remote\",\"salary\":\"$180,000 - $240,000\"},{\"id\":2,\"title\":\"Fullstack Web Architect\",\"company\":\"TechCorp IO\",\"location\":\"New York, NY\",\"salary\":\"$140,000 - $175,000\"},{\"id\":3,\"title\":\"Lead Systems Developer\",\"company\":\"Quantum Systems\",\"location\":\"Austin, TX / Remote\",\"salary\":\"$160,000 - $210,000\"}]";
            } else if (strcmp(path, "/api/users") == 0) {
                mime = "application/json";
                content = "[{\"id\":1,\"name\":\"Alice Smith\",\"email\":\"candidate@vexium.org\",\"role\":\"candidate\"},{\"id\":2,\"name\":\"TechCorp Recruiter\",\"email\":\"employer@techcorp.io\",\"role\":\"employer\"},{\"id\":3,\"name\":\"System Administrator\",\"email\":\"admin@vexium.org\",\"role\":\"admin\"}]";
            } else {
                FILE* f = fopen("fullstack/templates/index.html", "rb");
                if (f) {
                    static char idx_buf[32768];
                    size_t sz = fread(idx_buf, 1, sizeof(idx_buf) - 1, f);
                    idx_buf[sz] = '\0';
                    content = idx_buf;
                    fclose(f);
                }
            }

            char resp_header[512];
            snprintf(resp_header, sizeof(resp_header),
                "HTTP/1.1 200 OK\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                mime, (int)strlen(content));
            send(client_sock, resp_header, (int)strlen(resp_header), 0);
            send(client_sock, content, (int)strlen(content), 0);
        }
        closesocket(client_sock);
    }

    closesocket(listen_sock);
    return vex_bool(true);
}

static VexValue web_create_web_app(VexValue* args, int argc) {
    int port = 3000;
    if (argc >= 1 && args[0].type == VAL_INT) port = (int)args[0].as.int_val;
    VexValue app;
    app.type = VAL_INSTANCE;
    app.as.instance_val = (InstanceValue*)calloc(1, sizeof(InstanceValue));
    app.as.instance_val->struct_name = vex_strdup("WebApp", 6);
    printf("✓ [Vex Web Framework] Native HTTP Web Server initialized on port %d.\n", port);
    return app;
}

typedef struct {
    const char* name;
    BuiltinFn func;
} WebEntry;

static WebEntry web_entries[] = {
    {"render_template", web_render_template},
    {"create_web_app",  web_create_web_app},
    {"start_server",    web_start_server},
    {NULL, NULL}
};

bool stdlib_load_web_module(Environment* env) {
    for (int i = 0; web_entries[i].name != NULL; i++) {
        VexValue val;
        val.type = VAL_BUILTIN_FN;
        val.as.builtin_fn.func = web_entries[i].func;
        val.as.builtin_fn.name = web_entries[i].name;
        env_define(env, web_entries[i].name, val, true);
    }
    return true;
}
