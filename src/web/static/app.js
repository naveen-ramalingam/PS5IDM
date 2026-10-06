// State
let ws = null;
let reconnectTimer = null;
let downloads = new Map();

// UI Elements
const loginScreen = document.getElementById('loginScreen');
const appScreen = document.getElementById('appScreen');
const loginForm = document.getElementById('loginForm');
const loginError = document.getElementById('loginError');
const connectionStatus = document.getElementById('connectionStatus');
const downloadList = document.getElementById('downloadList');
const addModal = document.getElementById('addModal');

// Init
document.addEventListener('DOMContentLoaded', () => {
    // Check if we have session (this is a simple test, real app would check API)
    if (document.cookie.includes('session=admin_token')) {
        showApp();
    }
    
    // Navigation
    document.querySelectorAll('.nav-item').forEach(link => {
        link.addEventListener('click', (e) => {
            e.preventDefault();
            document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
            document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
            
            // Mark both desktop and mobile as active
            const target = e.target.getAttribute('data-target');
            document.querySelectorAll(`[data-target="${target}"]`).forEach(n => n.classList.add('active'));
            document.getElementById(target).classList.add('active');
        });
    });

    // Modals
    document.getElementById('addDownloadBtn').addEventListener('click', () => addModal.classList.add('active'));
    document.querySelectorAll('.close-modal').forEach(btn => {
        btn.addEventListener('click', () => {
            document.querySelectorAll('.modal').forEach(m => m.classList.remove('active'));
            document.getElementById('addUrlInput').value = '';
            document.getElementById('archivePasswordInput').value = '';
        });
    });

    // Forms
    loginForm.addEventListener('submit', handleLogin);
});

async function handleLogin(e) {
    e.preventDefault();
    const u = document.getElementById('username').value;
    const p = document.getElementById('password').value;
    
    try {
        const res = await fetch('/api/v1/auth/login', {
            method: 'POST',
            body: JSON.stringify({username: u, password: p})
        });
        const data = await res.json();
        if (data.success) {
            showApp();
        } else {
            loginError.textContent = data.error?.message || 'Login failed';
        }
    } catch (err) {
        loginError.textContent = 'Connection error';
    }
}

function showApp() {
    loginScreen.classList.remove('active');
    appScreen.classList.add('active');
    fetchInitialState();
    connectWebSocket();
}

async function fetchInitialState() {
    try {
        const res = await fetch('/api/v1/downloads');
        const data = await res.json();
        if (data.success) {
            downloads.clear();
            data.data.forEach(d => {
                downloads.set(d.id, d);
            });
            renderDownloads();
        }
        
        await fetchArchives();
        await fetchFiles();
        await fetchSettings();
    } catch (e) {
        console.error("Failed to fetch initial state", e);
    }
}

async function fetchArchives() {
    try {
        const res = await fetch('/api/v1/archives');
        const data = await res.json();
        if (data.success) {
            renderArchives(data.data);
        }
    } catch (e) {
        console.error("Failed to fetch archives", e);
    }
}

async function fetchFiles() {
    try {
        const res = await fetch('/api/v1/files');
        const data = await res.json();
        if (data.success) {
            renderFiles(data.data);
        }
    } catch (e) {
        console.error("Failed to fetch files", e);
    }
}

async function fetchSettings() {
    try {
        const res = await fetch('/api/v1/settings');
        const data = await res.json();
        if (data.success) {
            document.getElementById('settingDownloadDir').value = data.data.downloadDir || '';
            document.getElementById('settingConnections').value = data.data.maxConcurrentDownloads || 3;
            document.getElementById('settingSpeedLimit').value = data.data.downloadSpeedLimit || 0;
        }
    } catch (e) {
        console.error("Failed to fetch settings", e);
    }
}

function connectWebSocket() {
    if (ws) ws.close();
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(`${protocol}//${window.location.host}/ws`);
    
    ws.onopen = () => {
        connectionStatus.classList.add('connected');
        if (reconnectTimer) clearInterval(reconnectTimer);
    };
    
    ws.onclose = () => {
        connectionStatus.classList.remove('connected');
        reconnectTimer = setTimeout(connectWebSocket, 3000);
    };
    
    ws.onmessage = (e) => {
        try {
            const msg = JSON.parse(e.data);
            handleWsMessage(msg);
        } catch (err) {}
    };
}

function handleWsMessage(msg) {
    if (msg.event === 'download.progress') {
        const d = msg.data;
        if (downloads.has(d.id)) {
            const existing = downloads.get(d.id);
            existing.progress = d.progress;
            existing.speed = d.speed;
            existing.downloaded = d.downloaded;
            existing.total = d.total;
            existing.eta = d.eta;
            updateDownloadUI(existing);
        }
    } else if (msg.event === 'download.completed') {
        fetchInitialState(); // Refresh for now
    }
}

function formatSize(bytes) {
    if (!bytes || bytes < 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

function formatTime(secs) {
    if (secs <= 0) return '--';
    const m = Math.floor(secs / 60);
    const s = secs % 60;
    return `${m}m ${s}s`;
}

function renderDownloads() {
    downloadList.innerHTML = '';
    downloads.forEach(d => {
        const card = document.createElement('div');
        card.className = 'card download-card';
        card.id = `dl-${d.id}`;
        
        let actionBtn = '';
        const statuses = ['PENDING', 'QUEUED', 'CONNECTING', 'DOWNLOADING', 'PAUSED', 'COMPLETED', 'FAILED', 'CANCELLED', 'VERIFYING', 'RETRYING', 'MERGING'];
        const st = statuses[d.status] || 'UNKNOWN';
        
        if (d.status === 0 || d.status === 1 || d.status === 2 || d.status === 3 || d.status === 8 || d.status === 9 || d.status === 10) { // Active
            actionBtn = `<button class="btn btn-primary" onclick="downloadAction(${d.id}, 'pause')">Pause</button>`;
        } else if (d.status === 4) { // Paused
            actionBtn = `<button class="btn btn-primary" onclick="downloadAction(${d.id}, 'resume')">Resume</button>`;
        } else if (d.status === 6 || d.status === 7) { // Failed, Cancelled
            actionBtn = `<button class="btn btn-primary" onclick="downloadAction(${d.id}, 'resume')">Retry</button>`;
        }

        card.innerHTML = `
            <div class="dl-header">
                <div class="dl-title">${d.filename || d.url}</div>
                <div class="dl-status">${st}</div>
            </div>
            <div class="progress-container">
                <div class="progress-bar" id="dl-bar-${d.id}" style="width: ${d.progress}%"></div>
            </div>
            <div class="dl-stats">
                <span id="dl-sizes-${d.id}">${formatSize(d.downloaded)} / ${formatSize(d.total)}</span>
                <span id="dl-speed-${d.id}">${formatSize(d.speed)}/s &bull; ETA ${formatTime(d.eta)}</span>
            </div>
            <div style="margin-top: 10px; display: flex; gap: 10px;">
                ${actionBtn}
                <button class="btn btn-danger" onclick="downloadAction(${d.id}, 'cancel')">Cancel</button>
            </div>
        `;
        downloadList.appendChild(card);
    });
}

function renderArchives(archives) {
    const archiveList = document.getElementById('archiveList');
    archiveList.innerHTML = '';
    if (archives.length === 0) {
        archiveList.innerHTML = '<div style="padding: 20px; text-align: center; color: #888;">No archives found</div>';
        return;
    }
    
    archives.forEach(a => {
        const card = document.createElement('div');
        card.className = 'card download-card';
        card.innerHTML = `
            <div class="dl-header">
                <div class="dl-title">${a.baseName}</div>
                <div class="dl-status">${a.isComplete ? 'READY' : 'INCOMPLETE'}</div>
            </div>
            <div class="dl-stats">
                <span>Parts: ${a.foundPartCount} / ${a.expectedParts > 0 ? a.expectedParts : '?'}</span>
                <span>Size: ${formatSize(a.totalSize)}</span>
            </div>
            <div style="margin-top: 10px; display: flex; gap: 10px;">
                <button class="btn btn-primary" onclick="archiveAction(${a.id}, 'extract')" ${!a.isComplete ? 'disabled' : ''}>Extract</button>
                <button class="btn btn-danger" onclick="archiveAction(${a.id}, 'remove')">Delete</button>
            </div>
        `;
        archiveList.appendChild(card);
    });
}

function renderFiles(files) {
    const fileList = document.getElementById('fileList');
    fileList.innerHTML = '';
    
    // Sort directories first
    files.sort((a, b) => {
        if (a.isDirectory && !b.isDirectory) return -1;
        if (!a.isDirectory && b.isDirectory) return 1;
        return a.name.localeCompare(b.name);
    });

    files.forEach(f => {
        const row = document.createElement('div');
        row.className = 'file-item';
        const icon = f.isDirectory ? '📁' : (f.isArchive ? '📦' : '📄');
        row.innerHTML = `
            <span class="file-icon">${icon}</span>
            <span class="file-name">${f.name}</span>
            <span class="file-size">${f.isDirectory ? '--' : formatSize(f.size)}</span>
        `;
        fileList.appendChild(row);
    });
}

async function downloadAction(id, action) {
    try {
        await fetch('/api/v1/downloads/action', {
            method: 'POST',
            body: JSON.stringify({id: id, action: action})
        });
        fetchInitialState(); // Refresh list after action
    } catch (e) {
        console.error(e);
    }
}

let pendingExtractId = null;

async function archiveAction(id, action) {
    if (action === 'extract') {
        // Open password modal instead of extracting directly
        pendingExtractId = id;
        document.getElementById('archivePasswordInput').value = '';
        document.getElementById('passwordModal').classList.add('active');
        return;
    }
    try {
        await fetch('/api/v1/archives/action', {
            method: 'POST',
            body: JSON.stringify({id: id, action: action})
        });
        fetchInitialState();
    } catch (e) {
        console.error(e);
    }
}

document.getElementById('confirmExtractBtn').addEventListener('click', async () => {
    if (pendingExtractId === null) return;
    const password = document.getElementById('archivePasswordInput').value;
    try {
        await fetch('/api/v1/archives/action', {
            method: 'POST',
            body: JSON.stringify({id: pendingExtractId, action: 'extract', password: password})
        });
        document.getElementById('passwordModal').classList.remove('active');
        pendingExtractId = null;
        fetchInitialState();
    } catch (e) {
        console.error(e);
    }
});

async function addDownload() {
    const url = document.getElementById('addUrlInput').value;
    if (!url) return;
    try {
        await fetch('/api/v1/downloads', {
            method: 'POST',
            body: JSON.stringify({url: url})
        });
        document.getElementById('addModal').classList.remove('active');
        document.getElementById('addUrlInput').value = '';
        fetchInitialState();
    } catch (e) {
        console.error(e);
    }
}

document.getElementById('startDownloadBtn').addEventListener('click', addDownload);

async function saveSettings() {
    const dir = document.getElementById('settingDownloadDir').value;
    const conns = parseInt(document.getElementById('settingConnections').value, 10);
    const speed = parseInt(document.getElementById('settingSpeedLimit').value, 10);
    
    try {
        await fetch('/api/v1/settings', {
            method: 'POST',
            body: JSON.stringify({
                downloadDir: dir,
                maxConcurrentDownloads: conns,
                downloadSpeedLimit: speed
            })
        });
        alert("Settings saved!");
    } catch (e) {
        console.error(e);
        alert("Failed to save settings");
    }
}

document.getElementById('saveSettingsBtn').addEventListener('click', saveSettings);

function updateDownloadUI(d) {
    const bar = document.getElementById(`dl-bar-${d.id}`);
    const sizes = document.getElementById(`dl-sizes-${d.id}`);
    const speed = document.getElementById(`dl-speed-${d.id}`);
    
    if (bar) bar.style.width = `${d.progress}%`;
    if (sizes) sizes.textContent = `${formatSize(d.downloaded)} / ${formatSize(d.total)}`;
    if (speed) speed.innerHTML = `${formatSize(d.speed)}/s &bull; ETA ${formatTime(d.eta)}`;
}

// ── Link Grabber ─────────────────────────────────────────────────────────────
let grabbedLinks = [];

document.getElementById('linkGrabberBtn').addEventListener('click', () => {
    document.getElementById('linkGrabberModal').classList.add('active');
    document.getElementById('grabPageUrl').value = '';
    document.getElementById('grabStatus').style.display = 'none';
    document.getElementById('grabbedLinksContainer').style.display = 'none';
    document.getElementById('grabbedLinksList').innerHTML = '';
    document.getElementById('downloadGrabbedBtn').disabled = true;
    grabbedLinks = [];
});

document.getElementById('grabLinksBtn').addEventListener('click', async () => {
    const url = document.getElementById('grabPageUrl').value.trim();
    if (!url) return;

    const statusEl = document.getElementById('grabStatus');
    const statusText = document.getElementById('grabStatusText');
    const container = document.getElementById('grabbedLinksContainer');
    const listEl = document.getElementById('grabbedLinksList');

    statusEl.style.display = 'block';
    statusText.textContent = '⏳ Scanning page for download links...';
    statusText.style.color = '#f8d866';
    container.style.display = 'none';
    listEl.innerHTML = '';
    grabbedLinks = [];

    try {
        const res = await fetch('/api/v1/links/grab', {
            method: 'POST',
            body: JSON.stringify({ url: url })
        });
        const data = await res.json();

        if (!data.success) {
            statusText.textContent = '❌ ' + (data.error?.message || 'Failed to scan page');
            statusText.style.color = '#f5576c';
            return;
        }

        grabbedLinks = data.data.links || [];

        if (grabbedLinks.length === 0) {
            statusText.innerHTML = `⚠️ No direct links found. The site may require a captcha or timer.<br><br>
                <div style="margin-top: 10px; display: flex; gap: 10px; align-items: center;">
                    <button class="btn secondary" onclick="window.open('${url}', '_blank')">🌐 Open in Browser</button>
                    <span style="font-size: 0.8rem; color: #a5adc6;">Solve captchas, right-click the final download button, select "Copy Link", and Add URL.</span>
                </div>`;
            statusText.style.color = '#ffa726';
            return;
        }

        statusText.textContent = `✅ Found ${grabbedLinks.length} downloadable link(s)`;
        statusText.style.color = '#66bb6a';
        container.style.display = 'block';
        document.getElementById('grabbedLinksCount').textContent = `${grabbedLinks.length} files found`;

        grabbedLinks.forEach((link, i) => {
            const row = document.createElement('div');
            row.style.cssText = 'display: flex; align-items: center; gap: 10px; padding: 8px; border-bottom: 1px solid rgba(255,255,255,0.06);';
            row.innerHTML = `
                <input type="checkbox" id="grab-cb-${i}" class="grab-checkbox" checked style="width: 18px; height: 18px; accent-color: var(--primary); cursor: pointer;">
                <div style="flex: 1; min-width: 0;">
                    <div style="font-weight: 500; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;" title="${link.filename}">${link.filename}</div>
                    <div style="font-size: 0.75rem; color: var(--text-muted); white-space: nowrap; overflow: hidden; text-overflow: ellipsis;" title="${link.url}">${link.url}</div>
                </div>
            `;
            listEl.appendChild(row);
        });

        updateGrabDownloadBtn();
    } catch (e) {
        statusText.textContent = '❌ Network error: ' + e.message;
        statusText.style.color = '#f5576c';
    }
});

function updateGrabDownloadBtn() {
    const checked = document.querySelectorAll('.grab-checkbox:checked').length;
    const btn = document.getElementById('downloadGrabbedBtn');
    btn.disabled = checked === 0;
    btn.textContent = checked > 0 ? `Download Selected (${checked})` : 'Download Selected';
}

document.getElementById('grabbedLinksList').addEventListener('change', updateGrabDownloadBtn);

document.getElementById('selectAllGrabbed').addEventListener('click', () => {
    document.querySelectorAll('.grab-checkbox').forEach(cb => cb.checked = true);
    updateGrabDownloadBtn();
});

document.getElementById('deselectAllGrabbed').addEventListener('click', () => {
    document.querySelectorAll('.grab-checkbox').forEach(cb => cb.checked = false);
    updateGrabDownloadBtn();
});

document.getElementById('downloadGrabbedBtn').addEventListener('click', async () => {
    const selected = [];
    document.querySelectorAll('.grab-checkbox').forEach((cb, i) => {
        if (cb.checked && grabbedLinks[i]) {
            selected.push(grabbedLinks[i].url);
        }
    });

    if (selected.length === 0) return;

    for (const url of selected) {
        try {
            await fetch('/api/v1/downloads', {
                method: 'POST',
                body: JSON.stringify({ url: url })
            });
        } catch (e) {
            console.error('Failed to add download:', url, e);
        }
    }

    document.getElementById('linkGrabberModal').classList.remove('active');
    fetchInitialState();
});
