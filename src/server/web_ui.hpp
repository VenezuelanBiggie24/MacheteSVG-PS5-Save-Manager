// MacheteSVG - Embedded Web Station Frontend
// Copyright (C) 2026 MacheteSVG Team
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

namespace machete::server
{

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#endif

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
    .shinchan-stage {
      position: relative;
      width: 100%;
      height: 240px;
      display: flex;
      justify-content: center;
      align-items: center;
      background: radial-gradient(circle at 50% 50%, rgba(30, 41, 59, 0.45) 0%, rgba(15, 23, 42, 0.75) 100%);
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: var(--radius-md);
      overflow: hidden;
      box-shadow: inset 0 0 24px rgba(0, 0, 0, 0.4);
    }
    .shinchan-grid-pattern {
      position: absolute;
      inset: 0;
      background-image: radial-gradient(#334155 1px, transparent 1px);
      background-size: 18px 18px;
      opacity: 0.18;
      pointer-events: none;
    }
    .shinchan-shadow {
      position: absolute;
      bottom: 18px;
      width: 130px;
      height: 12px;
      background: rgba(0, 0, 0, 0.45);
      border-radius: 9999px;
      filter: blur(3px);
      pointer-events: none;
    }
    .shinchan-badge {
      position: absolute;
      top: 12px;
      right: 12px;
      display: flex;
      align-items: center;
      gap: 0.4rem;
      padding: 0.35rem 0.8rem;
      border-radius: 9999px;
      font-size: 0.72rem;
      font-weight: 700;
      letter-spacing: 0.05em;
      backdrop-filter: blur(8px);
      -webkit-backdrop-filter: blur(8px);
      transition: all 0.25s ease;
      z-index: 3;
    }
    .shinchan-caption {
      position: absolute;
      bottom: 10px;
      left: 14px;
      font-size: 0.72rem;
      color: #94a3b8;
      font-weight: 500;
      z-index: 3;
      display: flex;
      align-items: center;
      gap: 0.35rem;
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
      <div class="brand-icon" style="overflow: hidden; padding: 0;">
        <img src="/api/logo" alt="MacheteSVG Logo" style="width: 100%; height: 100%; object-fit: cover; border-radius: var(--radius-sm);" onerror="this.style.display='none'; this.nextElementSibling.style.display='block';">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#ffffff" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" style="display: none;">
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
          <span class="version-tag" id="main-header-version">PPSA99001 v1.3.0</span>
        </div>
        <div class="brand-subtitle">Estación Web Local & Gestor de Partidas PS5 / PS4</div>
      </div>
    </a>

    <div class="header-actions">
      <div class="status-pill" id="status-pill">
        <span class="status-dot"></span>
        <span id="console-ip">Conectando...</span>
      </div>
      <a href="/api/log" download="machete.log" class="btn btn-secondary btn-sm" id="btn-download-log" title="Descargar registro de eventos y diagnostico del sistema (machete.log)">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
          <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
          <polyline points="14 2 14 8 20 8"></polyline>
          <line x1="16" y1="13" x2="8" y2="13"></line>
          <line x1="16" y1="17" x2="8" y2="17"></line>
          <polyline points="10 9 9 9 8 9"></polyline>
        </svg>
        <span>Descargar Log</span>
      </a>
      <button class="btn btn-secondary btn-sm" id="btn-guide" onclick="openGuideModal()" title="Como restaurar partidas de otra PS5 o PS4 sin corrupcion">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
          <circle cx="12" cy="12" r="10"></circle>
          <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"></path>
          <line x1="12" y1="17" x2="12.01" y2="17"></line>
        </svg>
        <span>Guia Multi-Consola (PS5 & PS4)</span>
      </button>
      <button class="btn btn-secondary btn-sm" id="btn-open-updater" onclick="openUpdaterModal()" title="Subir paquete ZIP para actualizar MacheteSVG directamente">
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
          <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
          <polyline points="17 8 12 3 7 8"></polyline>
          <line x1="12" y1="3" x2="12" y2="15"></line>
        </svg>
        <span>Actualizar App (ZIP)</span>
      </button>
      <button class="btn btn-secondary btn-sm" id="btn-copy-url" title="Copiar enlace de conexion al portapapeles">
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
        <input type="text" id="search" class="search-input" placeholder="Buscar por titulo o Title ID (ej. Spider-Man, PPSA01411)... [Presiona /]">
        <button id="search-clear" class="search-clear" title="Limpiar busqueda">
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
      <!-- Inyectado dinamicamente -->
    </div>

    <!-- Empty State -->
    <div id="empty-state" class="empty-state" style="display: none;">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
        <circle cx="11" cy="11" r="8"></circle>
        <line x1="21" y1="21" x2="16.65" y2="16.65"></line>
      </svg>
      <h3>No se encontraron coincidencias</h3>
      <p>Intenta con otro termino de busqueda o cambia el filtro de plataforma.</p>
    </div>
  </main>

  <!-- Modal Principal (Archivos / Restaurar / Guia) -->
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
        <!-- Inyectado dinamicamente -->
      </div>
    </div>
  </div>

  <!-- Modal de Actualizacion Remota (OTA Web Updater) -->
  <div id="updater-modal" class="modal-backdrop">
    <div class="modal-card" style="max-width: 640px;">
      <div class="modal-header">
        <div>
          <h3 style="display:flex; align-items:center; gap:0.6rem;">
            <span>Actualizador Remoto de MacheteSVG</span>
            <span class="version-tag" id="updater-curr-version">v1.3.0</span>
          </h3>
          <p>Instalacion automatica de paquetes de actualizacion .zip sin necesidad de cliente FTP</p>
        </div>
        <button id="updater-modal-close" class="modal-close" title="Cerrar ventana">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
            <line x1="18" y1="6" x2="6" y2="18"></line>
            <line x1="6" y1="6" x2="18" y2="18"></line>
          </svg>
        </button>
      </div>
      <div class="modal-body" style="gap: 1.25rem;">
        <div style="background: rgba(99, 102, 241, 0.08); border: 1px solid rgba(99, 102, 241, 0.25); border-radius: var(--radius-sm); padding: 1rem; font-size: 0.84rem; line-height: 1.5; color: #cbd5e1;">
          <strong style="color: #ffffff; display: block; margin-bottom: 0.35rem;">Actualizacion 100% Limpia (Depuracion de archivos viejos y reinicio de log):</strong>
          Sube el archivo <code>.zip</code> de la nueva version (ejemplo: <code>MacheteSVG_v1.3.0_PS5.zip</code>). La consola descomprimira los archivos en <code>/data/homebrew/PPSA99001/</code>, eliminara automaticamente cualquier archivo o carpeta obsoleta que ya no pertenezca a la nueva version, reiniciara el archivo <code>machete.log</code> para una sesion completamente fresca, aplicara permisos <code>0777</code> y limpiara temporales, exactamente igual a un borrado y subida limpia via FTP.
        </div>

        <!-- Escenario de Animaciones Shin-chan (Consistencia Visual & Alta Fidelidad) -->
        <div class="shinchan-stage">
          <div class="shinchan-grid-pattern"></div>
          <div class="shinchan-shadow"></div>

          <!-- Contenedor del Personaje Animado -->
          <div style="position: relative; width: 190px; height: 190px; display: flex; align-items: center; justify-content: center; z-index: 2;">
            <!-- 1. Animacion de Baile Buri Buri (En espera / Subiendo / Instalando) -->
            <img id="shinchan-anim-dance" src="/api/shinchan/dance.gif" alt="Shin-chan Baile Buri Buri" style="width: 180px; height: 180px; object-fit: contain; display: block; filter: drop-shadow(0 4px 10px rgba(0,0,0,0.4));">

            <!-- 2. Animacion de Exito DONE (Celebracion) -->
            <img id="shinchan-anim-done" src="/api/shinchan/done.gif" alt="Shin-chan Exito DONE" style="width: 180px; height: 180px; object-fit: contain; display: none; filter: drop-shadow(0 4px 10px rgba(0,0,0,0.4));">

            <!-- 3. Animacion de Fallo (Llanto) -->
            <img id="shinchan-anim-error" src="/api/shinchan/error.gif" alt="Shin-chan Fallo" style="width: 180px; height: 180px; object-fit: contain; display: none; filter: drop-shadow(0 4px 10px rgba(0,0,0,0.4));">

            <!-- Cartel flotante en la esquina del personaje (DONE / FALLO) -->
            <div id="shinchan-corner-badge" style="display: none; position: absolute; top: 0; right: 0; align-items: center; gap: 0.35rem; padding: 0.25rem 0.65rem; border-radius: 0.5rem; font-size: 0.72rem; font-weight: 800; letter-spacing: 0.05em; box-shadow: 0 4px 12px rgba(0,0,0,0.45); z-index: 4;">
              <span id="shinchan-corner-icon" style="display: flex; align-items: center;"></span>
              <span id="shinchan-corner-text">DONE</span>
            </div>
          </div>

          <!-- Insignia de Estado de Shin-chan (Esquina Superior Derecha) -->
          <div id="shinchan-badge" class="shinchan-badge" style="background: rgba(6, 182, 212, 0.12); color: var(--cyan); border: 1px solid rgba(6, 182, 212, 0.35);">
            <span id="shinchan-badge-dot" style="width: 7px; height: 7px; border-radius: 50%; background: var(--cyan);"></span>
            <span id="shinchan-badge-text">EN ESPERA</span>
          </div>

          <!-- Indicador de Estado en la Esquina Inferior Izquierda -->
          <div id="shinchan-caption" class="shinchan-caption">
            <span>Shin-chan listo para actualizar</span>
          </div>
        </div>

        <!-- Selector rapido de previsualizacion de animaciones -->
        <div style="display: flex; gap: 0.5rem; justify-content: center; margin-top: -0.5rem; margin-bottom: -0.25rem;">
          <button type="button" class="btn btn-secondary btn-sm" onclick="setShinchanState('dance')" style="font-size: 0.72rem; padding: 0.2rem 0.65rem;" title="Ver animacion de baile buri-buri">Baile Buri-Buri</button>
          <button type="button" class="btn btn-secondary btn-sm" onclick="setShinchanState('success')" style="font-size: 0.72rem; padding: 0.2rem 0.65rem;" title="Ver animacion de victoria con cartel DONE">Exito (DONE)</button>
          <button type="button" class="btn btn-secondary btn-sm" onclick="setShinchanState('error')" style="font-size: 0.72rem; padding: 0.2rem 0.65rem;" title="Ver animacion de llanto con cartel FALLO">Fallo (Error)</button>
        </div>

        <div id="dropzone" style="border: 2px dashed rgba(99, 102, 241, 0.4); border-radius: var(--radius-md); padding: 2.2rem 1.5rem; text-align: center; background: rgba(14, 18, 30, 0.5); cursor: pointer; transition: all 0.2s ease;">
          <input type="file" id="zip-file-input" accept=".zip" style="display: none;">
          <div style="width: 54px; height: 54px; margin: 0 auto 1rem auto; background: rgba(99, 102, 241, 0.15); border-radius: 50%; display: flex; align-items: center; justify-content: center; color: var(--primary);">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" width="28" height="28" stroke-width="2.2">
              <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
              <polyline points="17 8 12 3 7 8"></polyline>
              <line x1="12" y1="3" x2="12" y2="15"></line>
            </svg>
          </div>
          <div style="font-weight: 700; color: #ffffff; font-size: 1rem; margin-bottom: 0.35rem;" id="dropzone-text">
            Arrastra el archivo ZIP aqui o haz clic para seleccionar
          </div>
          <div style="font-size: 0.78rem; color: var(--text-dim);" id="dropzone-sub">
            Formatos aceptados: MacheteSVG_*.zip (hasta 500 MB)
          </div>
        </div>

        <div id="selected-file-card" style="display: none; background: rgba(255, 255, 255, 0.04); border: 1px solid var(--border); border-radius: var(--radius-sm); padding: 0.85rem 1.15rem; align-items: center; justify-content: space-between;">
          <div style="display: flex; align-items: center; gap: 0.85rem; min-width: 0;">
            <svg viewBox="0 0 24 24" fill="none" stroke="var(--cyan)" width="24" height="24" stroke-width="2">
              <path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"></path>
              <polyline points="3.27 6.96 12 12.01 20.73 6.96"></polyline>
              <line x1="12" y1="22.08" x2="12" y2="12"></line>
            </svg>
            <div style="min-width: 0;">
              <div id="selected-file-name" style="font-weight: 600; color: #ffffff; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-size: 0.9rem;"></div>
              <div id="selected-file-size" style="font-size: 0.75rem; color: var(--text-muted);"></div>
            </div>
          </div>
          <button class="btn btn-secondary btn-sm" id="btn-change-file" style="flex-shrink: 0;">Cambiar</button>
        </div>

        <div id="upload-progress-wrap" style="display: none; flex-direction: column; gap: 0.55rem;">
          <div style="display: flex; justify-content: space-between; font-size: 0.82rem; font-weight: 600;">
            <span id="upload-phase" style="color: var(--cyan);">Subiendo paquete a la consola...</span>
            <span id="upload-percent" style="font-family: ui-monospace, monospace; color: #ffffff;">0%</span>
          </div>
          <div style="width: 100%; height: 8px; background: rgba(255, 255, 255, 0.08); border-radius: 9999px; overflow: hidden; position: relative;">
            <div id="upload-bar" style="width: 0%; height: 100%; background: linear-gradient(90deg, var(--primary), var(--cyan)); border-radius: 9999px; transition: width 0.15s ease;"></div>
          </div>
          <div style="display: flex; justify-content: space-between; font-size: 0.75rem; color: var(--text-dim);">
            <span id="upload-stats">0 MB / 0 MB</span>
            <span id="upload-speed">0 KB/s</span>
          </div>
        </div>

        <div id="update-status-box" style="display: none; border-radius: var(--radius-sm); padding: 1rem; font-size: 0.84rem; line-height: 1.5;"></div>

        <div style="display: flex; justify-content: flex-end; gap: 0.75rem; margin-top: 0.5rem;">
          <button class="btn btn-secondary" id="btn-updater-cancel">Cerrar</button>
          <button class="btn btn-primary" id="btn-updater-submit" disabled>
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
              <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
              <polyline points="17 8 12 3 7 8"></polyline>
              <line x1="12" y1="3" x2="12" y2="15"></line>
            </svg>
            <span id="btn-updater-submit-text">Subir e Instalar Actualizacion</span>
          </button>
        </div>
      </div>
    </div>
  </div>

  <!-- Toast Notification Container -->
  <div id="toast-container" class="toast-container"></div>

  <!-- Footer -->
  <footer>
    MacheteSVG • Disenado para PlayStation 5 con Jailbreak • Arquitectura Cero Corrupcion (PFS Mount + Apollo Resign)
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
      }, 3500);
    }

    async function loadStatus() {
      try {
        const res = await fetch('/api/status?_t=' + Date.now());
        if (res.ok) {
          const data = await res.json();
          document.getElementById('console-ip').textContent = `${data.ip}:${data.port}`;
          document.getElementById('stat-port').textContent = data.port;
          if (data.version) {
            const vLabel = `v${data.version}${data.build ? ' (Build ' + data.build + ')' : ''}`;
            const el = document.getElementById('updater-curr-version');
            if (el) el.textContent = vLabel;
            const hdrEl = document.getElementById('main-header-version');
            if (hdrEl) hdrEl.textContent = `PPSA99001 ${vLabel}`;
          }
        }
      } catch (e) {
        document.getElementById('console-ip').textContent = 'Conectado (LAN :8383)';
      }
    }

    async function loadGames() {
      try {
        const res = await fetch('/api/games?_t=' + Date.now());
        if (!res.ok) return;
        gamesData = await res.json();
        renderStats();
        renderGames();
      } catch (e) {
        console.error('Error cargando titulos:', e);
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

        const isPs4 = g.platform === 'PS4';
        const hasDummy = Boolean(g.has_save_dummy || g.has_save || isPs4);
        const crossBadgeHtml = isPs4
          ? `<span class="title-id-badge" style="color:#34d399; border-color:rgba(16,185,129,0.35); background:rgba(16,185,129,0.12);">MODO APOLLO RESIGN</span>`
          : (hasDummy
              ? `<span class="title-id-badge" style="color:#38bdf8; border-color:rgba(6,182,212,0.35); background:rgba(6,182,212,0.12);">SAVE DUMMY LISTO</span>`
              : `<span class="title-id-badge" style="color:#fbbf24; border-color:rgba(245,158,11,0.35); background:rgba(245,158,11,0.12);" title="Abre el juego 1 vez si vas a restaurar de otra PS5">ABRIR 1 VEZ (OTRA PS5)</span>`);

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
                ${crossBadgeHtml}
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
              <a href="/api/download/${encodeURIComponent(g.title_id)}" class="btn btn-primary btn-sm" onclick="showToast('Iniciando descarga ZIP con archivos desencriptados de ${escapeAttr(g.title)}...')">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
                  <polyline points="7 10 12 15 17 10"></polyline>
                  <line x1="12" y1="15" x2="12" y2="3"></line>
                </svg>
                <span>Descargar ZIP</span>
              </a>
              <button class="btn btn-secondary btn-sm" onclick="openRestoreModal('${escapeAttr(g.title_id)}', '${escapeAttr(g.title)}', '${escapeAttr(g.platform)}', ${hasDummy})">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <polyline points="1 4 1 10 7 10"></polyline>
                  <path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"></path>
                </svg>
                <span>Restaurar / Resignar</span>
              </button>
              <button class="btn btn-secondary btn-sm" style="grid-column: 1 / -1;" onclick="openFilesModal('${escapeAttr(g.title_id)}', '${escapeAttr(g.title)}')">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path>
                </svg>
                <span>Explorar Archivos Individuales (Keystone & Crypto Vault)</span>
              </button>
            ` : `
              <button class="btn btn-secondary btn-sm" style="grid-column: 1 / -1;" onclick="openRestoreModal('${escapeAttr(g.title_id)}', '${escapeAttr(g.title)}', '${escapeAttr(g.platform)}', ${hasDummy})">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor">
                  <polyline points="1 4 1 10 7 10"></polyline>
                  <path d="M3.51 15a9 9 0 1 0 2.13-9.36L1 10"></path>
                </svg>
                <span>Restaurar / Resignar Copia en Consola</span>
              </button>
            `}
          </div>
        `;
        grid.appendChild(card);
      }

      emptyState.style.display = visible === 0 ? 'block' : 'none';
    }

    function openGuideModal() {
      const modal = document.getElementById('modal');
      document.getElementById('modal-title').textContent = 'Guia de Restauracion Multi-Consola (PS5 & PS4)';
      document.getElementById('modal-subtitle').textContent = 'Arquitectura PFS Mount + Save Dummy Injection + Apollo Resigner';
      const modalBody = document.getElementById('modal-body');

      modalBody.innerHTML = `
        <div class="file-item" style="flex-direction:column; align-items:flex-start; gap:0.5rem; border-color:rgba(16,185,129,0.4);">
          <div style="font-weight:700; color:#34d399; font-size:0.95rem;">1. Partidas de PS4 (CUSAxxxxx) — Modo Apollo 100% Automatico</div>
          <div style="font-size:0.85rem; color:var(--text-muted); line-height:1.5;">
            <strong>Que hace MacheteSVG automaticamente:</strong> Resigna el <code>ACCOUNT_ID</code> de 64 bits de tu perfil actual, actualiza el <code>user_id</code> local, recalcula el hash <code>PARAMS.psid_hmac</code> en todos los archivos <code>param.sfo</code> (tanto en <code>savedata_meta</code> como dentro del contenedor PFS montado), e inyecta el <code>keystone</code> autentico de 96 bytes.<br>
            <strong>Que debes hacer tu:</strong> Absolutamente nada extra. Solo coloca la copia en <code>/data/MacheteSVG/</code> o en tu USB (<code>/mnt/usb0/MacheteSVG/</code>) y pulsa <strong>Restaurar</strong> desde la app en la consola o desde esta pagina web.
          </div>
        </div>
        <div class="file-item" style="flex-direction:column; align-items:flex-start; gap:0.5rem; border-color:rgba(6,182,212,0.4);">
          <div style="font-weight:700; color:#38bdf8; font-size:0.95rem;">2. Partidas de PS5 (PPSAxxxxx) — Inyeccion Cross-Console con Save Dummy</div>
          <div style="font-size:0.85rem; color:var(--text-muted); line-height:1.5;">
            <strong>Por que antes daba error de Datos Danados:</strong> Cada consola PS5 cifra el archivo <code>sdimg_*</code> con una clave unica de su chip SAMU. Si copias un <code>sdimg_*</code> de otra consola encima del tuyo, el SAMU lo rechaza.<br>
            <strong>Como lo resuelve MacheteSVG ahora:</strong> Al hacer un respaldo, MacheteSVG monta el contenedor PFS con <code>/dev/pfsmgr</code> y guarda los archivos en claro dentro de <code>decrypted/</code> junto con <code>crypto_vault.bin</code>.<br>
            <strong>Pasos para restaurar en OTRA PS5:</strong><br>
            1) Si el juego dice <em>ABRIR 1 VEZ</em>, inicia el juego una sola vez en la PS5 destino hasta que cree su primer autoguardado (<strong>Save Dummy</strong>) y cierralo.<br>
            2) Pulsa <strong>Restaurar</strong> en MacheteSVG (consola o web). MacheteSVG conserva tu Save Dummy local, lo monta en caliente, inyecta dentro los archivos desencriptados y el <code>keystone</code> original, actualiza el tamano en <code>param.json</code> y el SAMU de tu consola lo sella sin corrupcion.
          </div>
        </div>
      `;
      modal.classList.add('open');
    }

    async function openRestoreModal(titleId, title, platform, hasDummy) {
      const modal = document.getElementById('modal');
      const modalTitle = document.getElementById('modal-title');
      const modalSubtitle = document.getElementById('modal-subtitle');
      const modalBody = document.getElementById('modal-body');

      modalTitle.textContent = `Restaurar / Resignar: ${title}`;
      modalSubtitle.textContent = `${titleId} (${platform})`;
      modalBody.innerHTML = '<p style="color: var(--text-muted); text-align: center; padding: 2rem 0;">Consultando copias de seguridad en la consola...</p>';
      modal.classList.add('open');

      try {
        const res = await fetch(`/api/backups/${encodeURIComponent(titleId)}?_t=${Date.now()}`);
        const backups = res.ok ? await res.json() : [];

        const isPs4 = (platform === 'PS4');
        const statusBanner = isPs4
          ? `<div class="file-item" style="flex-direction:column; align-items:flex-start; gap:0.35rem; border-color:rgba(16,185,129,0.45); background:rgba(16,185,129,0.08);">
               <div style="font-weight:700; color:#34d399; font-size:0.88rem;">Modo Apollo PS4 Activo (100% Automatico)</div>
               <div style="font-size:0.8rem; color:var(--text-muted);">Al restaurar cualquier copia, MacheteSVG resignara automaticamente Account ID, PSID, param.sfo y Keystone dentro del contenedor PFS.</div>
             </div>`
          : (hasDummy
              ? `<div class="file-item" style="flex-direction:column; align-items:flex-start; gap:0.35rem; border-color:rgba(6,182,212,0.45); background:rgba(6,182,212,0.08);">
                   <div style="font-weight:700; color:#38bdf8; font-size:0.88rem;">Save Dummy Local Detectado — Listo para Inyeccion Cross-Console</div>
                   <div style="font-size:0.8rem; color:var(--text-muted);">MacheteSVG conservara tu contenedor PFS local (sdimg_*), inyectara los datos desencriptados y el SAMU de esta PS5 sellara la partida sin corrupcion.</div>
                 </div>`
              : `<div class="file-item" style="flex-direction:column; align-items:flex-start; gap:0.35rem; border-color:rgba(245,158,11,0.5); background:rgba(245,158,11,0.1);">
                   <div style="font-weight:700; color:#fbbf24; font-size:0.88rem;">Paso Previo Requerido si la partida viene de OTRA PS5</div>
                   <div style="font-size:0.8rem; color:var(--text-muted);">Esta consola aun no tiene un Save Dummy para este juego. Si la copia proviene de otra PS5, abre primero el juego 1 vez para que cree una partida vacia, cierralo y vuelve a pulsar Restaurar.</div>
                 </div>`);

        let html = statusBanner;
        html += `
          <div style="display:flex; justify-content:space-between; align-items:center; margin:0.35rem 0;">
            <span style="font-size:0.82rem; color:var(--text-muted); font-weight:600;">Copias en /data/MacheteSVG y USB (${backups.length})</span>
            <button class="btn btn-secondary btn-sm" onclick="triggerConsoleBackup('${escapeAttr(titleId)}', '${escapeAttr(title)}', '${escapeAttr(platform)}', ${hasDummy})">
              <span>+ Crear Nuevo Respaldo en Consola</span>
            </button>
          </div>
        `;

        if (backups.length === 0) {
          html += `
            <div style="padding:1.75rem; text-align:center; color:var(--text-muted); font-size:0.86rem; border:1px dashed var(--border); border-radius:var(--radius-sm);">
              No hay copias guardadas para <strong>${escapeHtml(titleId)}</strong> en <code>/data/MacheteSVG/</code> ni en <code>/mnt/usb0/MacheteSVG/</code>.<br><br>
              Puedes pulsar <strong>+ Crear Nuevo Respaldo en Consola</strong> arriba o copiar una carpeta de respaldo en tu USB.
            </div>
          `;
        } else {
          for (const b of backups) {
            const crossOk = (b.has_decrypted || b.has_crypto_vault);
            const badgeText = isPs4 ? 'MODO APOLLO OK' : (crossOk ? 'CROSS-CONSOLE + PFS OK' : (b.has_keystone ? 'KEYSTONE OK' : 'ESTANDAR'));
            const btnText = isPs4 ? 'Restaurar y Resignar (Apollo)' : 'Restaurar e Inyectar';
            html += `
              <div class="file-item">
                <div class="file-name-wrap" style="flex-direction:column; align-items:flex-start; gap:0.25rem;">
                  <div style="display:flex; align-items:center; gap:0.5rem; flex-wrap:wrap;">
                    <span class="badge badge-ps4" style="font-size:0.68rem;">${escapeHtml(b.device_label)}</span>
                    <span class="file-name">${escapeHtml(b.folder_name)}</span>
                    <span class="title-id-badge" style="color:#34d399; border-color:rgba(16,185,129,0.35);">${badgeText}</span>
                  </div>
                  <div style="font-size:0.76rem; color:var(--text-muted);">
                    ${escapeHtml(b.timestamp_str)} | ${b.file_count} archivos | ${escapeHtml(b.size_str)}
                  </div>
                </div>
                <button class="btn btn-primary btn-sm" onclick="triggerConsoleRestore('${escapeAttr(titleId)}', ${b.index})">
                  <span>${btnText}</span>
                </button>
              </div>
            `;
          }
        }

        modalBody.innerHTML = html;
      } catch (e) {
        modalBody.innerHTML = '<p style="color: #ef4444; text-align: center;">Error de conexion con la consola.</p>';
      }
    }

    async function triggerConsoleBackup(titleId, title, platform, hasDummy) {
      showToast(`Creando respaldo con extraccion PFS para ${titleId}...`);
      try {
        const res = await fetch(`/api/backup/${encodeURIComponent(titleId)}`, { method: 'POST' });
        const data = await res.json();
        showToast(data.message || 'Operacion finalizada');
        await loadGames();
        openRestoreModal(titleId, title, platform, hasDummy);
      } catch (e) {
        showToast('Error al ejecutar respaldo en consola', 'error');
      }
    }

    async function triggerConsoleRestore(titleId, index) {
      showToast(`Restaurando e inyectando partida en ${titleId}...`);
      try {
        const res = await fetch(`/api/restore/${encodeURIComponent(titleId)}?index=${index}`, { method: 'POST' });
        const data = await res.json();
        showToast(data.message || 'Restauracion completada');
        if (data.success) {
          document.getElementById('modal').classList.remove('open');
          await loadGames();
        }
      } catch (e) {
        showToast('Error de red al restaurar partida', 'error');
      }
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
        const res = await fetch(`/api/files/${encodeURIComponent(titleId)}?_t=${Date.now()}`);
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
          const sizeDisp = f.size_formatted || f.size_str || '';
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
              <span class="file-size">${escapeHtml(sizeDisp)}</span>
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
        modalBody.innerHTML = '<p style="color: #ef4444; text-align: center;">Error de conexion.</p>';
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

    // OTA Web Updater Logic
    let selectedZipFile = null;
    let isUpdating = false;

    function setShinchanState(state, captionText) {
      const animDance = document.getElementById('shinchan-anim-dance');
      const animDone = document.getElementById('shinchan-anim-done');
      const animError = document.getElementById('shinchan-anim-error');
      const badge = document.getElementById('shinchan-badge');
      const badgeDot = document.getElementById('shinchan-badge-dot');
      const badgeText = document.getElementById('shinchan-badge-text');
      const cornerBadge = document.getElementById('shinchan-corner-badge');
      const cornerIcon = document.getElementById('shinchan-corner-icon');
      const cornerText = document.getElementById('shinchan-corner-text');
      const caption = document.getElementById('shinchan-caption');

      if (!animDance || !animDone || !animError) return;

      if (state === 'success' || state === 'done') {
        animDance.style.display = 'none';
        animDone.style.display = 'block';
        animError.style.display = 'none';
        badge.style.background = 'rgba(16, 185, 129, 0.15)';
        badge.style.borderColor = 'rgba(16, 185, 129, 0.45)';
        badge.style.color = '#34d399';
        badgeDot.style.background = '#10b981';
        badgeText.textContent = 'EXITO (DONE)';
        if (cornerBadge) {
          cornerBadge.style.display = 'flex';
          cornerBadge.style.background = 'rgba(6, 78, 59, 0.95)';
          cornerBadge.style.border = '1px solid rgba(16, 185, 129, 0.8)';
          cornerBadge.style.color = '#34d399';
          cornerIcon.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" width="13" height="13" stroke-width="3"><polyline points="20 6 9 17 4 12"></polyline></svg>';
          cornerText.textContent = 'DONE';
        }
        if (caption) caption.innerHTML = `<span>${captionText || 'Actualizacion completada con exito'}</span>`;
      } else if (state === 'error' || state === 'fail') {
        animDance.style.display = 'none';
        animDone.style.display = 'none';
        animError.style.display = 'block';
        badge.style.background = 'rgba(244, 63, 94, 0.15)';
        badge.style.borderColor = 'rgba(244, 63, 94, 0.45)';
        badge.style.color = '#fb7185';
        badgeDot.style.background = '#f43f5e';
        badgeText.textContent = 'FALLO (ERROR)';
        if (cornerBadge) {
          cornerBadge.style.display = 'flex';
          cornerBadge.style.background = 'rgba(76, 5, 25, 0.95)';
          cornerBadge.style.border = '1px solid rgba(244, 63, 94, 0.8)';
          cornerBadge.style.color = '#fb7185';
          cornerIcon.innerHTML = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" width="13" height="13" stroke-width="3"><line x1="18" y1="6" x2="6" y2="18"></line><line x1="6" y1="6" x2="18" y2="18"></line></svg>';
          cornerText.textContent = 'FALLO';
        }
        if (caption) caption.innerHTML = `<span>${captionText || 'Error en la instalacion'}</span>`;
      } else if (state === 'uploading' || state === 'installing' || state === 'dance') {
        animDance.style.display = 'block';
        animDone.style.display = 'none';
        animError.style.display = 'none';
        if (cornerBadge) cornerBadge.style.display = 'none';
        badge.style.background = 'rgba(99, 102, 241, 0.18)';
        badge.style.borderColor = 'rgba(99, 102, 241, 0.5)';
        badge.style.color = '#818cf8';
        badgeDot.style.background = '#6366f1';
        badgeText.textContent = state === 'installing' ? 'INSTALANDO EN SSD' : 'SUBIENDO PAQUETE';
        if (caption) caption.innerHTML = `<span>${captionText || 'Shin-chan bailando buri-buri mientras actualiza...'}</span>`;
      } else { // idle
        animDance.style.display = 'block';
        animDone.style.display = 'none';
        animError.style.display = 'none';
        if (cornerBadge) cornerBadge.style.display = 'none';
        badge.style.background = 'rgba(6, 182, 212, 0.12)';
        badge.style.borderColor = 'rgba(6, 182, 212, 0.35)';
        badge.style.color = 'var(--cyan)';
        badgeDot.style.background = 'var(--cyan)';
        badgeText.textContent = 'EN ESPERA';
        if (caption) caption.innerHTML = `<span>${captionText || 'Shin-chan listo para actualizar'}</span>`;
      }
    }

    function openUpdaterModal() {
      document.getElementById('updater-modal').classList.add('open');
      resetUpdaterState();
    }

    function closeUpdaterModal() {
      if (isUpdating) {
        if (!confirm('La instalacion esta en curso. Seguro que deseas cancelar?')) return;
      }
      document.getElementById('updater-modal').classList.remove('open');
    }

    function resetUpdaterState() {
      selectedZipFile = null;
      isUpdating = false;
      document.getElementById('zip-file-input').value = '';
      document.getElementById('dropzone').style.display = 'block';
      document.getElementById('selected-file-card').style.display = 'none';
      document.getElementById('upload-progress-wrap').style.display = 'none';
      document.getElementById('update-status-box').style.display = 'none';
      const submitBtn = document.getElementById('btn-updater-submit');
      submitBtn.disabled = true;
      document.getElementById('btn-updater-submit-text').textContent = 'Subir e Instalar Actualizacion';
      setShinchanState('idle', 'Shin-chan listo para recibir el paquete');
    }

    function handleSelectedFile(file) {
      if (!file) return;
      if (!file.name.toLowerCase().endsWith('.zip')) {
        showToast('El archivo seleccionado debe ser un archivo .zip', 'error');
        return;
      }
      selectedZipFile = file;
      document.getElementById('selected-file-name').textContent = file.name;
      const sizeMb = (file.size / (1024 * 1024)).toFixed(2);
      document.getElementById('selected-file-size').textContent = `${sizeMb} MB (${file.size.toLocaleString()} bytes)`;
      document.getElementById('dropzone').style.display = 'none';
      document.getElementById('selected-file-card').style.display = 'flex';
      document.getElementById('btn-updater-submit').disabled = false;
      document.getElementById('update-status-box').style.display = 'none';
      setShinchanState('idle', 'Paquete listo: ' + file.name);
    }

    // Dropzone Events
    const dropzone = document.getElementById('dropzone');
    const zipFileInput = document.getElementById('zip-file-input');

    dropzone.addEventListener('click', () => zipFileInput.click());
    document.getElementById('btn-change-file').addEventListener('click', () => zipFileInput.click());

    zipFileInput.addEventListener('change', (e) => {
      if (e.target.files && e.target.files[0]) {
        handleSelectedFile(e.target.files[0]);
      }
    });

    ['dragenter', 'dragover'].forEach(eventName => {
      dropzone.addEventListener(eventName, (e) => {
        e.preventDefault();
        e.stopPropagation();
        dropzone.style.borderColor = 'var(--cyan)';
        dropzone.style.background = 'rgba(6, 182, 212, 0.12)';
      });
    });

    ['dragleave', 'drop'].forEach(eventName => {
      dropzone.addEventListener(eventName, (e) => {
        e.preventDefault();
        e.stopPropagation();
        dropzone.style.borderColor = 'rgba(99, 102, 241, 0.4)';
        dropzone.style.background = 'rgba(14, 18, 30, 0.5)';
      });
    });

    dropzone.addEventListener('drop', (e) => {
      if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files[0]) {
        handleSelectedFile(e.dataTransfer.files[0]);
      }
    });

    document.getElementById('updater-modal-close').addEventListener('click', closeUpdaterModal);
    document.getElementById('btn-updater-cancel').addEventListener('click', closeUpdaterModal);

    document.getElementById('updater-modal').addEventListener('click', (e) => {
      if (e.target.id === 'updater-modal') {
        closeUpdaterModal();
      }
    });

    // Subir e instalar
    document.getElementById('btn-updater-submit').addEventListener('click', () => {
      if (!selectedZipFile || isUpdating) return;

      isUpdating = true;
      const submitBtn = document.getElementById('btn-updater-submit');
      submitBtn.disabled = true;
      document.getElementById('btn-updater-cancel').disabled = true;
      document.getElementById('btn-change-file').style.display = 'none';
      document.getElementById('upload-progress-wrap').style.display = 'flex';
      document.getElementById('update-status-box').style.display = 'none';
      document.getElementById('upload-bar').style.width = '0%';
      document.getElementById('upload-percent').textContent = '0%';
      document.getElementById('upload-phase').textContent = 'Subiendo paquete a la consola...';
      document.getElementById('btn-updater-submit-text').textContent = 'Instalando...';

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/api/update');
      xhr.setRequestHeader('Content-Type', 'application/zip');

      const startTime = Date.now();
      let lastLoaded = 0;
      let lastTime = startTime;

      xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
          const pct = Math.round((e.loaded / e.total) * 100);
          document.getElementById('upload-bar').style.width = pct + '%';
          document.getElementById('upload-percent').textContent = pct + '%';

          const loadedMb = (e.loaded / (1024 * 1024)).toFixed(1);
          const totalMb = (e.total / (1024 * 1024)).toFixed(1);
          document.getElementById('upload-stats').textContent = `${loadedMb} MB / ${totalMb} MB`;

          const now = Date.now();
          const dt = (now - lastTime) / 1000;
          if (dt >= 0.5) {
            const bytesPerSec = (e.loaded - lastLoaded) / dt;
            const speedMb = (bytesPerSec / (1024 * 1024)).toFixed(1);
            document.getElementById('upload-speed').textContent = `${speedMb} MB/s`;
            lastLoaded = e.loaded;
            lastTime = now;
          }

          if (pct >= 100) {
            document.getElementById('upload-phase').textContent = 'Descomprimiendo, reemplazando archivos y aplicando permisos 0777...';
            document.getElementById('upload-speed').textContent = 'Instalando en SSD...';
            setShinchanState('installing', 'Descomprimiendo y aplicando actualizacion en SSD...');
          } else {
            setShinchanState('uploading', 'Subiendo paquete a la consola (' + pct + '%)...');
          }
        }
      };

      xhr.onload = () => {
        isUpdating = false;
        document.getElementById('btn-updater-cancel').disabled = false;
        document.getElementById('btn-change-file').style.display = 'inline-block';
        const statusBox = document.getElementById('update-status-box');
        statusBox.style.display = 'block';

        try {
          const res = JSON.parse(xhr.responseText);
          if (xhr.status === 200 && res.success) {
            statusBox.style.background = 'rgba(16, 185, 129, 0.12)';
            statusBox.style.border = '1px solid rgba(16, 185, 129, 0.4)';
            statusBox.style.color = '#34d399';
            statusBox.innerHTML = `<strong>Actualizacion Completada con Exito:</strong><br>${escapeHtml(res.message)}`;
            document.getElementById('upload-phase').textContent = 'Instalacion finalizada';
            document.getElementById('upload-bar').style.background = 'linear-gradient(90deg, #10b981, #06b6d4)';
            document.getElementById('btn-updater-submit-text').textContent = 'Actualizado';
            setShinchanState('success', 'Actualizacion limpia instalada con exito');
            showToast('Actualizacion instalada correctamente', 'success');
            loadStatus();
          } else {
            statusBox.style.background = 'rgba(239, 68, 68, 0.12)';
            statusBox.style.border = '1px solid rgba(239, 68, 68, 0.4)';
            statusBox.style.color = '#f87171';
            statusBox.innerHTML = `<strong>Fallo de Instalacion:</strong><br>${escapeHtml(res.message || 'Error desconocido')}`;
            document.getElementById('upload-phase').textContent = 'Fallo en la instalacion';
            document.getElementById('btn-updater-submit-text').textContent = 'Reintentar';
            submitBtn.disabled = false;
            setShinchanState('error', res.message || 'Fallo en la instalacion');
            showToast(res.message || 'Error al actualizar', 'error');
          }
        } catch (err) {
          statusBox.style.background = 'rgba(239, 68, 68, 0.12)';
          statusBox.style.border = '1px solid rgba(239, 68, 68, 0.4)';
          statusBox.style.color = '#f87171';
          statusBox.innerHTML = `<strong>Error de Respuesta:</strong><br>No se pudo procesar la respuesta de la consola.`;
          document.getElementById('upload-phase').textContent = 'Error de respuesta';
          document.getElementById('btn-updater-submit-text').textContent = 'Reintentar';
          submitBtn.disabled = false;
          setShinchanState('error', 'Error procesando respuesta de la consola');
        }
      };

      xhr.onerror = () => {
        isUpdating = false;
        document.getElementById('btn-updater-cancel').disabled = false;
        document.getElementById('btn-change-file').style.display = 'inline-block';
        submitBtn.disabled = false;
        document.getElementById('btn-updater-submit-text').textContent = 'Reintentar';
        const statusBox = document.getElementById('update-status-box');
        statusBox.style.display = 'block';
        statusBox.style.background = 'rgba(239, 68, 68, 0.12)';
        statusBox.style.border = '1px solid rgba(239, 68, 68, 0.4)';
        statusBox.style.color = '#f87171';
        statusBox.innerHTML = `<strong>Error de Conexion:</strong><br>Se perdio la comunicacion con el servidor web de la consola.`;
        document.getElementById('upload-phase').textContent = 'Error de conexion';
        setShinchanState('error', 'Error de conexion con la consola');
        showToast('Error de conexion con la consola', 'error');
      };

      xhr.send(selectedZipFile);
    });

    loadStatus();
    loadGames();
  </script>
</body>
</html>
)rawhtml";

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

} // namespace machete::server
