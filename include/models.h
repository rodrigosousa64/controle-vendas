#ifndef VENDA_MODELS_H
#define VENDA_MODELS_H

#include <stdint.h>
#include <stddef.h>
#include "sqlite3.h"

#define MAX_ITEMS 200
#define MAX_QUANTITY 1000000
#define MAX_PRICE_CENTS 100000000
#define MAX_SALE_CENTS 9000000000000LL

/* Códigos de retorno padronizados para o Model */
#define MODEL_OK                  0
#define MODEL_ERRO_GERAL         -1
#define MODEL_NAO_ENCONTRADO     -2
#define MODEL_DUPLICADO          -3
#define MODEL_ESTOQUE_INSUF      -4
#define MODEL_CONFLITO           -5
#define MODEL_PARAMETRO_INVALIDO -6

/*
 * REQUISITO 2 DO EDITAL:
 * "Cadastro de registros: Utilização de struct para representar os dados,
 * com pelo menos cinco campos relevantes e um identificador único."
 */
typedef struct {
    int64_t id;             /* Identificador único gerado automaticamente */
    char sku[41];           /* Código SKU único do produto */
    char name[121];         /* Nome/descrição do produto */
    char category[61];      /* Categoria (ex: Mercearia, Papelaria) */
    int64_t price_cents;    /* Preço de venda em centavos */
    int stock;              /* Quantidade atual em estoque */
    int min_stock;          /* Quantidade mínima para alerta de estoque baixo */
    int active;             /* 1 para ativo, 0 para inativo */
    char notes[501];        /* Observações adicionais */
} Product;

/* Item pertencente a uma venda */
typedef struct {
    int64_t id;
    int64_t sale_id;
    int64_t product_id;
    char sku[41];
    char name[121];
    int quantity;
    int64_t unit_price_cents;
    int64_t total_cents;
} SaleItem;

/* Registro de Venda */
typedef struct {
    int64_t id;                 /* Identificador único da venda */
    char customer[121];         /* Nome do cliente */
    char payment_method[20];    /* pix, dinheiro, credito, debito */
    int64_t subtotal_cents;     /* Soma dos itens */
    int64_t discount_cents;     /* Desconto aplicado em centavos */
    int64_t total_cents;        /* subtotal - desconto */
    char status[20];            /* completed ou cancelled */
    char notes[501];            /* Observações da venda */
    char created_at[32];        /* Data e hora de criação (ISO UTC) */
    char cancelled_at[32];      /* Data e hora de cancelamento */
    
    /* REQUISITO 7: Ponteiro para array dinâmico alocado com calloc */
    SaleItem *items;
    size_t item_count;
} Sale;

/* Funções de ciclo de vida do Banco de Dados */
int db_open(const char *path);
void db_close(void);
int db_seed(void);
sqlite3 *db_get_handle(void);

/* Operações do Model de Produtos */
int model_listar_produtos(Product **produtos, size_t *total);
int model_buscar_produto_por_id(int64_t id, Product *produto);
int model_inserir_produto(Product *produto);
int model_atualizar_produto(int64_t id, const Product *produto);
int model_excluir_produto(int64_t id);

/* Operações do Model de Vendas */
int model_listar_vendas(Sale **vendas, size_t *total);
int model_buscar_venda_por_id(int64_t id, Sale *venda);
int model_criar_venda(Sale *venda);
int model_cancelar_venda(int64_t id);
void model_liberar_venda(Sale *venda);
void model_liberar_vendas(Sale *vendas, size_t total);

#endif /* VENDA_MODELS_H */
