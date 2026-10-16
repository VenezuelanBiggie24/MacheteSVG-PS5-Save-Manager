// MacheteSVG - Embedded Web Station Frontend
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

namespace machete::server
{

constexpr std::string_view kWebUiHtml = R"rawhtml(<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>MacheteSVG - PS5 Save Station</title>
  <style>
    :root {
      --bg: #0b0d14;
      --card-bg: rgba(22, 27, 46, 0.75);
      --card-border: rgba(99, 102, 241, 0.2);
      --card-hover: rgba(99, 102, 241, 0.35);
      --primary: #6366f1;
      --primary-hover: #4f46e5;
      --cyan: #06b6d4;
      --emerald: #10b981;
      --amber: #f59e0b;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --badge-ps5: #7c3aed;
      --badge-ps4: #2563eb;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: radial-gradient(circle at 50% 0%, #1e1b4b 0%, var(--bg) 60%);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      overflow-x: hidden;
    }
    header {
      background: rgba(11, 13, 20, 0.8);
      backdrop-filter: blur(16px);
      border-bottom: 1px solid var(--card-border);
      position: sticky;
      top: 0;
      z-index: 100;
      padding: 1rem 2rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 0.85rem;
    }
    .brand-icon {
      width: 42px;
      height: 42px;
      background: linear-gradient(135deg, var(--primary), var(--cyan));
      border-radius: 10px;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 0 20px rgba(99, 102, 241, 0.5);
    }
    .brand-title {
      font-size: 1.35rem;
      font-weight: 800;
      letter-spacing: -0.025em;
      background: linear-gradient(135deg, #ffffff 40%, var(--cyan) 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .brand-subtitle {
      font-size: 0.78rem;
      color: var(--text-muted);
      letter-spacing: 0.05em;
      text-transform: uppercase;
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 1rem;
    }
    .pill-active {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.4rem 0.85rem;
      border-radius: 9999px;
      background: rgba(16, 185, 129, 0.15);
      border: 1px solid rgba(16, 185, 129, 0.35);
      color: #34d399;
      font-size: 0.82rem;
      font-weight: 600;
    }
    .dot-live {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #10b981;
      box-shadow: 0 0 8px #10b981;
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(0.85); }
    }
    .btn {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.55rem 1.15rem;
      border-radius: 8px;
      font-size: 0.88rem;
      font-weight: 600;
      cursor: pointer;
      border: none;
      transition: all 0.2s ease;
      text-decoration: none;
      color: #fff;
    }
    .btn-primary {
      background: linear-gradient(135deg, var(--primary), #4338ca);
      box-shadow: 0 4px 15px rgba(99, 102, 241, 0.4);
    }
    .btn-primary:hover {
      background: linear-gradient(135deg, var(--primary-hover), #3730a3);
      box-shadow: 0 6px 20px rgba(99, 102, 241, 0.6);
      transform: translateY(-1px);
    }
    .btn-secondary {
      background: rgba(255, 255, 255, 0.08);
      border: 1px solid rgba(255, 255, 255, 0.15);
      color: var(--text);
    }
    .btn-secondary:hover {
      background: rgba(255, 255, 255, 0.14);
      transform: translateY(-1px);
    }
    .btn-sm {
      padding: 0.35rem 0.75rem;
      font-size: 0.8rem;
    }
    main {
      max-width: 1300px;
      margin: 0 auto;
      padding: 2rem;
      width: 100%;
      flex: 1;
    }
    .hero {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 1.25rem;
      margin-bottom: 2rem;
    }
    .metric-card {
      background: var(--card-bg);
      backdrop-filter: blur(12px);
      border: 1px solid var(--card-border);
      border-radius: 14px;
      padding: 1.25rem 1.5rem;
      display: flex;
      flex-direction: column;
      gap: 0.4rem;
    }
    .metric-title {
      font-size: 0.8rem;
      color: var(--text-muted);
      text-transform: uppercase;
      letter-spacing: 0.05em;
      font-weight: 600;
    }
    .metric-value {
      font-size: 1.75rem;
      font-weight: 800;
      color: #fff;
    }
    .toolbar {
      background: var(--card-bg);
      backdrop-filter: blur(12px);
      border: 1px solid var(--card-border);
      border-radius: 14px;
      padding: 1rem 1.5rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
      margin-bottom: 2rem;
    }
    .search-box {
      position: relative;
      flex: 1;
      max-width: 450px;
    }
    .search-input {
      width: 100%;
      padding: 0.65rem 1rem 0.65rem 2.6rem;
      background: rgba(11, 13, 20, 0.6);
      border: 1px solid var(--card-border);
      border-radius: 8px;
      color: #fff;
      font-size: 0.9rem;
      outline: none;
      transition: border-color 0.2s ease;
    }
    .search-input:focus {
      border-color: var(--cyan);
      box-shadow: 0 0 12px rgba(6, 182, 212, 0.3);
    }
    .search-icon {
      position: absolute;
      left: 0.9rem;
      top: 50%;
      transform: translateY(-50%);
      color: var(--text-muted);
      pointer-events: none;
    }
    .filter-pills {
      display: flex;
      gap: 0.5rem;
    }
    .filter-btn {
      padding: 0.45rem 0.95rem;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--card-border);
      border-radius: 6px;
      color: var(--text-muted);
      font-size: 0.82rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s ease;
    }
    .filter-btn.active {
      background: var(--primary);
      border-color: var(--primary);
      color: #fff;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(350px, 1fr));
      gap: 1.5rem;
    }
    .game-card {
      background: var(--card-bg);
      backdrop-filter: blur(12px);
      border: 1px solid var(--card-border);
      border-radius: 16px;
      padding: 1.25rem;
      display: flex;
      flex-direction: column;
      gap: 1rem;
      transition: all 0.25s ease;
      position: relative;
      overflow: hidden;
    }
    .game-card:hover {
      border-color: var(--card-hover);
      transform: translateY(-3px);
      box-shadow: 0 10px 25px rgba(0, 0, 0, 0.4);
    }
    .game-header {
      display: flex;
      gap: 1rem;
      align-items: center;
    }
    .game-icon {
      width: 72px;
      height: 72px;
      border-radius: 12px;
      object-fit: cover;
      background: #1e1b4b;
      border: 1px solid rgba(255, 255, 255, 0.1);
      flex-shrink: 0;
    }
    .game-meta {
      flex: 1;
      min-width: 0;
    }
    .game-badge-row {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-bottom: 0.35rem;
    }
    .badge {
      font-size: 0.72rem;
      font-weight: 700;
      padding: 0.15rem 0.5rem;
      border-radius: 4px;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }
    .badge-ps5 { background: var(--badge-ps5); color: #fff; }
    .badge-ps4 { background: var(--badge-ps4); color: #fff; }
    .badge-id {
      background: rgba(255, 255, 255, 0.08);
      color: var(--cyan);
      font-family: monospace;
      font-size: 0.75rem;
    }
    .game-title {
      font-size: 1.05rem;
      font-weight: 700;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      color: #fff;
    }
    .game-save-info {
      display: flex;
      justify-content: space-between;
      font-size: 0.82rem;
      color: var(--text-muted);
      border-top: 1px solid rgba(255, 255, 255, 0.06);
      padding-top: 0.75rem;
    }
    .game-actions {
      display: flex;
      gap: 0.75rem;
    }
    .game-actions .btn {
      flex: 1;
      justify-content: center;
    }
    /* Modal */
    .modal-backdrop {
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.75);
      backdrop-filter: blur(8px);
      z-index: 200;
      display: none;
      align-items: center;
      justify-content: center;
      padding: 1rem;
    }
    .modal-backdrop.open { display: flex; }
    .modal {
      background: #131726;
      border: 1px solid var(--card-border);
      border-radius: 18px;
      width: 100%;
      max-width: 650px;
      max-height: 85vh;
      display: flex;
      flex-direction: column;
      box-shadow: 0 20px 50px rgba(0, 0, 0, 0.6);
      overflow: hidden;
    }
    .modal-header {
      padding: 1.25rem 1.5rem;
      border-bottom: 1px solid var(--card-border);
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .modal-body {
      padding: 1.5rem;
      overflow-y: auto;
      flex: 1;
    }
    .file-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.75rem 1rem;
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid rgba(255, 255, 255, 0.06);
      border-radius: 8px;
      margin-bottom: 0.5rem;
      transition: background 0.15s ease;
    }
    .file-item:hover {
      background: rgba(255, 255, 255, 0.07);
    }
    .file-info {
      display: flex;
      flex-direction: column;
      gap: 0.2rem;
      min-width: 0;
    }
    .file-name {
      font-family: monospace;
      font-size: 0.88rem;
      font-weight: 600;
      color: #fff;
      word-break: break-all;
    }
    .file-size {
      font-size: 0.75rem;
      color: var(--text-muted);
    }
    .empty-state {
      text-align: center;
      padding: 4rem 1rem;
      color: var(--text-muted);
    }
  </style>
</head>
<body>
  <header>
    <div class="brand">
      <div class="brand-icon">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
          <polygon points="12 2 2 7 12 12 22 7 12 2"></polygon>
          <polyline points="2 17 12 22 22 17"></polyline>
          <polyline points="2 12 12 17 22 12"></polyline>
        </svg>
      </div>
      <div>
        <div class="brand-title">MacheteSVG</div>
        <div class="brand-subtitle">PS5 Save Station &bull; Local Web Server</div>
      </div>
    </div>
    <div class="header-actions">
      <div class="pill-active">
        <div class="dot-live"></div>
        <span id="console-ip">Conectando...</span>
      </div>
      <a href="/api/download_all" class="btn btn-primary">
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
          <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
          <polyline points="7 10 12 15 17 10"></polyline>
          <line x1="12" y1="15" x2="12" y2="3"></line>
        </svg>
        Descargar Todo (.zip)
      </a>
    </div>
  </header>

  <main>
    <div class="hero">
      <div class="metric-card">
        <div class="metric-title">Juegos Instalados</div>
        <div class="metric-value" id="stat-games">0</div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Partidas Detectadas</div>
        <div class="metric-value" id="stat-saves">0</div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Espacio en Partidas</div>
        <div class="metric-value" id="stat-size">0 MB</div>
      </div>
      <div class="metric-card">
        <div class="metric-title">Puerto Local</div>
        <div class="metric-value" id="stat-port">8383</div>
      </div>
    </div>

    <div class="toolbar">
      <div class="search-box">
        <svg class="search-icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <circle cx="11" cy="11" r="8"></circle>
          <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
        </svg>
        <input type="text" id="search" class="search-input" placeholder="Buscar por título o Title ID (ej. Spider-Man, PPSA01411)...">
      </div>
      <div class="filter-pills">
        <button class="filter-btn active" data-filter="all">Todos</button>
        <button class="filter-btn" data-filter="PS5">PS5</button>
        <button class="filter-btn" data-filter="PS4">PS4</button>
      </div>
    </div>

    <div id="games-grid" class="grid">
      <!-- Se inyectan las tarjetas de juego dinámicamente -->
    </div>

    <div id="empty-state" class="empty-state" style="display: none;">
      <h3>No se encontraron juegos o partidas</h3>
      <p style="margin-top: 0.5rem;">Prueba con otro término de búsqueda o asegúrate de que tus juegos estén instalados.</p>
    </div>
  </main>

  <!-- Modal de Exploración de Archivos Individuales -->
  <div id="modal" class="modal-backdrop">
    <div class="modal">
      <div class="modal-header">
        <div>
          <h3 id="modal-title" style="color: #fff;">Archivos de la Partida</h3>
          <p id="modal-subtitle" style="font-size: 0.78rem; color: var(--text-muted); font-family: monospace;"></p>
        </div>
        <button id="modal-close" class="btn btn-secondary btn-sm">&times; Cerrar</button>
      </div>
      <div id="modal-body" class="modal-body">
        <!-- Lista de archivos -->
      </div>
    </div>
  </div>

  <script>
    let gamesData = [];
    let currentFilter = 'all';

    async function loadStatus() {
      try {
        const res = await fetch('/api/status');
        if (res.ok) {
          const data = await res.json();
          document.getElementById('console-ip').textContent = `${data.ip}:${data.port}`;
          document.getElementById('stat-port').textContent = data.port;
        }
      } catch (e) {
        document.getElementById('console-ip').textContent = 'Conectado (LAN)';
      }
    }

    async function loadGames() {
      try {
        const res = await fetch('/api/games');
        if (!res.ok) return;
        gamesData = await res.json();
        renderStats();
        renderGames();
      } catch (e) {
        console.error('Error cargando juegos:', e);
      }
    }

    function renderStats() {
      document.getElementById('stat-games').textContent = gamesData.length;
      let savesCount = 0;
      let totalBytes = 0;
      for (const g of gamesData) {
        if (g.has_save) savesCount++;
        totalBytes += (g.size_bytes || 0);
      }
      document.getElementById('stat-saves').textContent = savesCount;
      const mb = (totalBytes / (1024 * 1024)).toFixed(1);
      document.getElementById('stat-size').textContent = `${mb} MB`;
    }

    function renderGames() {
      const query = document.getElementById('search').value.toLowerCase().trim();
      const grid = document.getElementById('games-grid');
      grid.innerHTML = '';

      const filtered = gamesData.filter(g => {
        const matchFilter = (currentFilter === 'all') || (g.platform === currentFilter);
        const matchQuery = !query || g.title.toLowerCase().includes(query) || g.title_id.toLowerCase().includes(query);
        return matchFilter && matchQuery;
      });

      if (filtered.length === 0) {
        document.getElementById('empty-state').style.display = 'block';
        return;
      }
      document.getElementById('empty-state').style.display = 'none';

      for (const g of filtered) {
        const card = document.createElement('div');
        card.className = 'game-card';
        card.innerHTML = `
          <div class="game-header">
            <img class="game-icon" src="/api/icon/${encodeURIComponent(g.title_id)}" alt="" onerror="this.src='data:image/svg+xml;utf8,<svg xmlns=\\'http://www.w3.org/2000/svg\\' width=\\'72\\' height=\\'72\\'><rect width=\\'100%\\' height=\\'100%\\' fill=\\'%231e1b4b\\'/><text x=\\'50%\\' y=\\'55%\\' fill=\\'%236366f1\\' font-size=\\'24\\' font-family=\\'sans-serif\\' font-weight=\\'bold\\' text-anchor=\\'middle\\'>PS</text></svg>'">
            <div class="game-meta">
              <div class="game-badge-row">
                <span class="badge ${g.platform === 'PS5' ? 'badge-ps5' : 'badge-ps4'}">${g.platform}</span>
                <span class="badge badge-id">${g.title_id}</span>
              </div>
              <div class="game-title" title="${escapeHtml(g.title)}">${escapeHtml(g.title)}</div>
            </div>
          </div>
          <div class="game-save-info">
            <span>Partida: <strong>${g.has_save ? (g.size_str || 'Detectada') : 'Sin Partida'}</strong></span>
            <span>${g.timestamp_str || ''}</span>
          </div>
          <div class="game-actions">
            ${g.has_save ? `
              <a href="/api/download/${encodeURIComponent(g.title_id)}" class="btn btn-primary btn-sm">
                <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                  <polyline points="7 10 12 15 17 10"></polyline>
                  <line x1="12" y1="15" x2="12" y2="3"></line>
                </svg>
                Descargar (.zip)
              </a>
              <button class="btn btn-secondary btn-sm" onclick="openFilesModal('${encodeURIComponent(g.title_id)}', '${escapeAttr(g.title)}')">
                <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path>
                </svg>
                Archivos
              </button>
            ` : `<button class="btn btn-secondary btn-sm" disabled style="opacity: 0.4;">Sin datos de guardado</button>`}
          </div>
        `;
        grid.appendChild(card);
      }
    }

    async function openFilesModal(titleId, titleName) {
      const modal = document.getElementById('modal');
      const modalTitle = document.getElementById('modal-title');
      const modalSubtitle = document.getElementById('modal-subtitle');
      const modalBody = document.getElementById('modal-body');

      modalTitle.textContent = titleName;
      modalSubtitle.textContent = titleId;
      modalBody.innerHTML = '<p style="color: var(--text-muted); text-align: center; padding: 2rem;">Cargando lista de archivos...</p>';
      modal.classList.add('open');

      try {
        const res = await fetch(`/api/files/${encodeURIComponent(titleId)}`);
        if (!res.ok) {
          modalBody.innerHTML = '<p style="color: #ef4444; text-align: center;">Error al leer los archivos de la partida.</p>';
          return;
        }
        const files = await res.json();
        if (files.length === 0) {
          modalBody.innerHTML = '<p style="color: var(--text-muted); text-align: center;">No hay archivos individuales para mostrar.</p>';
          return;
        }

        modalBody.innerHTML = '';
        for (const f of files) {
          const item = document.createElement('div');
          item.className = 'file-item';
          item.innerHTML = `
            <div class="file-info">
              <span class="file-name">${escapeHtml(f.name)}</span>
              <span class="file-size">${f.size_formatted}</span>
            </div>
            <a href="/api/file/${encodeURIComponent(titleId)}/${encodeURIComponent(f.name)}" class="btn btn-secondary btn-sm">
              <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                <polyline points="7 10 12 15 17 10"></polyline>
                <line x1="12" y1="15" x2="12" y2="3"></line>
              </svg>
              Descargar
            </a>
          `;
          modalBody.appendChild(item);
        }
      } catch (e) {
        modalBody.innerHTML = '<p style="color: #ef4444; text-align: center;">Error de conexión.</p>';
      }
    }

    document.getElementById('modal-close').addEventListener('click', () => {
      document.getElementById('modal').classList.remove('open');
    });

    document.getElementById('modal').addEventListener('click', (e) => {
      if (e.target.id === 'modal') {
        document.getElementById('modal').classList.remove('open');
      }
    });

    document.getElementById('search').addEventListener('input', renderGames);

    document.querySelectorAll('.filter-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        currentFilter = btn.dataset.filter;
        renderGames();
      });
    });

    function escapeHtml(str) {
      if (!str) return '';
      return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
    }
    function escapeAttr(str) {
      if (!str) return '';
      return str.replace(/'/g, "\\'");
    }

    loadStatus();
    loadGames();
  </script>
</body>
</html>
)rawhtml";

} // namespace machete::server
