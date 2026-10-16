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
      --bg: #07090e;
      --bg-surface: rgba(14, 18, 30, 0.72);
      --bg-surface-elevated: rgba(22, 28, 48, 0.85);
      --border: rgba(99, 102, 241, 0.18);
      --border-hover: rgba(99, 102, 241, 0.45);
      --primary: #6366f1;
      --primary-hover: #4f46e5;
      --primary-glow: rgba(99, 102, 241, 0.35);
      --cyan: #06b6d4;
      --cyan-glow: rgba(6, 182, 212, 0.35);
      --emerald: #10b981;
      --emerald-glow: rgba(16, 185, 129, 0.3);
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --text-dim: #64748b;
      --badge-ps5: #7c3aed;
      --badge-ps4: #1d4ed8;
      --radius-sm: 8px;
      --radius-md: 14px;
      --radius-lg: 20px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      background: #07090e radial-gradient(circle at 50% 0%, #151936 0%, #07090e 65%);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      overflow-x: hidden;
      -webkit-font-smoothing: antialiased;
    }
    header {
      background: rgba(7, 9, 14, 0.82);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      border-bottom: 1px solid var(--border);
      position: sticky;
      top: 0;
      z-index: 100;
      padding: 0.85rem 2rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 1.25rem;
      flex-wrap: wrap;
    }
    .brand {
      display: flex;
      align-items: center;
      gap: 0.9rem;
      text-decoration: none;
    }
    .brand-icon {
      width: 44px;
      height: 44px;
      background: linear-gradient(135deg, var(--primary) 0%, var(--cyan) 100%);
      border-radius: var(--radius-sm);
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 0 24px var(--primary-glow);
      flex-shrink: 0;
    }
    .brand-title {
      font-size: 1.35rem;
      font-weight: 800;
      letter-spacing: -0.03em;
      background: linear-gradient(135deg, #ffffff 40%, var(--cyan) 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      display: flex;
      align-items: center;
      gap: 0.6rem;
    }
    .version-tag {
      font-size: 0.72rem;
      font-weight: 700;
      letter-spacing: 0.05em;
      padding: 0.15rem 0.55rem;
      border-radius: 9999px;
      background: rgba(99, 102, 241, 0.15);
      border: 1px solid rgba(99, 102, 241, 0.35);
      color: #a5b4fc;
      -webkit-text-fill-color: #a5b4fc;
    }
    .brand-subtitle {
      font-size: 0.76rem;
      color: var(--text-muted);
      letter-spacing: 0.03em;
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 0.85rem;
      flex-wrap: wrap;
    }
    .status-pill {
      display: inline-flex;
      align-items: center;
      gap: 0.55rem;
      padding: 0.45rem 0.95rem;
      border-radius: 9999px;
      background: rgba(16, 185, 129, 0.12);
      border: 1px solid rgba(16, 185, 129, 0.35);
      color: #34d399;
      font-size: 0.84rem;
      font-weight: 600;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #10b981;
      box-shadow: 0 0 10px #10b981;
      animation: pulse 2s infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.5; transform: scale(0.85); }
    }
    .btn {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      padding: 0.55rem 1.15rem;
      border-radius: var(--radius-sm);
      font-size: 0.86rem;
      font-weight: 600;
      cursor: pointer;
      border: none;
      transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
      text-decoration: none;
      white-space: nowrap;
    }
    .btn svg {
      width: 16px;
      height: 16px;
      stroke-width: 2.2;
      flex-shrink: 0;
    }
    .btn-primary {
      background: linear-gradient(135deg, var(--primary) 0%, #4338ca 100%);
      color: #ffffff;
      box-shadow: 0 4px 16px var(--primary-glow);
      border: 1px solid rgba(255, 255, 255, 0.12);
    }
    .btn-primary:hover {
      background: linear-gradient(135deg, #4f46e5 0%, #3730a3 100%);
      box-shadow: 0 6px 20px rgba(99, 102, 241, 0.5);
      transform: translateY(-1px);
    }
    .btn-secondary {
      background: rgba(255, 255, 255, 0.06);
      color: var(--text);
      border: 1px solid rgba(255, 255, 255, 0.12);
    }
    .btn-secondary:hover {
      background: rgba(255, 255, 255, 0.12);
      border-color: rgba(255, 255, 255, 0.24);
      transform: translateY(-1px);
    }
    .btn-sm {
      padding: 0.35rem 0.75rem;
      font-size: 0.78rem;
    }
    main {
      flex: 1;
      max-width: 1400px;
      width: 100%;
      margin: 0 auto;
      padding: 2rem 2rem 3rem 2rem;
    }
    .hero-stats {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
      gap: 1.25rem;
      margin-bottom: 2rem;
    }
    .stat-card {
      background: var(--bg-surface);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--border);
      border-radius: var(--radius-md);
      padding: 1.25rem 1.5rem;
      display: flex;
      align-items: center;
      gap: 1.15rem;
      transition: all 0.25s ease;
    }
    .stat-card:hover {
      border-color: var(--border-hover);
      transform: translateY(-2px);
      box-shadow: 0 8px 24px rgba(0, 0, 0, 0.3);
    }
    .stat-icon {
      width: 46px;
      height: 46px;
      border-radius: var(--radius-sm);
      display: flex;
      align-items: center;
      justify-content: center;
      flex-shrink: 0;
    }
    .stat-icon svg {
      width: 22px;
      height: 22px;
      stroke-width: 2.2;
    }
    .stat-icon.indigo {
      background: rgba(99, 102, 241, 0.15);
      color: #818cf8;
      border: 1px solid rgba(99, 102, 241, 0.3);
    }
    .stat-icon.emerald {
      background: rgba(16, 185, 129, 0.15);
      color: #34d399;
      border: 1px solid rgba(16, 185, 129, 0.3);
    }
    .stat-icon.cyan {
      background: rgba(6, 182, 212, 0.15);
      color: #38bdf8;
      border: 1px solid rgba(6, 182, 212, 0.3);
    }
    .stat-icon.amber {
      background: rgba(245, 158, 11, 0.15);
      color: #fbbf24;
      border: 1px solid rgba(245, 158, 11, 0.3);
    }
    .stat-info h4 {
      font-size: 0.78rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--text-muted);
      margin-bottom: 0.25rem;
    }
    .stat-info p {
      font-size: 1.45rem;
      font-weight: 800;
      color: var(--text);
      line-height: 1.2;
    }
    .toolbar {
      background: var(--bg-surface);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--border);
      border-radius: var(--radius-md);
      padding: 0.85rem 1.25rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 1rem;
      flex-wrap: wrap;
      margin-bottom: 2rem;
    }
    .search-box {
      position: relative;
      flex: 1;
      max-width: 480px;
      min-width: 240px;
    }
    .search-input {
      width: 100%;
      padding: 0.65rem 1rem 0.65rem 2.65rem;
      background: rgba(7, 9, 14, 0.65);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      color: var(--text);
      font-size: 0.9rem;
      outline: none;
      transition: all 0.2s ease;
    }
    .search-input:focus {
      border-color: var(--cyan);
      box-shadow: 0 0 16px var(--cyan-glow);
    }
    .search-icon {
      position: absolute;
      left: 0.85rem;
      top: 50%;
      transform: translateY(-50%);
      color: var(--text-dim);
      pointer-events: none;
      width: 18px;
      height: 18px;
    }
    .search-clear {
      position: absolute;
      right: 0.75rem;
      top: 50%;
      transform: translateY(-50%);
      background: none;
      border: none;
      color: var(--text-dim);
      cursor: pointer;
      display: none;
      padding: 2px;
    }
    .search-clear:hover { color: var(--text); }
    .filter-pills {
      display: flex;
      gap: 0.5rem;
      flex-wrap: wrap;
    }
    .filter-btn {
      padding: 0.45rem 1rem;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border);
      border-radius: var(--radius-sm);
      color: var(--text-muted);
      font-size: 0.84rem;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s ease;
      display: inline-flex;
      align-items: center;
      gap: 0.45rem;
    }
    .filter-btn:hover {
      background: rgba(255, 255, 255, 0.08);
      color: var(--text);
    }
    .filter-btn.active {
      background: var(--primary);
      border-color: var(--primary);
      color: #ffffff;
      box-shadow: 0 2px 10px var(--primary-glow);
    }
    .filter-badge {
      font-size: 0.72rem;
      padding: 0.1rem 0.4rem;
      border-radius: 9999px;
      background: rgba(0, 0, 0, 0.25);
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(360px, 1fr));
      gap: 1.5rem;
    }
    .game-card {
      background: var(--bg-surface);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--border);
      border-radius: var(--radius-md);
      padding: 1.35rem;
      display: flex;
      flex-direction: column;
      gap: 1.15rem;
      transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1);
      position: relative;
      overflow: hidden;
    }
    .game-card:hover {
      border-color: var(--border-hover);
      transform: translateY(-3px);
      box-shadow: 0 12px 32px rgba(0, 0, 0, 0.45);
    }
    .game-header {
      display: flex;
      gap: 1.15rem;
      align-items: flex-start;
    }
    .game-icon-wrap {
      width: 76px;
      height: 76px;
      border-radius: 12px;
      background: #111528;
      border: 1px solid rgba(255, 255, 255, 0.1);
      overflow: hidden;
      flex-shrink: 0;
      box-shadow: 0 4px 12px rgba(0, 0, 0, 0.35);
      position: relative;
    }
    .game-icon {
      width: 100%;
      height: 100%;
      object-fit: cover;
      display: block;
    }
    .game-icon-fallback {
      width: 100%;
      height: 100%;
      display: flex;
      align-items: center;
      justify-content: center;
      color: var(--text-dim);
    }
    .game-icon-fallback svg {
      width: 32px;
      height: 32px;
    }
    .game-meta {
      flex: 1;
      min-width: 0;
    }
    .game-badge-row {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-bottom: 0.4rem;
      flex-wrap: wrap;
    }
    .badge {
      font-size: 0.72rem;
      font-weight: 700;
      padding: 0.18rem 0.55rem;
      border-radius: 4px;
      text-transform: uppercase;
      letter-spacing: 0.05em;
    }
    .badge-ps5 {
      background: var(--badge-ps5);
      color: #ffffff;
      box-shadow: 0 0 10px rgba(124, 58, 237, 0.4);
    }
    .badge-ps4 {
      background: var(--badge-ps4);
      color: #ffffff;
      box-shadow: 0 0 10px rgba(29, 78, 216, 0.4);
    }
    .title-id-badge {
      font-size: 0.72rem;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      font-weight: 600;
      color: var(--text-muted);
      background: rgba(255, 255, 255, 0.06);
      padding: 0.18rem 0.55rem;
      border-radius: 4px;
      border: 1px solid rgba(255, 255, 255, 0.08);
    }
    .game-title {
      font-size: 1.05rem;
      font-weight: 700;
      color: #ffffff;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
      margin-bottom: 0.35rem;
    }
    .game-save-meta {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.8rem;
      color: var(--text-muted);
    }
    .save-indicator {
      width: 7px;
      height: 7px;
      border-radius: 50%;
    }
    .save-indicator.active {
      background: #10b981;
      box-shadow: 0 0 8px #10b981;
    }
    .save-indicator.inactive {
      background: var(--text-dim);
    }
    .game-actions {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 0.75rem;
      margin-top: auto;
      border-top: 1px solid rgba(255, 255, 255, 0.06);
      padding-top: 1rem;
    }
    .game-actions .btn {
      justify-content: center;
      width: 100%;
    }
    .empty-state {
      text-align: center;
      padding: 5rem 2rem;
      background: var(--bg-surface);
      border: 1px dashed var(--border);
      border-radius: var(--radius-md);
      grid-column: 1 / -1;
    }
    .empty-state svg {
      width: 64px;
      height: 64px;
      color: var(--text-dim);
      margin-bottom: 1.25rem;
    }
    .empty-state h3 {
      font-size: 1.25rem;
      font-weight: 700;
      color: #ffffff;
      margin-bottom: 0.5rem;
    }
    .empty-state p {
      font-size: 0.9rem;
      color: var(--text-muted);
      max-width: 500px;
      margin: 0 auto;
    }
    .modal-backdrop {
      position: fixed;
      inset: 0;
      background: rgba(0, 0, 0, 0.78);
      backdrop-filter: blur(20px);
      -webkit-backdrop-filter: blur(20px);
      z-index: 1000;
      display: none;
      align-items: center;
      justify-content: center;
      padding: 1.5rem;
      opacity: 0;
      transition: opacity 0.25s ease;
    }
    .modal-backdrop.open {
      display: flex;
      opacity: 1;
    }
    .modal-card {
      background: var(--bg-surface-elevated);
      border: 1px solid var(--border-hover);
      border-radius: var(--radius-lg);
      box-shadow: 0 24px 64px rgba(0, 0, 0, 0.8), 0 0 32px var(--primary-glow);
      max-width: 680px;
      width: 100%;
      max-height: 85vh;
      display: flex;
      flex-direction: column;
      overflow: hidden;
      animation: modalSlide 0.25s cubic-bezier(0.16, 1, 0.3, 1);
    }
    @keyframes modalSlide {
      from { transform: scale(0.95) translateY(10px); }
      to { transform: scale(1) translateY(0); }
    }
    .modal-header {
      padding: 1.5rem 1.75rem;
      border-bottom: 1px solid rgba(255, 255, 255, 0.08);
      display: flex;
      justify-content: space-between;
      align-items: flex-start;
      gap: 1rem;
    }
    .modal-header h3 {
      font-size: 1.25rem;
      font-weight: 800;
      color: #ffffff;
      margin-bottom: 0.25rem;
    }
    .modal-header p {
      font-size: 0.8rem;
      color: var(--text-muted);
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
    }
    .modal-close {
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid rgba(255, 255, 255, 0.12);
      border-radius: var(--radius-sm);
      color: var(--text-muted);
      cursor: pointer;
      padding: 0.4rem;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.2s ease;
    }
    .modal-close:hover {
      background: rgba(255, 255, 255, 0.15);
      color: #ffffff;
    }
    .modal-close svg { width: 18px; height: 18px; }
    .modal-body {
      padding: 1.5rem 1.75rem;
      overflow-y: auto;
      flex: 1;
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
    }
    .file-item {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.85rem 1.15rem;
      background: rgba(7, 9, 14, 0.5);
      border: 1px solid rgba(255, 255, 255, 0.06);
      border-radius: var(--radius-sm);
      gap: 1rem;
      transition: all 0.2s ease;
    }
    .file-item:hover {
      background: rgba(255, 255, 255, 0.04);
      border-color: rgba(99, 102, 241, 0.35);
    }
    .file-name-wrap {
      display: flex;
      align-items: center;
      gap: 0.75rem;
      min-width: 0;
    }
    .file-icon {
      color: var(--cyan);
      display: flex;
      align-items: center;
      flex-shrink: 0;
    }
    .file-icon svg { width: 18px; height: 18px; }
    .file-name {
      font-size: 0.88rem;
      font-weight: 600;
      font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
      color: #ffffff;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    .file-size {
      font-size: 0.8rem;
      color: var(--text-muted);
      white-space: nowrap;
    }
    .toast-container {
      position: fixed;
      bottom: 2rem;
      right: 2rem;
      z-index: 2000;
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
      pointer-events: none;
    }
    .toast {
      background: rgba(14, 18, 32, 0.95);
      backdrop-filter: blur(16px);
      -webkit-backdrop-filter: blur(16px);
      border: 1px solid var(--border-hover);
      box-shadow: 0 12px 32px rgba(0, 0, 0, 0.6);
      border-radius: var(--radius-sm);
      padding: 0.75rem 1.25rem;
      color: #ffffff;
      font-size: 0.86rem;
      font-weight: 600;
      display: flex;
      align-items: center;
      gap: 0.65rem;
      pointer-events: auto;
      animation: toastIn 0.25s cubic-bezier(0.16, 1, 0.3, 1);
    }
    .toast.success { border-color: rgba(16, 185, 129, 0.6); }
    .toast.success svg { color: #10b981; }
    @keyframes toastIn {
      from { transform: translateY(20px); opacity: 0; }
      to { transform: translateY(0); opacity: 1; }
    }
    footer {
      border-top: 1px solid var(--border);
      padding: 1.5rem 2rem;
      text-align: center;
      font-size: 0.8rem;
      color: var(--text-dim);
    }
    @media (max-width: 768px) {
      header { padding: 0.85rem 1rem; }
      main { padding: 1.25rem 1rem 2.5rem 1rem; }
      .grid { grid-template-columns: 1fr; }
      .game-actions { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>

  <!-- Top Header Navigation -->
  <header>
    <a href="/" class="brand">
      <div class="brand-icon">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
          <rect x="2" y="4" width="20" height="16" rx="4"></rect>
          <path d="M6 12h4"></path>
          <path d="M8 10v4"></path>
          <circle cx="15" cy="11" r="1"></circle>
          <circle cx="17" cy="13" r="1"></circle>
        </svg>
      </div>
      <div>
        <div class="brand-title">
          <span>MacheteSVG</span>
          <span class="version-tag">PPSA99001 v1.0.0</span>
        </div>
        <div class="brand-subtitle">Estación Web Local & Gestor de Partidas PS5 / PS4</div>
      </div>
    </a>

    <div class="header-actions">
      <div class="status-pill" id="status-pill">
        <span class="status-dot"></span>
        <span id="console-ip">Conectando...</span>
      </div>
      <button class="btn btn-secondary btn-sm" id="btn-copy-url" title="Copiar enlace de conexión al portapapeles">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
          <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
          <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"></path>
        </svg>
        <span id="copy-url-text">Copiar URL</span>
      </button>
      <a href="/api/download_all" class="btn btn-primary btn-sm" id="btn-download-all">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
          <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
          <polyline points="7 10 12 15 17 10"></polyline>
          <line x1="12" y1="15" x2="12" y2="3"></line>
        </svg>
        <span>Descargar Todo (ZIP)</span>
      </a>
    </div>
  </header>

  <!-- Main Content Dashboard -->
  <main>
    <!-- Metrics Overview -->
    <div class="hero-stats">
      <div class="stat-card">
        <div class="stat-icon indigo">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <rect x="2" y="7" width="20" height="14" rx="2" ry="2"></rect>
            <path d="M16 21V5a2 2 0 0 0-2-2h-4a2 2 0 0 0-2 2v16"></path>
          </svg>
        </div>
        <div class="stat-info">
          <h4>Juegos Instalados</h4>
          <p id="stat-games">0</p>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon emerald">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path>
            <polyline points="17 21 17 13 7 13 7 21"></polyline>
            <polyline points="7 3 7 8 15 8"></polyline>
          </svg>
        </div>
        <div class="stat-info">
          <h4>Partidas con Respaldo</h4>
          <p id="stat-saves">0</p>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon cyan">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <ellipse cx="12" cy="5" rx="9" ry="3"></ellipse>
            <path d="M21 12c0 1.66-4 3-9 3s-9-1.34-9-3"></path>
            <path d="M3 5v14c0 1.66 4 3 9 3s9-1.34 9-3V5"></path>
          </svg>
        </div>
        <div class="stat-info">
          <h4>Espacio de Partidas</h4>
          <p id="stat-size">0 MB</p>
        </div>
      </div>

      <div class="stat-card">
        <div class="stat-icon amber">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <circle cx="12" cy="12" r="10"></circle>
            <line x1="2" y1="12" x2="22" y2="12"></line>
            <path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"></path>
          </svg>
        </div>
        <div class="stat-info">
          <h4>Puerto del Servidor</h4>
          <p id="stat-port">8383</p>
        </div>
      </div>
    </div>

    <!-- Search & Filter Controls -->
    <div class="toolbar">
      <div class="search-box">
        <svg class="search-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor">
          <circle cx="11" cy="11" r="8"></circle>
          <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
        </svg>
        <input type="text" id="search" class="search-input" placeholder="Buscar por título o Title ID (ej. Spider-Man, PPSA01411)... [Presiona /]">
        <button id="search-clear" class="search-clear" title="Limpiar búsqueda">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <line x1="18" y1="6" x2="6" y2="18"></line>
            <line x1="6" y1="6" x2="18" y2="18"></line>
          </svg>
        </button>
      </div>

      <div class="filter-pills">
        <button class="filter-btn active" data-filter="all">
          <span>Todos</span>
          <span class="filter-badge" id="badge-all">0</span>
        </button>
        <button class="filter-btn" data-filter="PS5">
          <span>PlayStation 5</span>
          <span class="filter-badge" id="badge-ps5">0</span>
        </button>
        <button class="filter-btn" data-filter="PS4">
          <span>PlayStation 4</span>
          <span class="filter-badge" id="badge-ps4">0</span>
        </button>
      </div>
    </div>

    <!-- Games Grid -->
    <div id="games-grid" class="grid">
      <!-- Inyectado dinámicamente -->
    </div>

    <!-- Empty State -->
    <div id="empty-state" class="empty-state" style="display: none;">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
        <circle cx="11" cy="11" r="8"></circle>
        <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
      </svg>
      <h3>No se encontraron coincidencias</h3>
      <p>Intenta con otro término de búsqueda o cambia el filtro de plataforma.</p>
    </div>
  </main>

  <!-- Modal Explorador de Archivos -->
  <div id="modal" class="modal-backdrop">
    <div class="modal-card">
      <div class="modal-header">
        <div>
          <h3 id="modal-title">Explorador de Archivos</h3>
          <p id="modal-subtitle">PPSA00000</p>
        </div>
        <button id="modal-close" class="modal-close" title="Cerrar ventana">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <line x1="18" y1="6" x2="6" y2="18"></line>
            <line x1="6" y1="6" x2="18" y2="18"></line>
          </svg>
        </button>
      </div>
      <div id="modal-body" class="modal-body">
        <!-- Inyectado dinámicamente -->
      </div>
    </div>
  </div>

  <!-- Toast Notification Container -->
  <div id="toast-container" class="toast-container"></div>

  <!-- Footer -->
  <footer>
    MacheteSVG • Diseñado para PlayStation 5 con Jailbreak • Arquitectura Cero Corrupción
  </footer>

  <script>
    let gamesData = [];
    let currentFilter = 'all';

    function showToast(message, type = 'success') {
      const container = document.getElementById('toast-container');
      const toast = document.createElement('div');
      toast.className = `toast ${type}`;
      toast.innerHTML = `
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5">
          <polyline points="20 6 9 17 4 12"></polyline>
        </svg>
        <span>${escapeHtml(message)}</span>
      `;
      container.appendChild(toast);
      setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateY(10px)';
        toast.style.transition = 'all 0.25s ease';
        setTimeout(() => toast.remove(), 250);
      }, 3000);
    }

    async function loadStatus() {
      try {
        const res = await fetch('/api/status');
        if (res.ok) {
          const data = await res.json();
          document.getElementById('console-ip').textContent = `${data.ip}:${data.port}`;
          document.getElementById('stat-port').textContent = data.port;
        }
      } catch (e) {
        document.getElementById('console-ip').textContent = 'Conectado (LAN :8383)';
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
        console.error('Error cargando títulos:', e);
      }
    }

    function renderStats() {
      document.getElementById('stat-games').textContent = gamesData.length;
      let savesCount = 0;
      let totalBytes = 0;
      let ps5Count = 0;
      let ps4Count = 0;

      for (const g of gamesData) {
        if (g.has_save) savesCount++;
        totalBytes += (g.size_bytes || 0);
        if (g.platform === 'PS5') ps5Count++;
        if (g.platform === 'PS4') ps4Count++;
      }

      document.getElementById('stat-saves').textContent = savesCount;
      document.getElementById('badge-all').textContent = gamesData.length;
      document.getElementById('badge-ps5').textContent = ps5Count;
      document.getElementById('badge-ps4').textContent = ps4Count;

      const mb = (totalBytes / (1024 * 1024)).toFixed(1);
      document.getElementById('stat-size').textContent = totalBytes >= (1024 * 1024 * 1024)
        ? `${(totalBytes / (1024 * 1024 * 1024)).toFixed(2)} GB`
        : `${mb} MB`;
    }

    function renderGames() {
      const query = document.getElementById('search').value.toLowerCase().trim();
      const grid = document.getElementById('games-grid');
      const emptyState = document.getElementById('empty-state');
      grid.innerHTML = '';

      let visible = 0;

      for (const g of gamesData) {
        if (currentFilter !== 'all' && g.platform !== currentFilter) continue;
        if (query && !g.title.toLowerCase().includes(query) && !g.title_id.toLowerCase().includes(query)) continue;

        visible++;
        const card = document.createElement('div');
        card.className = 'game-card';

        const platformBadgeClass = g.platform === 'PS5' ? 'badge-ps5' : 'badge-ps4';
        const saveIndicatorClass = g.has_save ? 'active' : 'inactive';
        const saveStatusText = g.has_save ? `Partida: ${g.size_str}` : 'Sin datos guardados';

        card.innerHTML = `
          <div class="game-header">
            <div class="game-icon-wrap">
              <img src="/api/icon/${encodeURIComponent(g.title_id)}"
                   class="game-icon"
                   alt="${escapeHtml(g.title)}"
                   loading="lazy"
                   onerror="this.style.display='none'; this.nextElementSibling.style.display='flex';">
              <div class="game-icon-fallback" style="display: none;">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <rect x="2" y="6" width="20" height="12" rx="3"></rect>
                  <circle cx="8" cy="12" r="2"></circle>
                  <circle cx="16" cy="12" r="2"></circle>
                </svg>
              </div>
            </div>
            <div class="game-meta">
              <div class="game-badge-row">
                <span class="badge ${platformBadgeClass}">${escapeHtml(g.platform)}</span>
                <span class="title-id-badge">${escapeHtml(g.title_id)}</span>
              </div>
              <div class="game-title" title="${escapeHtml(g.title)}">${escapeHtml(g.title)}</div>
              <div class="game-save-meta">
                <span class="save-indicator ${saveIndicatorClass}"></span>
                <span>${saveStatusText}</span>
              </div>
            </div>
          </div>
          <div class="game-actions">
            ${g.has_save ? `
              <a href="/api/download/${encodeURIComponent(g.title_id)}" class="btn btn-primary btn-sm" onclick="showToast('Iniciando descarga ZIP de ${escapeAttr(g.title)}...')">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                  <polyline points="7 10 12 15 17 10"></polyline>
                  <line x1="12" y1="15" x2="12" y2="3"></line>
                </svg>
                <span>Descargar ZIP</span>
              </a>
              <button class="btn btn-secondary btn-sm" onclick="openFilesModal('${escapeAttr(g.title_id)}', '${escapeAttr(g.title)}')">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path>
                </svg>
                <span>Archivos</span>
              </button>
            ` : `
              <button class="btn btn-secondary btn-sm" style="grid-column: 1 / -1; opacity: 0.5; cursor: not-allowed;" disabled>
                <span>Sin partida en consola</span>
              </button>
            `}
          </div>
        `;
        grid.appendChild(card);
      }

      emptyState.style.display = visible === 0 ? 'block' : 'none';
    }

    async function openFilesModal(titleId, title) {
      const modal = document.getElementById('modal');
      const modalTitle = document.getElementById('modal-title');
      const modalSubtitle = document.getElementById('modal-subtitle');
      const modalBody = document.getElementById('modal-body');

      modalTitle.textContent = title;
      modalSubtitle.textContent = titleId;
      modalBody.innerHTML = '<p style="color: var(--text-muted); text-align: center; padding: 2rem 0;">Cargando archivos...</p>';
      modal.classList.add('open');

      try {
        const res = await fetch(`/api/files/${encodeURIComponent(titleId)}`);
        if (!res.ok) {
          modalBody.innerHTML = '<p style="color: #ef4444; text-align: center;">Error al obtener archivos.</p>';
          return;
        }

        const files = await res.json();
        if (files.length === 0) {
          modalBody.innerHTML = '<p style="color: var(--text-muted); text-align: center;">No hay archivos en la carpeta de partida.</p>';
          return;
        }

        modalBody.innerHTML = '';
        for (const f of files) {
          const item = document.createElement('div');
          item.className = 'file-item';
          item.innerHTML = `
            <div class="file-name-wrap">
              <span class="file-icon">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <path d="M13 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V9z"></path>
                  <polyline points="13 2 13 9 20 9"></polyline>
                </svg>
              </span>
              <span class="file-name" title="${escapeHtml(f.name)}">${escapeHtml(f.name)}</span>
            </div>
            <div style="display: flex; align-items: center; gap: 1rem;">
              <span class="file-size">${escapeHtml(f.size_str)}</span>
              <a href="/api/file/${encodeURIComponent(titleId)}/${encodeURIComponent(f.name)}"
                 class="btn btn-secondary btn-sm"
                 download="${escapeAttr(f.name)}"
                 onclick="showToast('Descargando ${escapeAttr(f.name)}...')">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                  <polyline points="7 10 12 15 17 10"></polyline>
                  <line x1="12" y1="15" x2="12" y2="3"></line>
                </svg>
                <span>Bajar</span>
              </a>
            </div>
          `;
          modalBody.appendChild(item);
        }
      } catch (e) {
        modalBody.innerHTML = '<p style="color: #ef4444; text-align: center;">Error de conexión.</p>';
      }
    }

    // Modal Events
    document.getElementById('modal-close').addEventListener('click', () => {
      document.getElementById('modal').classList.remove('open');
    });

    document.getElementById('modal').addEventListener('click', (e) => {
      if (e.target.id === 'modal') {
        document.getElementById('modal').classList.remove('open');
      }
    });

    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        document.getElementById('modal').classList.remove('open');
      } else if (e.key === '/' && document.activeElement !== document.getElementById('search')) {
        e.preventDefault();
        document.getElementById('search').focus();
      }
    });

    // Search events
    const searchInput = document.getElementById('search');
    const searchClear = document.getElementById('search-clear');

    searchInput.addEventListener('input', () => {
      searchClear.style.display = searchInput.value ? 'block' : 'none';
      renderGames();
    });

    searchClear.addEventListener('click', () => {
      searchInput.value = '';
      searchClear.style.display = 'none';
      searchInput.focus();
      renderGames();
    });

    // Filter Pills
    document.querySelectorAll('.filter-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        currentFilter = btn.dataset.filter;
        renderGames();
      });
    });

    // Copy URL
    document.getElementById('btn-copy-url').addEventListener('click', () => {
      const text = window.location.href;
      navigator.clipboard.writeText(text).then(() => {
        const copyText = document.getElementById('copy-url-text');
        copyText.textContent = 'Copiado!';
        showToast('Enlace copiado al portapapeles');
        setTimeout(() => { copyText.textContent = 'Copiar URL'; }, 2000);
      }).catch(() => {
        showToast('No se pudo copiar el enlace', 'error');
      });
    });

    function escapeHtml(str) {
      if (!str) return "";
      return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
    }

    function escapeAttr(str) {
      if (!str) return "";
      return str.replace(/'/g, "\\'");
    }

    loadStatus();
    loadGames();
  </script>
</body>
</html>
)rawhtml";

} // namespace machete::server
