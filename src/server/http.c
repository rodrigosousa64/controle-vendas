#include "app.h"
#include "civetweb.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t g_running = 1;
static struct mg_context *g_server_context = NULL;

void http_stop(void) {
    g_running = 0;
}

static void tratar_sinal(int sig) {
    (void)sig;
    g_running = 0;
}

/* Envia uma resposta JSON construída pelas Views */
static void enviar_resposta_json(struct mg_connection *conn, Response *resp) {
    const char *corpo = resp->body ? resp->body : "{\"error\":\"Erro interno.\"}";
    const char *status_str = resp->status < 400 ? "OK" : (resp->status == 404 ? "Not Found" : "Error");

    mg_printf(conn,
              "HTTP/1.1 %d %s\r\n"
              "Content-Type: %s\r\n"
              "Content-Length: %zu\r\n"
              "Connection: close\r\n"
              "X-Content-Type-Options: nosniff\r\n\r\n",
              resp->status, status_str,
              resp->content_type ? resp->content_type : "application/json; charset=utf-8",
              strlen(corpo));
    mg_write(conn, corpo, strlen(corpo));

    free(resp->body);
    resp->body = NULL;
}

/* Handler geral para requisições de API */
static int api_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *info = mg_get_request_info(conn);

    /* Valida origem local contra CSRF */
    const char *origin = mg_get_header(conn, "Origin");
    if (origin && !strstr(origin, "127.0.0.1") && !strstr(origin, "localhost")) {
        Response resp = json_erro(403, "Origem não autorizada.");
        enviar_resposta_json(conn, &resp);
        return 1;
    }

    Request req = {0};
    snprintf(req.method, sizeof(req.method), "%s", info->request_method);
    snprintf(req.path, sizeof(req.path), "%s", info->local_uri);
    if (info->query_string) {
        snprintf(req.query, sizeof(req.query), "%s", info->query_string);
    }

    /* Leitura segura do corpo da requisição */
    const char *len_header = mg_get_header(conn, "Content-Length");
    long body_len = 0;
    if (len_header) {
        char *end = NULL;
        errno = 0;
        body_len = strtol(len_header, &end, 10);
        if (errno || !end || *end || body_len < 0 || body_len > MAX_BODY) {
            Response resp = json_erro(body_len > MAX_BODY ? 413 : 400,
                                      body_len > MAX_BODY ? "Corpo da requisição acima de 64 KiB." : "Tamanho inválido.");
            enviar_resposta_json(conn, &resp);
            return 1;
        }
    }

    if (body_len > 0) {
        const char *content_type = mg_get_header(conn, "Content-Type");
        if (!content_type || strncasecmp(content_type, "application/json", 16) != 0) {
            Response resp = json_erro(415, "Utilize Content-Type: application/json.");
            enviar_resposta_json(conn, &resp);
            return 1;
        }

        req.body = calloc((size_t)body_len + 1, 1);
        if (!req.body) {
            Response resp = json_erro(500, "Memória insuficiente.");
            enviar_resposta_json(conn, &resp);
            return 1;
        }
        mg_read(conn, req.body, (size_t)body_len);
        req.body_len = (size_t)body_len;
    }

    /* Delega para o Roteador */
    Response resp = rotear_requisicao(&req);
    enviar_resposta_json(conn, &resp);

    free(req.body);
    return 1;
}

/* Helper para enviar arquivos com cabeçalhos de segurança */
static int enviar_arquivo_estatico(struct mg_connection *conn, const char *caminho, const char *mime_type) {
    FILE *f = fopen(caminho, "rb");
    if (!f) {
        Response resp = json_erro(404, "Arquivo não encontrado.");
        enviar_resposta_json(conn, &resp);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    mg_printf(conn,
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: %s\r\n"
              "Content-Length: %ld\r\n"
              "Connection: close\r\n"
              "Cache-Control: no-store\r\n"
              "X-Content-Type-Options: nosniff\r\n"
              "Referrer-Policy: no-referrer\r\n"
              "Content-Security-Policy: default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'\r\n\r\n",
              mime_type, size);

    char buffer[4096];
    size_t lidos;
    while ((lidos = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        mg_write(conn, buffer, lidos);
    }
    fclose(f);
    return 1;
}

/* Handlers para as páginas HTML (Templates) */
static int template_dashboard_handler(struct mg_connection *conn, void *data) {
    (void)data;
    return enviar_arquivo_estatico(conn, "templates/dashboard.html", "text/html; charset=utf-8");
}

static int template_produtos_handler(struct mg_connection *conn, void *data) {
    (void)data;
    return enviar_arquivo_estatico(conn, "templates/produtos.html", "text/html; charset=utf-8");
}

static int template_vendas_handler(struct mg_connection *conn, void *data) {
    (void)data;
    return enviar_arquivo_estatico(conn, "templates/vendas.html", "text/html; charset=utf-8");
}

static int template_relatorios_handler(struct mg_connection *conn, void *data) {
    (void)data;
    return enviar_arquivo_estatico(conn, "templates/relatorios.html", "text/html; charset=utf-8");
}

/* Handler para arquivos estáticos (CSS, JS, Imagens) */
static int static_assets_handler(struct mg_connection *conn, void *data) {
    (void)data;
    const struct mg_request_info *info = mg_get_request_info(conn);
    const char *uri = info->local_uri;

    char caminho_arquivo[512];
    if (strncmp(uri, "/static/", 8) == 0) {
        snprintf(caminho_arquivo, sizeof(caminho_arquivo), "static/%s", uri + 8);
    } else if (strcmp(uri, "/style.css") == 0) {
        snprintf(caminho_arquivo, sizeof(caminho_arquivo), "static/css/style.css");
    } else if (strcmp(uri, "/favicon.svg") == 0) {
        snprintf(caminho_arquivo, sizeof(caminho_arquivo), "static/img/favicon.svg");
    } else {
        Response resp = json_erro(404, "Recurso não encontrado.");
        enviar_resposta_json(conn, &resp);
        return 1;
    }

    const char *mime = "text/plain";
    if (strstr(caminho_arquivo, ".css"))  mime = "text/css; charset=utf-8";
    else if (strstr(caminho_arquivo, ".js"))   mime = "application/javascript; charset=utf-8";
    else if (strstr(caminho_arquivo, ".svg"))  mime = "image/svg+xml";
    else if (strstr(caminho_arquivo, ".html")) mime = "text/html; charset=utf-8";

    return enviar_arquivo_estatico(conn, caminho_arquivo, mime);
}

/* Bloqueio de segurança para diretórios internos */
static int not_found_handler(struct mg_connection *conn, void *data) {
    (void)data;
    Response resp = json_erro(404, "Acesso negado.");
    enviar_resposta_json(conn, &resp);
    return 1;
}

int http_run(const char *host, int port) {
    (void)host;
    char listening_port[16];
    snprintf(listening_port, sizeof(listening_port), "%d", port);

    char *options[] = {
        "listening_ports", listening_port,
        "enable_directory_listing", "no",
        NULL
    };

    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    g_server_context = mg_start(&callbacks, NULL, (const char **)options);
    if (!g_server_context) {
        fprintf(stderr, "Erro: Não foi possível iniciar o servidor na porta %d.\n", port);
        return 1;
    }

    /* Registro das rotas da API */
    mg_set_request_handler(g_server_context, "/api/", api_handler, NULL);

    /* Registro dos Templates HTML (Padrão MVT) */
    mg_set_request_handler(g_server_context, "/$", template_dashboard_handler, NULL);
    mg_set_request_handler(g_server_context, "/dashboard$", template_dashboard_handler, NULL);
    mg_set_request_handler(g_server_context, "/index.html$", template_dashboard_handler, NULL);
    mg_set_request_handler(g_server_context, "/produtos$", template_produtos_handler, NULL);
    mg_set_request_handler(g_server_context, "/vendas$", template_vendas_handler, NULL);
    mg_set_request_handler(g_server_context, "/relatorios$", template_relatorios_handler, NULL);

    /* Registro de arquivos estáticos */
    mg_set_request_handler(g_server_context, "/static/", static_assets_handler, NULL);
    mg_set_request_handler(g_server_context, "/style.css$", static_assets_handler, NULL);
    mg_set_request_handler(g_server_context, "/favicon.svg$", static_assets_handler, NULL);

    /* Proteção de diretórios de código e dados */
    mg_set_request_handler(g_server_context, "/data/", not_found_handler, NULL);
    mg_set_request_handler(g_server_context, "/src/", not_found_handler, NULL);
    mg_set_request_handler(g_server_context, "/vendor/", not_found_handler, NULL);
    mg_set_request_handler(g_server_context, "/templates/", not_found_handler, NULL);

    signal(SIGINT, tratar_sinal);
    signal(SIGTERM, tratar_sinal);

    printf("\n  ======================================================\n");
    printf("  VENDA • Sistema de Controle de Vendas %s\n", APP_VERSION);
    printf("  Arquitetura MVT (Model-View-Template) com C + Web\n");
    printf("  Acesse no navegador: http://127.0.0.1:%d\n", port);
    printf("  Pressione Ctrl+C para encerrar o servidor com segurança.\n");
    printf("  ======================================================\n\n");
    fflush(stdout);

    while (g_running) {
        struct timespec sleep_time = {0, 100000000}; /* 100ms */
        nanosleep(&sleep_time, NULL);
    }

    mg_stop(g_server_context);
    g_server_context = NULL;
    puts("\nServidor encerrado. Dados preservados no banco de dados.");
    return 0;
}
