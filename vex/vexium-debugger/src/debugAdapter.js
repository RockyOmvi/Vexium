'use strict';
/**
 * Vexium Debug Adapter — implements the Debug Adapter Protocol (DAP)
 * over stdio.  VS Code spawns this script with Node.js and communicates
 * using Content-Length-framed JSON messages.
 *
 * The adapter spawns  vexium.exe debug <file> [--break N,N,...]
 * and translates the simple DBG: stderr protocol into DAP events.
 */

const { spawn, execFileSync } = require('child_process');
const path  = require('path');
const fs    = require('fs');

// ─────────────────────────────────────────────────────────────────────────
//  DAP Wire Protocol  (Content-Length framing over stdin / stdout)
// ─────────────────────────────────────────────────────────────────────────
class DAPProtocol {
  constructor(input, output) {
    this._in  = input;
    this._out = output;
    this._buf = Buffer.alloc(0);
    this._cb  = null;
    this._seq = 1;

    this._in.on('data', chunk => {
      this._buf = Buffer.concat([this._buf, chunk]);
      this._drain();
    });
  }

  onMessage(fn) { this._cb = fn; }

  _drain() {
    while (true) {
      const str = this._buf.toString('utf8');
      const hEnd = str.indexOf('\r\n\r\n');
      if (hEnd < 0) break;

      const m = str.substring(0, hEnd).match(/Content-Length:\s*(\d+)/i);
      if (!m) { this._buf = Buffer.alloc(0); break; }

      const len    = parseInt(m[1]);
      const needed = hEnd + 4 + len;
      if (this._buf.length < needed) break;

      const body = this._buf.slice(hEnd + 4, needed).toString('utf8');
      this._buf  = this._buf.slice(needed);

      try { const msg = JSON.parse(body); if (this._cb) this._cb(msg); }
      catch (_) { /* ignore malformed */ }
    }
  }

  send(msg) {
    const body = JSON.stringify(msg);
    const len  = Buffer.byteLength(body, 'utf8');
    this._out.write(`Content-Length: ${len}\r\n\r\n${body}`);
  }

  nextSeq() { return this._seq++; }
}

// ─────────────────────────────────────────────────────────────────────────
//  Vexium Debug Adapter
// ─────────────────────────────────────────────────────────────────────────
class VexiumDebugAdapter {
  constructor() {
    this._dap  = new DAPProtocol(process.stdin, process.stdout);
    this._dap.onMessage(msg => this._onMessage(msg));

    this._proc        = null;   // vexium child process
    this._stderrBuf   = '';     // partial stderr line buffer
    this._pausing     = false;  // waiting for DBG:VARS after DBG:STOP

    // State at last pause
    this._currentLine = 0;
    this._currentFile = '';
    this._cachedVars  = [];

    // Pending breakpoints from setBreakpoints (before launch)
    this._pendingBPs  = new Map();  // filePath -> [lineNumber]

    // Launch args
    this._programFile  = '';
    this._vexiumExe    = '';
    this._stopOnEntry  = true;
  }

  // ── helpers ──────────────────────────────────────────────────────────

  _response(req, body = {}) {
    this._dap.send({
      seq: this._dap.nextSeq(), type: 'response',
      request_seq: req.seq, success: true,
      command: req.command, body
    });
  }

  _errResponse(req, msg) {
    this._dap.send({
      seq: this._dap.nextSeq(), type: 'response',
      request_seq: req.seq, success: false,
      command: req.command, message: msg,
      body: { error: { id: 1, format: msg } }
    });
  }

  _event(name, body = {}) {
    this._dap.send({ seq: this._dap.nextSeq(), type: 'event', event: name, body });
  }

  _output(text, category = 'console') {
    this._event('output', { category, output: text });
  }

  // ── DAP message dispatch ─────────────────────────────────────────────

  _onMessage(msg) {
    if (msg.type !== 'request') return;
    switch (msg.command) {
      case 'initialize':              this._onInitialize(msg); break;
      case 'launch':                  this._onLaunch(msg);     break;
      case 'setBreakpoints':          this._onSetBPs(msg);     break;
      case 'setExceptionBreakpoints': this._response(msg, {}); break;
      case 'configurationDone':       this._onConfigDone(msg); break;
      case 'threads':                 this._onThreads(msg);    break;
      case 'stackTrace':              this._onStackTrace(msg); break;
      case 'scopes':                  this._onScopes(msg);     break;
      case 'variables':               this._onVariables(msg);  break;
      case 'continue':                this._onContinue(msg);   break;
      case 'next':                    this._onNext(msg);       break;
      case 'stepIn':                  this._onStepIn(msg);     break;
      case 'stepOut':                 this._onStepOut(msg);    break;
      case 'evaluate':                this._onEvaluate(msg);   break;
      case 'pause':                   this._response(msg, {}); break;
      case 'disconnect':
      case 'terminate':               this._onDisconnect(msg); break;
      default:                        this._response(msg, {}); break;
    }
  }

  // ── initialize ───────────────────────────────────────────────────────

  _onInitialize(req) {
    this._response(req, {
      supportsConfigurationDoneRequest:  true,
      supportsEvaluateForHovers:         true,
      supportTerminateDebuggee:          true,
      supportsSetBreakpointsRequest:     true,
    });
    // initialized event is sent AFTER launch wires up; see _onLaunch
  }

  // ── launch ───────────────────────────────────────────────────────────

  _findVexium(hint) {
    if (hint && fs.existsSync(hint)) return hint;

    // Common relative locations from the adapter script
    const base = path.resolve(__dirname, '..', '..');
    const candidates = [
      path.join(base, 'vex', 'vexium.exe'),
      path.join(base, 'vexium.exe'),
      path.join(base, '..', 'vex', 'vexium.exe'),
      path.join(__dirname, '..', '..', '..', 'vexium.exe'),
    ];
    for (const c of candidates) if (fs.existsSync(c)) return c;

    // Try system PATH
    try { execFileSync('vexium', ['--version'], { stdio: 'ignore' }); return 'vexium'; } catch (_) {}
    try { execFileSync('vexium.exe', ['--version'], { stdio: 'ignore' }); return 'vexium.exe'; } catch (_) {}
    return null;
  }

  _onLaunch(req) {
    const args = req.arguments || {};
    this._programFile = args.program || '';
    this._stopOnEntry = args.stopOnEntry !== false;
    const hint        = args.runtimePath || '';

    if (!this._programFile) { this._errResponse(req, '"program" is required in launch config'); return; }
    if (!fs.existsSync(this._programFile)) { this._errResponse(req, `File not found: ${this._programFile}`); return; }

    this._vexiumExe = this._findVexium(hint);
    if (!this._vexiumExe) {
      this._errResponse(req, 'vexium.exe not found. Set "runtimePath" in launch.json.');
      return;
    }

    this._response(req);

    // Notify VS Code to send breakpoints now
    this._event('initialized');
  }

  // ── setBreakpoints ───────────────────────────────────────────────────

  _onSetBPs(req) {
    const src  = (req.arguments || {}).source || {};
    const bps  = (req.arguments || {}).breakpoints || [];
    const file = src.path || '';

    this._pendingBPs.set(file, bps.map(b => b.line));

    // Return all as verified (optimistic before process starts)
    this._response(req, {
      breakpoints: bps.map(b => ({ verified: true, line: b.line }))
    });
  }

  // ── configurationDone ────────────────────────────────────────────────

  _onConfigDone(req) {
    this._response(req);
    this._launchProcess();
  }

  _launchProcess() {
    // Gather breakpoints for the target file and all open files
    const allBPs = [];
    for (const lines of this._pendingBPs.values()) allBPs.push(...lines);
    // Deduplicate and sort
    const bpArg = [...new Set(allBPs)].sort((a, b) => a - b).join(',');

    const vexArgs = ['debug', this._programFile];
    if (bpArg) vexArgs.push('--break', bpArg);

    this._output(`Launching: ${this._vexiumExe} ${vexArgs.join(' ')}\n`, 'console');

    this._proc = spawn(this._vexiumExe, vexArgs, {
      stdio: ['pipe', 'pipe', 'pipe'],
      cwd: path.dirname(this._programFile),
    });

    this._proc.stdout.on('data', data => {
      this._output(data.toString(), 'stdout');
    });

    this._proc.stderr.on('data', data => {
      this._stderrBuf += data.toString();
      this._processStderr();
    });

    this._proc.on('exit', code => {
      this._event('exited',     { exitCode: code || 0 });
      this._event('terminated', {});
      this._proc = null;
    });

    this._proc.on('error', err => {
      this._output(`Failed to launch vexium: ${err.message}\n`, 'stderr');
      this._event('terminated', {});
    });
  }

  // ── DBG: stderr protocol ─────────────────────────────────────────────

  _processStderr() {
    const lines = this._stderrBuf.split('\n');
    this._stderrBuf = lines.pop(); // keep incomplete last line

    for (const line of lines) {
      if (line.startsWith('DBG:READY')) {
        // Process is ready.  If stopOnEntry=false, just continue.
        if (!this._stopOnEntry) this._sendCmd('c\n');
        // If stopOnEntry=true, the runtime will already pause at line 1.

      } else if (line.startsWith('DBG:STOP:')) {
        // DBG:STOP:line|col|filepath   (| avoids splitting Windows drive letters)
        const payload = line.substring('DBG:STOP:'.length);
        const parts   = payload.split('|');
        this._currentLine = parseInt(parts[0]) || 0;
        // parts[1] = col (not used in DAP scopes)
        this._currentFile = parts.slice(2).join('|'); // re-join in case path had |
        this._pausing = true;
        // DBG:VARS comes on the very next line — handled below

      } else if (line.startsWith('DBG:VARS:')) {
        const json = line.substring('DBG:VARS:'.length);
        try   { this._cachedVars = JSON.parse(json); }
        catch { this._cachedVars = []; }

        if (this._pausing) {
          this._pausing = false;
          this._event('stopped', {
            reason: 'breakpoint',
            threadId: 1,
            allThreadsStopped: true,
          });
        }

      } else if (line.startsWith('DBG:EXIT:')) {
        const code = parseInt(line.substring('DBG:EXIT:'.length)) || 0;
        this._event('exited',     { exitCode: code });
        this._event('terminated', {});

      } else if (line.length > 0) {
        // Regular stderr text (parse errors, runtime errors)
        this._output(line + '\n', 'stderr');
      }
    }
  }

  _sendCmd(cmd) {
    if (this._proc && this._proc.stdin && !this._proc.stdin.destroyed)
      this._proc.stdin.write(cmd);
  }

  // ── threads / stack / scopes / variables ─────────────────────────────

  _onThreads(req) {
    this._response(req, { threads: [{ id: 1, name: 'main' }] });
  }

  _onStackTrace(req) {
    const frames = [];
    if (this._currentLine > 0) {
      frames.push({
        id: 1,
        name: path.basename(this._currentFile || 'unknown', '.vxm'),
        source: {
          name: path.basename(this._currentFile || ''),
          path: this._currentFile || undefined,
        },
        line:   this._currentLine,
        column: 0,
      });
    }
    this._response(req, { stackFrames: frames, totalFrames: frames.length });
  }

  _onScopes(req) {
    // One scope: local variables (variablesReference 1)
    this._response(req, {
      scopes: [{
        name: 'Variables',
        variablesReference: 1,
        expensive: false,
        presentationHint: 'locals',
      }]
    });
  }

  _onVariables(req) {
    const ref = (req.arguments || {}).variablesReference;
    let vars = [];
    if (ref === 1) {
      vars = (this._cachedVars || []).map(v => ({
        name: String(v.name),
        value: String(v.value),
        type: String(v.type),
        variablesReference: 0,
        presentationHint: { kind: 'data' },
      }));
    }
    this._response(req, { variables: vars });
  }

  // ── execution control ─────────────────────────────────────────────────

  _onContinue(req) {
    this._response(req, { allThreadsContinued: true });
    this._sendCmd('c\n');
  }

  _onNext(req) {
    this._response(req);
    this._sendCmd('n\n');
  }

  _onStepIn(req) {
    this._response(req);
    this._sendCmd('s\n');
  }

  _onStepOut(req) {
    this._response(req);
    this._sendCmd('o\n');
  }

  // ── evaluate (hover / watch) ──────────────────────────────────────────

  _onEvaluate(req) {
    const expr = ((req.arguments || {}).expression || '').trim();
    const v    = (this._cachedVars || []).find(x => x.name === expr);
    if (v) {
      this._response(req, { result: String(v.value), variablesReference: 0 });
    } else {
      this._response(req, { result: '<not in scope>', variablesReference: 0 });
    }
  }

  // ── disconnect / terminate ────────────────────────────────────────────

  _onDisconnect(req) {
    this._response(req);
    if (this._proc) {
      try { this._sendCmd('q\n'); } catch (_) {}
      const p = this._proc;
      this._proc = null;
      setTimeout(() => { try { p.kill(); } catch (_) {} }, 500);
    }
    setTimeout(() => process.exit(0), 600);
  }
}

// ─────────────────────────────────────────────────────────────────────────
//  Entry point
// ─────────────────────────────────────────────────────────────────────────
process.stdin.resume();
process.stdin.setEncoding(null); // binary mode for proper buffering
new VexiumDebugAdapter();
