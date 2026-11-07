'use strict';

document.addEventListener('DOMContentLoaded', async () => {
  try {
    const [products, sales] = await Promise.all([
      api('/api/products'),
      api('/api/sales')
    ]);

    // 1. Cálculos de resumo no navegador (JavaScript)
    const completedSales = sales.filter(s => s.status === 'completed');
    const totalVendidoCents = completedSales.reduce((acc, s) => acc + s.total_cents, 0);
    const activeProducts = products.filter(p => p.active);
    const lowStockProducts = activeProducts.filter(p => p.stock <= p.min_stock);

    // Atualiza os cards
    document.getElementById('stat-total').textContent = money(totalVendidoCents);
    document.getElementById('stat-sales-count').textContent = completedSales.length;
    document.getElementById('stat-active-products').textContent = activeProducts.length;
    document.getElementById('stat-low-stock').textContent = lowStockProducts.length;

    // 2. Renderiza Alertas de Estoque Baixo
    const lowStockContainer = document.getElementById('low-stock-container');
    if (lowStockProducts.length === 0) {
      lowStockContainer.innerHTML = '<p class="empty-state">Nenhum produto com estoque crítico.</p>';
    } else {
      lowStockContainer.innerHTML = `
        <div class="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Produto</th>
                <th class="right">Estoque</th>
                <th class="right">Mínimo</th>
              </tr>
            </thead>
            <tbody>
              ${lowStockProducts.slice(0, 5).map(p => `
                <tr>
                  <td>
                    <strong>${escapeHTML(p.name)}</strong>
                    <div style="font-size:12px;color:#667085;">${escapeHTML(p.sku)}</div>
                  </td>
                  <td class="right" style="color:#d92d20;font-weight:600;">${p.stock} un.</td>
                  <td class="right">${p.min_stock} un.</td>
                </tr>
              `).join('')}
            </tbody>
          </table>
        </div>
      `;
    }

    // 3. Renderiza Vendas Recentes
    const recentSalesContainer = document.getElementById('recent-sales-container');
    if (sales.length === 0) {
      recentSalesContainer.innerHTML = '<p class="empty-state">Nenhuma venda registrada ainda.</p>';
    } else {
      recentSalesContainer.innerHTML = `
        <div class="table-wrap">
          <table>
            <thead>
              <tr>
                <th>Venda</th>
                <th>Cliente</th>
                <th class="right">Total</th>
                <th>Status</th>
              </tr>
            </thead>
            <tbody>
              ${sales.slice(0, 5).map(s => `
                <tr>
                  <td><strong>#${String(s.id).padStart(4, '0')}</strong></td>
                  <td>${escapeHTML(s.customer || 'Cliente balcão')}</td>
                  <td class="right" style="font-weight:600;">${money(s.total_cents)}</td>
                  <td>
                    <span class="badge ${s.status === 'cancelled' ? 'cancelled' : 'completed'}">
                      ${s.status === 'cancelled' ? 'Cancelada' : 'Concluída'}
                    </span>
                  </td>
                </tr>
              `).join('')}
            </tbody>
          </table>
        </div>
      `;
    }

  } catch (err) {
    toast('Erro ao carregar visão geral: ' + err.message, true);
  }
});
