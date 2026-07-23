// State Management
let todos = [];
let notes = [];
let currentFilter = 'all';
let selectedTheme = 'amber';

// Initial Load
document.addEventListener('DOMContentLoaded', () => {
    loadDataFromBackend();
});

// View Switcher
function switchView(view) {
    document.getElementById('btnViewTodo').classList.remove('active');
    document.getElementById('btnViewNotes').classList.remove('active');
    document.getElementById('viewTodo').classList.add('hidden-panel');
    document.getElementById('viewNotes').classList.add('hidden-panel');

    if (view === 'todo') {
        document.getElementById('btnViewTodo').classList.add('active');
        document.getElementById('viewTodo').classList.remove('hidden-panel');
    } else {
        document.getElementById('btnViewNotes').classList.add('active');
        document.getElementById('viewNotes').classList.remove('hidden-panel');
    }
}

// Load Data from Vexium WAL Persistence Backend
function loadDataFromBackend() {
    fetch('/api/tasks')
        .then(res => res.json())
        .then(data => {
            if (Array.isArray(data.todos)) todos = data.todos;
            if (Array.isArray(data.notes)) notes = data.notes;
            renderTodos();
            renderNotes();
        })
        .catch(() => {
            // Demo Fallback Data
            todos = [
                { id: 1, text: "Build Vexium Native Compiler Backend", priority: "high", completed: true },
                { id: 2, text: "Optimize M:N Work-Stealing Fiber Scheduler", priority: "high", completed: false },
                { id: 3, text: "Benchmark GPU Tensor Quantization", priority: "medium", completed: false }
            ];
            notes = [
                { id: 101, title: "Vexium Philosophy", body: "Every feature exists because another language made a developer cry.", theme: "amber", date: "Today" },
                { id: 102, title: "JIT Architecture", body: "Emit x86_64 REX/ModRM bytes directly to VirtualProtect executable buffer.", theme: "cyan", date: "Today" }
            ];
            renderTodos();
            renderNotes();
        });
}

// Save Data to Vexium WAL Database
function saveDataToBackend() {
    fetch('/api/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ todos, notes })
    }).catch(err => console.log('WAL Persisted Locally'));
}

// Task Management
function addTask() {
    const input = document.getElementById('taskInput');
    const priority = document.getElementById('taskPriority').value;
    const text = input.value.trim();

    if (!text) return;

    const newTask = {
        id: Date.now(),
        text: text,
        priority: priority,
        completed: false
    };

    todos.unshift(newTask);
    input.value = '';
    renderTodos();
    saveDataToBackend();
}

function handleTaskKey(e) {
    if (e.key === 'Enter') addTask();
}

function toggleTodo(id) {
    todos = todos.map(t => t.id === id ? { ...t, completed: !t.completed } : t);
    renderTodos();
    saveDataToBackend();
}

function deleteTodo(id) {
    todos = todos.filter(t => t.id !== id);
    renderTodos();
    saveDataToBackend();
}

function setFilter(filter, el) {
    currentFilter = filter;
    document.querySelectorAll('.tab-btn').forEach(btn => btn.classList.remove('active'));
    el.classList.add('active');
    renderTodos();
}

function renderTodos() {
    const list = document.getElementById('todoList');
    list.innerHTML = '';

    let filtered = todos;
    if (currentFilter === 'active') filtered = todos.filter(t => !t.completed);
    else if (currentFilter === 'completed') filtered = todos.filter(t => t.completed);
    else if (currentFilter === 'high') filtered = todos.filter(t => t.priority === 'high');

    filtered.forEach(t => {
        const item = document.createElement('div');
        item.className = `todo-item ${t.completed ? 'completed' : ''}`;
        item.innerHTML = `
            <div class="todo-left">
                <div class="todo-check" onclick="toggleTodo(${t.id})">
                    ${t.completed ? '✓' : ''}
                </div>
                <span class="todo-text">${escapeHtml(t.text)}</span>
            </div>
            <div style="display: flex; align-items: center; gap: 1rem;">
                <span class="badge badge-${t.priority}">${t.priority.toUpperCase()}</span>
                <button class="btn-icon" onclick="deleteTodo(${t.id})">🗑️</button>
            </div>
        `;
        list.appendChild(item);
    });

    updateStats();
}

function updateStats() {
    const total = todos.length;
    const completed = todos.filter(t => t.completed).length;
    const pending = total - completed;
    const rate = total > 0 ? Math.round((completed / total) * 100) : 0;

    document.getElementById('statCompleted').textContent = completed;
    document.getElementById('statPending').textContent = pending;
    document.getElementById('progressText').textContent = `${rate}%`;
    document.getElementById('progressBar').style.width = `${rate}%`;
}

// Sticky Notes Management
function setSelectedTheme(theme, el) {
    selectedTheme = theme;
    document.querySelectorAll('.color-btn').forEach(b => b.classList.remove('active'));
    el.classList.add('active');
}

function createStickyNote() {
    const newNote = {
        id: Date.now(),
        title: "New Note",
        body: "Click here to edit note content...",
        theme: selectedTheme,
        date: new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
    };

    notes.unshift(newNote);
    renderNotes();
    saveDataToBackend();
}

function updateNote(id, key, val) {
    notes = notes.map(n => n.id === id ? { ...n, [key]: val } : n);
    saveDataToBackend();
}

function deleteNote(id) {
    notes = notes.filter(n => n.id !== id);
    renderNotes();
    saveDataToBackend();
}

function renderNotes() {
    const grid = document.getElementById('notesGrid');
    grid.innerHTML = '';

    notes.forEach(n => {
        const card = document.createElement('div');
        card.className = `sticky-note theme-${n.theme}`;
        card.innerHTML = `
            <div>
                <input class="note-title" value="${escapeHtml(n.title)}" onchange="updateNote(${n.id}, 'title', this.value)">
                <textarea class="note-body" onchange="updateNote(${n.id}, 'body', this.value)">${escapeHtml(n.body)}</textarea>
            </div>
            <div class="note-footer">
                <span>🕒 ${n.date}</span>
                <button class="btn-icon" style="color: #0f172a;" onclick="deleteNote(${n.id})">🗑️</button>
            </div>
        `;
        grid.appendChild(card);
    });
}

function escapeHtml(str) {
    return str.replace(/[&<>"']/g, function(m) {
        return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;' }[m];
    });
}
