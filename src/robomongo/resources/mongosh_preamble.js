// ROBOMONGO MONGOSH PREAMBLE — injected once at shell startup
// Protocol version: 4
//
// ── Execution model ─────────────────────────────────────────────────────────
//
// mongosh runs every REPL input through its async-rewriter (babel), which turns
// the asynchronous shell API (find, aggregate, cursor.toArray, hasNext, next, …)
// into synchronous-looking calls backed by Atomics.wait. That rewriting only
// happens for code that arrives as REPL input text — NOT for code passed to a
// plain `eval`, `new Function`, or `require`d module.
//
// Earlier versions ran the user's statements via `(0, eval)`, which is not
// rewritten. There the shell API stays async: `coll.find({})` is a
// Promise<Cursor> and `var v = coll.find({}).toArray()` leaves `v` holding a
// Promise instead of an array. To match the original Robomongo (synchronous
// shell semantics), the user's code must be executed as REPL input so the
// rewriter processes it.
//
// Flow per execution (driven from MongoshEngine.cpp):
//   1. C++ → __robo_prepare("<base64 script>", "<id>")
//        We split the script into statements, wrap each so its value is
//        captured (__robo_push), install print capture, reset state, and emit
//        the wrapped source (base64) back to C++.
//   2. C++ decodes the wrapped source and sends it verbatim as REPL input, so
//        mongosh rewrites it → cursors/toArray run synchronously, `var`s persist
//        and stay usable across the script's lines. The wrapped source ends by
//        calling __robo_emit("<id>").
//   3. __robo_emit classifies the captured values (draining cursors for the
//        first page) and prints a sentinel-framed JSON result array.

(function () {
    'use strict';
    // Bind to the original print: __robo_prepare temporarily overrides
    // globalThis.print to capture user output, but our sentinels must always go
    // to the real stdout.
    const _realPrint = print;
    const _print = function () { return _realPrint.apply(null, arguments); };

    // ── Serialisation / type detection ─────────────────────────────────────────

    function _serialize(val) {
        if (val === undefined || val === null) return null;
        try { return EJSON.serialize(val); } catch (_) { return { __robo_text: String(val) }; }
    }

    function _isWriteResult(val) {
        if (!val || typeof val !== 'object') return false;
        const name = val.constructor ? val.constructor.name : '';
        return (
            name === 'WriteResult' || name === 'BulkWriteResult' ||
            name === 'InsertOneResult' || name === 'InsertManyResult' ||
            name === 'UpdateResult' || name === 'DeleteResult'
        );
    }

    // ── Cursor metadata registry ────────────────────────────────────────────────
    // find/aggregate record paging metadata here, keyed by the returned cursor.
    // _classify (in __robo_emit) looks it up to build a pageable "query" result.

    const _metaReg = new WeakMap();

    function _buildQuery(meta, serializedDocs) {
        const result = {
            type: 'query',
            ns: meta.ns || '',
            query: meta.query ? EJSON.stringify(meta.query) : '{}',
            projection: meta.projection ? EJSON.stringify(meta.projection) : '{}',
            sort: meta.sort ? EJSON.stringify(meta.sort) : '{}',
            skip: meta.skip || 0,
            limit: meta.limit !== undefined ? meta.limit : -1,
            docs: serializedDocs
        };
        if (meta.pipeline !== undefined)
            result.pipeline = EJSON.stringify({ p: meta.pipeline });
        return result;
    }

    // ── Statement splitter ────────────────────────────────────────────────────
    //
    // Splits a JS script into top-level statements without an external parser.
    // Tracks string/template/comment/regex/bracket state to avoid false splits
    // on semicolons or newlines inside those constructs.

    function _splitStatements(src) {
        const stmts = [];
        const n = src.length;
        let i = 0, start = 0;
        let state = 'N';            // N normal, LC line-comment, BC block-comment,
                                    // Sq/Dq/Tl/Rx string/template/regex
        let depth = 0;
        const tmplStack = [];
        let lastNonWs = '';

        function canEndStmt(c) { return /[\w$)\]'"`/]/.test(c); }
        function isContinuation(c) { return /^[.([+\-*/%&|^?:,]/.test(c); }
        function peekNext() {
            let j = i + 1;
            while (j < n && /\s/.test(src[j])) j++;
            return src[j] || '';
        }
        function flush(pos) {
            const s = src.slice(start, pos).trim();
            if (s) stmts.push(s);
            start = pos + 1;
            lastNonWs = '';
        }

        while (i < n) {
            const c = src[i];

            if (state === 'LC') {
                if (c === '\n') {
                    state = 'N';
                    if (depth === 0 && canEndStmt(lastNonWs)) {
                        const next = peekNext();
                        if (next && !isContinuation(next)) { flush(i); i++; continue; }
                    }
                }
                i++; continue;
            }
            if (state === 'BC') {
                if (c === '*' && src[i + 1] === '/') { state = 'N'; i += 2; } else i++;
                continue;
            }
            if (state === 'Sq') {
                if (c === '\\') i++;
                else if (c === "'") { state = 'N'; lastNonWs = "'"; }
                i++; continue;
            }
            if (state === 'Dq') {
                if (c === '\\') i++;
                else if (c === '"') { state = 'N'; lastNonWs = '"'; }
                i++; continue;
            }
            if (state === 'Tl') {
                if (c === '\\') { i += 2; continue; }
                if (c === '`') { state = 'N'; lastNonWs = '`'; i++; continue; }
                if (c === '$' && src[i + 1] === '{') {
                    tmplStack.push(depth); depth++;
                    state = 'N'; i += 2; continue;
                }
                i++; continue;
            }
            if (state === 'Rx') {
                if (c === '\\') { i += 2; continue; }
                if (c === '[') {
                    i++;
                    while (i < n && src[i] !== ']') { if (src[i] === '\\') i++; i++; }
                    i++; continue;
                }
                if (c === '/') {
                    while (i + 1 < n && /[gimsuy]/.test(src[i + 1])) i++;
                    state = 'N'; lastNonWs = '/'; i++; continue;
                }
                if (c === '\n') { state = 'N'; i++; continue; }
                i++; continue;
            }

            // Normal state
            if (c === '}' && tmplStack.length > 0 && depth === tmplStack[tmplStack.length - 1] + 1) {
                tmplStack.pop(); depth--; state = 'Tl'; i++; continue;
            }
            if (c === '/' && src[i + 1] === '/') { state = 'LC'; i += 2; continue; }
            if (c === '/' && src[i + 1] === '*') { state = 'BC'; i += 2; continue; }
            if (c === '/') {
                if (!lastNonWs || /[=+\-*%&|^~<>!?:,;({[\n]/.test(lastNonWs)) {
                    state = 'Rx'; i++; continue;
                }
                lastNonWs = '/'; i++; continue;
            }
            if (c === "'") { state = 'Sq'; i++; continue; }
            if (c === '"') { state = 'Dq'; i++; continue; }
            if (c === '`') { state = 'Tl'; i++; continue; }
            if (c === '(' || c === '[' || c === '{') { depth++; lastNonWs = c; i++; continue; }
            if (c === ')' || c === ']')               { depth--; lastNonWs = c; i++; continue; }
            if (c === '}')                             { depth--; lastNonWs = c; i++; continue; }
            if (c === ';' && depth === 0) { flush(i); i++; continue; }
            if (c === '\n' && depth === 0 && canEndStmt(lastNonWs)) {
                const next = peekNext();
                if (next && !isContinuation(next)) { flush(i); i++; continue; }
            }
            if (/\S/.test(c)) lastNonWs = c;
            i++;
        }

        const tail = src.slice(start).trim();
        if (tail) stmts.push(tail);
        return stmts;
    }

    // A statement whose completion value we want to capture (i.e. not a
    // declaration or control-flow statement that has no useful value).
    function _isExprStmt(s) {
        return !/^(var|let|const|function|class|if|for|while|do|switch|try|return|throw|break|continue|with|debugger|import|export)\b/.test(s)
            && !/^[{;]/.test(s);
    }

    // ── Debug logging ─────────────────────────────────────────────────────────
    const _ROBO_DEBUG = false;
    const _debugBuf = [];
    function _roboLog(msg) { if (_ROBO_DEBUG) _debugBuf.push(msg); }
    function _flushLogs() {
        if (!_ROBO_DEBUG) return;
        for (const m of _debugBuf) _print('<<<ROBO_LOG>>>' + m + '<<<ROBO_LOG_END>>>');
        _debugBuf.length = 0;
    }

    // ── Collection / cursor intercepts ──────────────────────────────────────────
    // These run rewritten (the preamble is REPL input). find/aggregate therefore
    // resolve to real cursors when called from rewritten user code; we just record
    // paging metadata. Cursor chain methods update that metadata so paging honours
    // .sort()/.skip()/.limit()/.projection().

    try {
        const collProto = Object.getPrototypeOf(db.getCollection('__robodummy__'));

        const origFind = collProto.find;
        collProto.find = function (query, projection) {
            const cursor = origFind.apply(this, arguments);
            try {
                _metaReg.set(cursor, {
                    ns: this.getFullName(),
                    query: query || {}, projection: projection || {},
                    sort: {}, skip: 0, limit: -1
                });
            } catch (_) {}
            return cursor;
        };

        const origAggregate = collProto.aggregate;
        collProto.aggregate = function () {
            const cursor = origAggregate.apply(this, arguments);
            try {
                _metaReg.set(cursor, {
                    ns: this.getFullName(),
                    pipeline: arguments[0] || [], skip: 0
                });
            } catch (_) {}
            return cursor;
        };

        // Patch cursor chain methods so paging metadata tracks them. Obtain the
        // cursor prototype from a sample cursor (resolved synchronously here
        // because the preamble is rewritten).
        const sampleCursor = collProto.find.call(db.getCollection('__robodummy__'), {});
        const curProto = Object.getPrototypeOf(sampleCursor);
        const chainMethods = [['sort', 'sort'], ['skip', 'skip'], ['limit', 'limit'],
                              ['projection', 'projection'], ['project', 'projection']];
        for (const [method, metaKey] of chainMethods) {
            const orig = curProto[method];
            if (typeof orig !== 'function') continue;
            curProto[method] = function (arg) {
                try { const m = _metaReg.get(this); if (m) m[metaKey] = arg; } catch (_) {}
                return orig.apply(this, arguments);
            };
        }
    } catch (e) {
        _roboLog('intercept install failed: ' + (e && e.message ? e.message : String(e)));
    }

    // ── Per-execution capture state ──────────────────────────────────────────────

    let _items = [];          // captured statement values, in order
    let _pendingPrints = [];  // print() output awaiting the next push/flush
    let _error = null;        // first uncaught error (stops the script)
    let _origPrint = null;
    let _origPrintjson = null;

    function _flushPrints() {
        if (_pendingPrints.length) {
            _items.push({ kind: 'prints', data: _pendingPrints });
            _pendingPrints = [];
        }
    }

    // Called by the wrapped script to capture each expression statement's value.
    globalThis.__robo_push = function (val) {
        _flushPrints();
        _items.push({ kind: 'value', data: val });
    };
    // Capture a plain status line (e.g. "switched to db x").
    globalThis.__robo_text = function (text) {
        _flushPrints();
        _items.push({ kind: 'text', data: text });
    };

    // ── __robo_prepare ───────────────────────────────────────────────────────────
    // Build the wrapped, REPL-rewritable source for one execution and hand it back
    // to C++ (base64) to be sent as REPL input.

    globalThis.__robo_prepare = function (scriptB64, execId) {
        const script = Buffer.from(scriptB64, 'base64').toString('utf8');
        const stmts = _splitStatements(script);

        // Reset capture state and install print interception for the upcoming run.
        _items = [];
        _pendingPrints = [];
        _error = null;
        _origPrint = globalThis.print;
        _origPrintjson = globalThis.printjson;
        // print(): primitives coalesce into text lines (so loops stay one block),
        // but an object/array argument is captured as a value so it renders as a
        // document/array tree — matching original Robomongo's print(doc).
        globalThis.print = function () {
            const args = Array.from(arguments);
            if (args.length && args.every(a => a === null || typeof a !== 'object')) {
                _pendingPrints.push(args.map(a => String(a)).join(' '));
                return;
            }
            for (const a of args) {
                if (a !== null && typeof a === 'object') { _flushPrints(); _items.push({ kind: 'value', data: a }); }
                else _pendingPrints.push(String(a));
            }
        };
        globalThis.printjson = function (v) {
            if (v !== null && typeof v === 'object') { _flushPrints(); _items.push({ kind: 'value', data: v }); }
            else _pendingPrints.push(String(v));
        };

        let body = '';
        for (const raw of stmts) {
            const s = raw.trim();
            if (!s) continue;
            const useMatch = s.match(/^use\s+(\S+)$/);
            if (useMatch) {
                const name = JSON.stringify(useMatch[1]);
                body += 'db = db.getSiblingDB(' + name + '); __robo_text("switched to db " + ' + name + ');\n';
            } else if (_isExprStmt(s)) {
                body += '__robo_push(\n(' + s + ')\n);\n';
            } else {
                body += s + ';\n';
            }
        }

        const wrapped =
            'try {\n' + body + '} catch (__robo_e) {\n' +
            '  globalThis.__robo_set_error(__robo_e);\n' +
            '}\n' +
            '__robo_emit(' + JSON.stringify(execId) + ');\n';

        _print('<<<ROBO_PREP_' + execId + '>>>' +
               Buffer.from(wrapped, 'utf8').toString('base64') +
               '<<<ROBO_PREP_END_' + execId + '>>>');
    };

    globalThis.__robo_set_error = function (e) {
        _error = {
            type: 'error',
            message: (e && e.message) ? e.message : String(e),
            code: (e && e.code) || 0,
            codeName: (e && e.codeName) || ''
        };
    };

    // ── __robo_emit ───────────────────────────────────────────────────────────────
    // Classify captured values (draining cursors for the first page) and print the
    // sentinel-framed JSON result array. Runs rewritten, so cursor iteration here is
    // synchronous (Atomics-backed).

    globalThis.__robo_emit = function (execId) {
        // Restore print first, so result serialisation can't be captured.
        if (_origPrint) globalThis.print = _origPrint;
        if (_origPrintjson) globalThis.printjson = _origPrintjson;
        _flushPrints();

        const batchSize = globalThis.__robo_batchSize || 50;
        const output = [];

        for (const item of _items) {
            if (item.kind === 'prints') {
                output.push({ type: 'text', text: item.data.join('\n') });
                continue;
            }
            if (item.kind === 'text') {
                output.push({ type: 'text', text: item.data });
                continue;
            }
            const val = item.data;
            if (val === undefined) continue;

            if (Array.isArray(val)) {
                output.push({ type: 'array', docs: val.map(d => _serialize(d)) });
            } else if (val && typeof val === 'object' && _metaReg.has(val)) {
                const meta = _metaReg.get(val);
                const docs = [];
                try {
                    let count = 0;
                    while (count < batchSize && val.hasNext()) {
                        docs.push(_serialize(val.next()));
                        count++;
                    }
                } catch (e) {
                    output.push({ type: 'error', message: e.message, code: e.code || 0 });
                    continue;
                }
                output.push(_buildQuery(meta, docs));
            } else if (_isWriteResult(val)) {
                output.push({ type: 'value', value: _serialize(val) });
            } else if (val && typeof val === 'object') {
                output.push({ type: 'value', value: _serialize(val) });
            } else {
                output.push({ type: 'text', text: String(val) });
            }
        }

        if (_error) output.push(_error);

        _flushLogs();
        _print('<<<ROBO_START_' + execId + '>>>');
        _print(JSON.stringify(output));
        _print('<<<ROBO_END_' + execId + '>>>');

        // Clear state for the next run.
        _items = [];
        _pendingPrints = [];
        _error = null;
    };

    // ── __robo_use ──────────────────────────────────────────────────────────────

    globalThis.__robo_use = function (dbNameB64) {
        const name = Buffer.from(dbNameB64, 'base64').toString('utf8');
        try { db = db.getSiblingDB(name); _print('<<<ROBO_USE_OK>>>'); }
        catch (e) { _print('<<<ROBO_USE_ERR>>>' + e.message); }
    };

    // ── __robo_ping ───────────────────────────────────────────────────────────────

    globalThis.__robo_ping = function () {
        try { db.runCommand({ ping: 1 }); _print('<<<ROBO_PING_OK>>>'); }
        catch (e) { _print('<<<ROBO_PING_ERR>>>' + e.message); }
    };

    // ── __robo_complete ───────────────────────────────────────────────────────────

    globalThis.__robo_complete = function (prefixB64) {
        const prefix = Buffer.from(prefixB64, 'base64').toString('utf8');
        const results = [];
        try {
            if (prefix.startsWith('db.')) {
                const partial = prefix.slice(3);
                for (const c of db.getCollectionNames())
                    if (c.startsWith(partial)) results.push('db.' + c);
            }
        } catch (_) {}
        _print('<<<ROBO_COMPLETE>>>' + JSON.stringify(results) + '<<<ROBO_COMPLETE_END>>>');
    };

    // ── __robo_set_batch ──────────────────────────────────────────────────────────

    globalThis.__robo_set_batch = function (n) {
        globalThis.__robo_batchSize = n;
        _print('<<<ROBO_BATCH_OK>>>');
    };

    _print('<<<ROBO_PREAMBLE_READY>>>');
})();
