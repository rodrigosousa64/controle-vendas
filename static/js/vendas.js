'use strict';

let allSales = [];
let availableProducts = [];
let cart = new Map(); // product_id -> { product, quantity }

const paymentLabels = {
  pix: 'Pix',
  dinheiro: 'Dinheiro',
  credito: 'Cartão de crédito',
  debito: 'Cartão de débito'
};

async function carregarVendas() {
  try {
    allSales = await api('/api/sales');
    renderTabelaVendas();
  } catch (err) {
    toast('Erro ao carregar vendas: ' + err.message, true);
  }
}

function renderTabelaVendas() {
  const tbody = document.getElementById('sales-tbody');
  const busca = normalize(document.getElementById('sales-search').value);
  const statusFiltro = document.getElementById('sales-filter-status').value;

  const filtradas = allSales.filter(s => {
    const idStr = String(s.id).padStart(4, '0');
    const matchBusca = !busca ||
      idStr.includes(busca) ||
      normalize(s.customer).includes(busca) ||
      String(s.id) === busca;

    let matchStatus = true;
    if (statusFiltro === 'completed') matchStatus = s.status === 'completed';
    else if (statusFiltro === 'cancelled') matchStatus = s.status === 'cancelled';

    return matchBusca && matchStatus;
  });

  if (filtradas.length === 0) {
    tbody.innerHTML = '<tr><td colspan="8" class="empty-state">Nenhuma venda encontrada.</td></tr>';
    return;
  }

  tbody.innerHTML = filtradas.map(s => {
    const totalItens = (s.items || []).reduce((acc, it) => acc + it.quantity, 0);
    const isCancelada = s.status === 'cancelled';

    return `
      <tr>
        <td><strong>#${String(s.id).padStart(4, '0')}</strong></td>
        <td>${dateTime(s.created_at)}</td>
        <td>${escapeHTML(s.customer || 'Cliente balcão')}</td>
        <td>${paymentLabels[s.payment_method] || s.payment_method}</td>
        <td class="right">${totalItens} un.</td>
        <td class="right" style="font-weight:600;">${money(s.total_cents)}</td>
        <td>
          <span class="badge ${isCancelada ? 'cancelled' : 'completed'}">
            ${isCancelada ? 'Cancelada' : 'Concluída'}
          </span>
        </td>
        <td class="right">
          <button type="button" class="button secondary small" onclick="verComprovante(${s.id})" aria-label="Ver venda #${s.id}">
            Ver venda #${s.id}
          </button>
        </td>
      </tr>
    `;
  }).join('');
}

/* Abertura do Modal de Nova Venda (PDV) */
async function abrirModalNovaVenda() {
  try {
    availableProducts = await api('/api/products');
  } catch (err) {
    toast('Erro ao buscar produtos para venda: ' + err.message, true);
    return;
  }

  cart.clear();

  const html = `
    <div class="modal-header">
      <div>
        <h2 id="modal-title">Nova Venda</h2>
        <p>Selecione os produtos e finalize a transação.</p>
      </div>
      <button type="button" class="icon-button" data-action="close-modal" aria-label="Fechar janela">
        ${icon('close', 18)}
      </button>
    </div>

    <div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:20px;max-height:70vh;overflow-y:auto;padding-right:4px;">
      <!-- Lado Esquerdo: Catálogo de Produtos -->
      <div>
        <div style="margin-bottom:12px;">
          <input type="search" id="catalog-search" placeholder="Buscar produto para adicionar..." style="width:100%;padding:8px 12px;border:1px solid #d0d5dd;border-radius:6px;" oninput="renderCatalogoVenda()" />
        </div>
        <div id="catalog-list" style="display:flex;flex-direction:column;gap:8px;max-height:360px;overflow-y:auto;"></div>
      </div>

      <!-- Lado Direito: Carrinho de Compras -->
      <div style="background:#fcfcfd;border:1px solid #eaecf0;border-radius:8px;padding:16px;display:flex;flex-direction:column;">
        <h3 style="font-size:15px;margin-bottom:12px;">Itens no Carrinho</h3>
        <div id="cart-items" style="flex:1;min-height:120px;max-height:220px;overflow-y:auto;margin-bottom:16px;">
          <p class="empty-state" style="padding:16px 0;">Carrinho vazio. Adicione produtos ao lado.</p>
        </div>

        <form id="form-finalizar-venda" onsubmit="finalizarVenda(event)">
          <div class="form-error" style="display:none;margin-bottom:12px;"></div>

          <div class="form-group" style="margin-bottom:10px;">
            <label for="s-customer">Nome do Cliente</label>
            <input type="text" id="s-customer" placeholder="Cliente balcão (opcional)" maxlength="120" />
          </div>

          <div class="form-grid" style="grid-template-columns:1fr 1fr;gap:8px;margin-bottom:10px;">
            <div class="form-group">
              <label for="s-payment">Pagamento *</label>
              <select id="s-payment" required>
                <option value="pix">Pix</option>
                <option value="dinheiro">Dinheiro</option>
                <option value="credito">Cartão de Crédito</option>
                <option value="debito">Cartão de Débito</option>
              </select>
            </div>
            <div class="form-group">
              <label for="s-discount">Desconto (R$)</label>
              <input type="text" id="s-discount" value="0,00" oninput="atualizarTotaisCarrinho()" />
            </div>
          </div>

          <!-- Resumo de Valores -->
          <div style="border-top:1px dashed #d0d5dd;padding-top:12px;margin-top:8px;">
            <div style="display:flex;justify-content:space-between;font-size:13px;color:#475467;margin-bottom:4px;">
              <span>Subtotal:</span>
              <span id="cart-subtotal">R$ 0,00</span>
            </div>
            <div style="display:flex;justify-content:space-between;font-size:13px;color:#475467;margin-bottom:6px;">
              <span>Desconto:</span>
              <span id="cart-discount">- R$ 0,00</span>
            </div>
            <div style="display:flex;justify-content:space-between;font-size:16px;font-weight:700;color:#1d2939;">
              <span>Total da venda:</span>
              <span id="cart-total" style="color:#ee663e;">R$ 0,00</span>
            </div>
          </div>

          <div style="margin-top:16px;display:flex;justify-content:flex-end;gap:8px;">
            <button type="button" class="button secondary" data-action="close-modal">Cancelar</button>
            <button type="submit" class="button primary" id="btn-finalizar-submit">Finalizar venda</button>
          </div>
        </form>
      </div>
    </div>
  `;

  openModal(html, 'modal-large');
  renderCatalogoVenda();
}

function renderCatalogoVenda() {
  const container = document.getElementById('catalog-list');
  if (!container) return;
  const busca = normalize(document.getElementById('catalog-search').value);

  const disponiveis = availableProducts.filter(p => p.active && p.stock > 0 &&
    (!busca || normalize(p.name).includes(busca) || normalize(p.sku).includes(busca)));

  if (disponiveis.length === 0) {
    container.innerHTML = '<p class="empty-state">Nenhum produto disponível.</p>';
    return;
  }

  container.innerHTML = disponiveis.map(p => {
    const jaNoCarrinho = cart.get(p.id)?.quantity || 0;
    const restante = p.stock - jaNoCarrinho;

    return `
      <div style="display:flex;align-items:center;justify-content:space-between;padding:8px 10px;background:#fff;border:1px solid #eaecf0;border-radius:6px;">
        <div>
          <div style="font-weight:600;font-size:13px;">${escapeHTML(p.name)}</div>
          <div style="font-size:12px;color:#667085;">${money(p.price_cents)} • Restante: ${restante} un.</div>
        </div>
        <button type="button" class="button secondary small" onclick="adicionarAoCarrinho(${p.id})" ${restante <= 0 ? 'disabled' : ''} aria-label="Adicionar ${escapeHTML(p.name)}">
          Adicionar ${escapeHTML(p.name)}
        </button>
      </div>
    `;
  }).join('');
}

function adicionarAoCarrinho(productId) {
  const prod = availableProducts.find(p => p.id === productId);
  if (!prod) return;

  const current = cart.get(productId);
  const qty = current ? current.quantity + 1 : 1;

  if (qty > prod.stock) {
    toast(`Estoque insuficiente de ${prod.name}!`, true);
    return;
  }

  cart.set(productId, { product: prod, quantity: qty });
  renderCarrinho();
  renderCatalogoVenda();
}

function alterarQtdCarrinho(productId, delta) {
  const current = cart.get(productId);
  if (!current) return;
  const newQty = current.quantity + delta;

  if (newQty <= 0) {
    cart.delete(productId);
  } else if (newQty > current.product.stock) {
    toast(`Estoque máximo atingido para ${current.product.name}!`, true);
    return;
  } else {
    current.quantity = newQty;
  }

  renderCarrinho();
  renderCatalogoVenda();
}

function renderCarrinho() {
  const container = document.getElementById('cart-items');
  if (!container) return;

  if (cart.size === 0) {
    container.innerHTML = '<p class="empty-state" style="padding:16px 0;">Carrinho vazio. Adicione produtos ao lado.</p>';
    atualizarTotaisCarrinho();
    return;
  }

  let html = '';
  for (const [id, item] of cart.entries()) {
    const itemTotal = item.quantity * item.product.price_cents;
    html += `
      <div style="display:flex;align-items:center;justify-content:space-between;padding:6px 0;border-bottom:1px solid #f2f4f7;font-size:13px;">
        <div style="flex:1;">
          <div style="font-weight:600;">${escapeHTML(item.product.name)}</div>
          <div style="color:#667085;">${money(item.product.price_cents)} cada</div>
        </div>
        <div style="display:flex;align-items:center;gap:6px;margin:0 12px;">
          <button type="button" class="button secondary small" style="padding:2px 8px;" onclick="alterarQtdCarrinho(${id}, -1)">-</button>
          <strong style="min-width:20px;text-align:center;">${item.quantity}</strong>
          <button type="button" class="button secondary small" style="padding:2px 8px;" onclick="alterarQtdCarrinho(${id}, 1)">+</button>
        </div>
        <div style="font-weight:600;min-width:70px;text-align:right;">${money(itemTotal)}</div>
      </div>
    `;
  }

  container.innerHTML = html;
  atualizarTotaisCarrinho();
}

function atualizarTotaisCarrinho() {
  let subtotal = 0;
  for (const item of cart.values()) {
    subtotal += item.quantity * item.product.price_cents;
  }

  let discount = 0;
  const discountInput = document.getElementById('s-discount');
  if (discountInput) {
    try {
      discount = parseMoney(discountInput.value || '0', true);
    } catch {
      discount = 0;
    }
  }

  if (discount > subtotal) discount = subtotal;
  const total = subtotal - discount;

  const subtotalEl = document.getElementById('cart-subtotal');
  const discountEl = document.getElementById('cart-discount');
  const totalEl = document.getElementById('cart-total');

  if (subtotalEl) subtotalEl.textContent = money(subtotal);
  if (discountEl) discountEl.textContent = `- ${money(discount)}`;
  if (totalEl) totalEl.textContent = money(total);
}

async function finalizarVenda(event) {
  event.preventDefault();
  const form = event.target;
  const errorEl = form.querySelector('.form-error');
  errorEl.style.display = 'none';

  if (cart.size === 0) {
    errorEl.textContent = 'Adicione pelo menos um produto ao carrinho antes de finalizar.';
    errorEl.style.display = 'block';
    return;
  }

  try {
    const customer = form.querySelector('#s-customer').value.trim();
    const payment_method = form.querySelector('#s-payment').value;
    const discountStr = form.querySelector('#s-discount').value.trim();
    const discount_cents = parseMoney(discountStr || '0', true);

    const items = [];
    for (const [productId, it] of cart.entries()) {
      items.push({ product_id: productId, quantity: it.quantity });
    }

    const payload = {
      customer,
      payment_method,
      discount_cents,
      notes: '',
      items
    };

    const novaVenda = await api('/api/sales', {
      method: 'POST',
      body: JSON.stringify(payload)
    });

    closeModal();
    toast('Venda registrada com sucesso!');
    await carregarVendas();
    verComprovante(novaVenda.id);
  } catch (err) {
    errorEl.textContent = err.message;
    errorEl.style.display = 'block';
  }
}

/* Modal de Comprovante de Venda e Cancelamento */
async function verComprovante(saleId) {
  let sale;
  try {
    sale = await api(`/api/sales/${saleId}`);
  } catch (err) {
    toast('Erro ao buscar venda: ' + err.message, true);
    return;
  }

  const isCancelada = sale.status === 'cancelled';
  const idFormatado = `#${String(sale.id).padStart(4, '0')}`;

  const html = `
    <div class="modal-header">
      <div>
        <h2 id="modal-title">Venda ${idFormatado}</h2>
        <p>${dateTime(sale.created_at)}</p>
      </div>
      <button type="button" class="icon-button" data-action="close-modal" aria-label="Fechar janela">
        ${icon('close', 18)}
      </button>
    </div>

    <div style="background:#fff;border:1px solid #eaecf0;border-radius:8px;padding:20px;margin-bottom:16px;">
      <div style="display:flex;justify-content:space-between;margin-bottom:12px;border-bottom:1px solid #f2f4f7;padding-bottom:12px;">
        <div>
          <span style="font-size:12px;color:#667085;">Cliente:</span>
          <div style="font-weight:600;">${escapeHTML(sale.customer || 'Cliente balcão')}</div>
        </div>
        <div>
          <span style="font-size:12px;color:#667085;">Pagamento:</span>
          <div style="font-weight:600;">${paymentLabels[sale.payment_method] || sale.payment_method}</div>
        </div>
        <div>
          <span style="font-size:12px;color:#667085;">Situação:</span>
          <div>
            <span class="badge ${isCancelada ? 'cancelled' : 'completed'}">
              ${isCancelada ? 'Cancelada' : 'Concluída'}
            </span>
          </div>
        </div>
      </div>

      <div class="table-wrap" style="margin-bottom:16px;">
        <table>
          <thead>
            <tr>
              <th>Item</th>
              <th class="right">Qtd</th>
              <th class="right">Preço Un.</th>
              <th class="right">Total</th>
            </tr>
          </thead>
          <tbody>
            ${(sale.items || []).map(it => `
              <tr>
                <td>
                  <strong>${escapeHTML(it.name)}</strong>
                  <div style="font-size:11px;color:#667085;">${escapeHTML(it.sku)}</div>
                </td>
                <td class="right">${it.quantity}</td>
                <td class="right">${money(it.unit_price_cents)}</td>
                <td class="right" style="font-weight:600;">${money(it.total_cents)}</td>
              </tr>
            `).join('')}
          </tbody>
        </table>
      </div>

      <div style="display:flex;flex-direction:column;align-items:flex-end;gap:4px;border-top:1px dashed #d0d5dd;padding-top:12px;">
        <div style="font-size:13px;color:#475467;">Subtotal: ${money(sale.subtotal_cents)}</div>
        <div style="font-size:13px;color:#475467;">Desconto: - ${money(sale.discount_cents)}</div>
        <div class="grand-total" style="font-size:18px;font-weight:700;color:#1d2939;margin-top:4px;">
          Total: <strong style="color:#ee663e;">${money(sale.total_cents)}</strong>
        </div>
      </div>
    </div>

    <div class="modal-footer" style="display:flex;justify-content:space-between;align-items:center;">
      <div>
        ${!isCancelada ? `
          <button type="button" class="button danger small" onclick="cancelarVenda(${sale.id})">
            Cancelar venda
          </button>
        ` : ''}
      </div>
      <div style="display:flex;gap:8px;">
        <button type="button" class="button secondary" onclick="window.print()">
          ${icon('print', 16)} Imprimir
        </button>
        <button type="button" class="button primary" data-action="close-modal">Concluir</button>
      </div>
    </div>
  `;

  openModal(html);
}

/* Cancelamento de venda com devolução de estoque */
async function cancelarVenda(saleId) {
  const confirmed = await askConfirm(
    'Cancelar esta venda?',
    'Ao cancelar, os itens vendidos serão devolvidos ao estoque de cada produto e o valor será excluído dos relatórios de faturamento.',
    'Cancelar venda',
    true
  );
  if (!confirmed) return;

  try {
    await api(`/api/sales/${saleId}/cancel`, { method: 'POST' });
    toast('Venda cancelada e estoque devolvido com sucesso!');
    await carregarVendas();
    verComprovante(saleId);
  } catch (err) {
    toast(err.message, true);
  }
}

document.addEventListener('DOMContentLoaded', () => {
  document.getElementById('sales-search').addEventListener('input', renderTabelaVendas);
  document.getElementById('sales-filter-status').addEventListener('change', renderTabelaVendas);
  document.getElementById('btn-nova-venda').addEventListener('click', abrirModalNovaVenda);
  carregarVendas();
});
