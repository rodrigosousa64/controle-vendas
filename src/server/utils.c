#include "http.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Cria uma Response contendo um corpo JSON */
Response json_resposta(int status, cJSON *json) {
    char *body = json ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    if (!body) {
        return (Response){500, strdup("{\"error\":\"Memória insuficiente.\"}"), "application/json; charset=utf-8"};
    }
    return (Response){status, body, "application/json; charset=utf-8"};
}

/* Cria uma Response de erro padronizada */
Response json_erro(int status, const char *mensagem) {
    cJSON *json = cJSON_CreateObject();
    if (json) {
        cJSON_AddStringToObject(json, "error", mensagem);
    }
    return json_resposta(status, json);
}

/* Helper para extrair texto seguro de um cJSON */
static int extrair_texto(const cJSON *obj, const char *chave, char *out, size_t cap, int obrigatorio) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, chave);
    if (!item && !obrigatorio) {
        out[0] = '\0';
        return 1;
    }
    if (!cJSON_IsString(item)) return 0;

    const char *p = item->valuestring;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t len = strlen(p);
    while (len && isspace((unsigned char)p[len - 1])) len--;

    if ((obrigatorio && len == 0) || len >= cap) return 0;
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)p[i] < 32 && p[i] != '\n' && p[i] != '\t') return 0;
    }

    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

/* Helper para extrair número inteiro seguro de um cJSON */
static int extrair_inteiro(const cJSON *obj, const char *chave, int64_t min, int64_t max, int64_t *out) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, chave);
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble)) return 0;
    if (floor(item->valuedouble) != item->valuedouble) return 0;
    if (item->valuedouble < (double)min || item->valuedouble > (double)max) return 0;
    *out = (int64_t)item->valuedouble;
    return 1;
}

/* Converte JSON string da requisição para a struct Product */
int json_para_produto(const char *json_str, Product *out) {
    if (!json_str) return 0;
    cJSON *root = cJSON_ParseWithOpts(json_str, NULL, 1);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return 0;
    }

    int64_t stock = 0, min_stock = 0;
    const cJSON *active = cJSON_GetObjectItemCaseSensitive(root, "active");

    int ok = extrair_texto(root, "sku", out->sku, sizeof(out->sku), 1) &&
             extrair_texto(root, "name", out->name, sizeof(out->name), 1) &&
             extrair_texto(root, "category", out->category, sizeof(out->category), 1) &&
             extrair_texto(root, "notes", out->notes, sizeof(out->notes), 0) &&
             extrair_inteiro(root, "price_cents", 1, MAX_PRICE_CENTS, &out->price_cents) &&
             extrair_inteiro(root, "stock", 0, MAX_QUANTITY, &stock) &&
             extrair_inteiro(root, "min_stock", 0, MAX_QUANTITY, &min_stock);

    if (active && !cJSON_IsBool(active)) ok = 0;

    if (ok) {
        out->stock = (int)stock;
        out->min_stock = (int)min_stock;
        out->active = (!active || cJSON_IsTrue(active)) ? 1 : 0;
    }

    cJSON_Delete(root);
    return ok;
}

/* Converte a struct Product para cJSON */
cJSON *produto_para_cjson(const Product *p) {
    if (!p) return NULL;
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;

    cJSON_AddNumberToObject(obj, "id", (double)p->id);
    cJSON_AddStringToObject(obj, "sku", p->sku);
    cJSON_AddStringToObject(obj, "name", p->name);
    cJSON_AddStringToObject(obj, "category", p->category);
    cJSON_AddNumberToObject(obj, "price_cents", (double)p->price_cents);
    cJSON_AddNumberToObject(obj, "stock", p->stock);
    cJSON_AddNumberToObject(obj, "min_stock", p->min_stock);
    cJSON_AddBoolToObject(obj, "active", p->active);
    cJSON_AddStringToObject(obj, "notes", p->notes);
    return obj;
}

/* Converte array de produtos em cJSON Array */
cJSON *produtos_para_cjson_array(const Product *produtos, size_t total) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    for (size_t i = 0; i < total; i++) {
        cJSON *item = produto_para_cjson(&produtos[i]);
        if (item) cJSON_AddItemToArray(arr, item);
    }
    return arr;
}

/* Converte JSON string da criação de venda para struct Sale rascunho */
int json_para_venda_rascunho(const char *json_str, Sale *out) {
    if (!json_str) return 0;
    cJSON *root = cJSON_ParseWithOpts(json_str, NULL, 1);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return 0;
    }

    memset(out, 0, sizeof(Sale));
    cJSON *items = cJSON_GetObjectItemCaseSensitive(root, "items");
    int count = cJSON_GetArraySize(items);

    if (!cJSON_IsArray(items) || count < 1 || count > MAX_ITEMS) {
        cJSON_Delete(root);
        return 0;
    }

    int ok = extrair_texto(root, "customer", out->customer, sizeof(out->customer), 0) &&
             extrair_texto(root, "payment_method", out->payment_method, sizeof(out->payment_method), 1) &&
             extrair_texto(root, "notes", out->notes, sizeof(out->notes), 0) &&
             extrair_inteiro(root, "discount_cents", 0, MAX_SALE_CENTS, &out->discount_cents);

    if (!ok) {
        cJSON_Delete(root);
        return 0;
    }

    /* Valida meio de pagamento */
    if (strcmp(out->payment_method, "pix") != 0 &&
        strcmp(out->payment_method, "dinheiro") != 0 &&
        strcmp(out->payment_method, "credito") != 0 &&
        strcmp(out->payment_method, "debito") != 0) {
        cJSON_Delete(root);
        return 0;
    }

    /* REQUISITO 7: Alocação dinâmica com calloc com verificação */
    out->item_count = (size_t)count;
    out->items = calloc(out->item_count, sizeof(SaleItem));
    if (!out->items) {
        cJSON_Delete(root);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        cJSON *item_obj = cJSON_GetArrayItem(items, i);
        int64_t pid = 0, qty = 0;
        if (!cJSON_IsObject(item_obj) ||
            !extrair_inteiro(item_obj, "product_id", 1, 2147483647, &pid) ||
            !extrair_inteiro(item_obj, "quantity", 1, MAX_QUANTITY, &qty)) {
            free(out->items);
            out->items = NULL;
            cJSON_Delete(root);
            return 0;
        }

        out->items[i].product_id = pid;
        out->items[i].quantity = (int)qty;

        /* Rejeita produto repetido na mesma venda */
        for (int j = 0; j < i; j++) {
            if (out->items[j].product_id == pid) {
                free(out->items);
                out->items = NULL;
                cJSON_Delete(root);
                return 0;
            }
        }
    }

    cJSON_Delete(root);
    return 1;
}

/* Converte struct Sale completa para cJSON */
cJSON *venda_para_cjson(const Sale *s) {
    if (!s) return NULL;
    cJSON *obj = cJSON_CreateObject();
    if (!obj) return NULL;

    cJSON_AddNumberToObject(obj, "id", (double)s->id);
    cJSON_AddStringToObject(obj, "customer", s->customer);
    cJSON_AddStringToObject(obj, "payment_method", s->payment_method);
    cJSON_AddNumberToObject(obj, "subtotal_cents", (double)s->subtotal_cents);
    cJSON_AddNumberToObject(obj, "discount_cents", (double)s->discount_cents);
    cJSON_AddNumberToObject(obj, "total_cents", (double)s->total_cents);
    cJSON_AddStringToObject(obj, "status", s->status);
    cJSON_AddStringToObject(obj, "notes", s->notes);
    cJSON_AddStringToObject(obj, "created_at", s->created_at);
    if (s->cancelled_at[0]) {
        cJSON_AddStringToObject(obj, "cancelled_at", s->cancelled_at);
    } else {
        cJSON_AddNullToObject(obj, "cancelled_at");
    }

    /* Itens vinculados */
    cJSON *items_arr = cJSON_CreateArray();
    if (items_arr && s->items) {
        for (size_t i = 0; i < s->item_count; i++) {
            cJSON *it = cJSON_CreateObject();
            cJSON_AddNumberToObject(it, "id", (double)s->items[i].id);
            cJSON_AddNumberToObject(it, "sale_id", (double)s->items[i].sale_id);
            cJSON_AddNumberToObject(it, "product_id", (double)s->items[i].product_id);
            cJSON_AddStringToObject(it, "sku", s->items[i].sku);
            cJSON_AddStringToObject(it, "name", s->items[i].name);
            cJSON_AddNumberToObject(it, "quantity", s->items[i].quantity);
            cJSON_AddNumberToObject(it, "unit_price_cents", (double)s->items[i].unit_price_cents);
            cJSON_AddNumberToObject(it, "total_cents", (double)s->items[i].total_cents);
            cJSON_AddItemToArray(items_arr, it);
        }
    }
    cJSON_AddItemToObject(obj, "items", items_arr);
    return obj;
}

/* Converte lista de vendas para cJSON Array */
cJSON *vendas_para_cjson_array(const Sale *vendas, size_t total) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;
    for (size_t i = 0; i < total; i++) {
        cJSON *item = venda_para_cjson(&vendas[i]);
        if (item) cJSON_AddItemToArray(arr, item);
    }
    return arr;
}

/* Utilitário para decodificar parâmetros de query string (?chave=valor) */
static int valor_hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int query_param(const char *query, const char *key, char *out, size_t capacity) {
    if (!query || !key || !out || capacity == 0) return 0;
    size_t key_len = strlen(key);
    out[0] = '\0';

    while (*query) {
        const char *end = strchr(query, '&');
        if (!end) end = query + strlen(query);
        const char *eq = memchr(query, '=', (size_t)(end - query));
        if (eq && (size_t)(eq - query) == key_len && strncmp(query, key, key_len) == 0) {
            size_t n = 0;
            for (const char *p = eq + 1; p < end; p++) {
                int c = (unsigned char)*p;
                if (c == '%') {
                    if (p + 2 >= end) return -1;
                    int hi = valor_hex(p[1]), lo = valor_hex(p[2]);
                    if (hi < 0 || lo < 0) return -1;
                    c = hi * 16 + lo;
                    p += 2;
                } else if (c == '+') {
                    c = ' ';
                }
                if (!c || n + 1 >= capacity) return -1;
                out[n++] = (char)c;
            }
            out[n] = '\0';
            return 1;
        }
        query = *end ? end + 1 : end;
    }
    return 0;
}
