// ROBOMONGO MONGOSH PREAMBLE — injected once at shell startup
// Protocol version: 2
(function () {
    'use strict';
    const _print = (...args) => print(...args);

    // ── Type detection ────────────────────────────────────────────────────────

    function _isCursor(val) {
        if (!val || typeof val !== 'object') return false;
        const name = val.constructor ? val.constructor.name : '';
        return (
            name === 'Cursor' || name === 'DBCursor' || name === 'AggregationCursor' ||
            name === 'CommandCursor' ||
            (typeof val.hasNext === 'function' && typeof val.toArray === 'function')
        );
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

    function _serialize(val) {
        if (val === undefined || val === null) return null;
        try { return EJSON.serialize(val); } catch (_) { return { __robo_text: String(val) }; }
    }

    function _classify(val, capturedPrints) {
        if (val === undefined) {
            if (capturedPrints.length > 0) return { type: 'text', text: capturedPrints.join('\n') };
            return null;
        }
        if (_isCursor(val)) {
            const meta = val.__robo_meta || {};
            let docs = [];
            try {
                docs = val.limit(globalThis.__robo_batchSize || 50).toArray().map(d => _serialize(d));
            } catch (e) {
                return { type: 'error', message: e.message, code: e.code || 0 };
            }
            return {
                type: 'query', ns: meta.ns || '',
                query: meta.query ? EJSON.stringify(meta.query) : '{}',
                projection: meta.projection ? EJSON.stringify(meta.projection) : '{}',
                sort: meta.sort ? EJSON.stringify(meta.sort) : '{}',
                skip: meta.skip || 0, limit: meta.limit !== undefined ? meta.limit : -1,
                docs: docs
            };
        }
        if (_isWriteResult(val)) return { type: 'value', value: _serialize(val) };
        if (Array.isArray(val)) return { type: 'value', value: val.map(d => _serialize(d)) };
        if (typeof val === 'object') return { type: 'value', value: _serialize(val) };
        return { type: 'text', text: String(val) };
    }

    // ── Statement splitter ────────────────────────────────────────────────────
    //
    // Splits a JS script into top-level statements without an external parser.
    // Tracks string/template/comment/regex/bracket state to avoid false splits
    // on semicolons or newlines inside those constructs.
    //
    // Split rules (only at bracket depth 0):
    //   1. Explicit semicolon (;)
    //   2. Newline, when the preceding non-whitespace character is a valid
    //      statement-ender AND the first non-whitespace on the next line does
    //      not look like a binary-operator continuation (.  (  [  +  -  *  /
    //      %  &  |  ^  ?  :  ,).
    //
    // Limitation: regex-after-identifier (e.g. `return /re/`) is mis-classified
    // as division, which is acceptable for typical MongoDB shell scripts.

    function _splitStatements(src) {
        const stmts = [];
        const n = src.length;
        let i = 0, start = 0;
        // States: N=normal LC=line-comment BC=block-comment
        //         Sq=single-quote Dq=double-quote Tl=template Rx=regex
        let state = 'N';
        let depth = 0;
        const tmplStack = []; // depth level when a ${...} template expression opened
        let lastNonWs = ''; // last non-whitespace char seen in Normal state

        // A character that can legitimately end a statement.
        function canEndStmt(c) { return /[\w$)\]'"`/]/.test(c); }

        // Characters that, when appearing at the start of a new line, suggest
        // the previous line is not yet a complete statement.
        function isContinuation(c) { return /^[.([+\-*/%&|^?:,]/.test(c); }

        // First non-whitespace character after position i.
        function peekNext() {
            let j = i + 1;
            while (j < n && /\s/.test(src[j])) j++;
            return src[j] || '';
        }

        function flush(pos) {
            const s = src.slice(start, pos).trim();
            if (s) stmts.push(s);
            start = pos + 1; // skip the delimiter character
            lastNonWs = '';
        }

        while (i < n) {
            const c = src[i];

            if (state === 'LC') {
                if (c === '\n') {
                    state = 'N';
                    // Treat the comment-closing newline as a potential statement split
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
                if (c === '[') { // character class — read until ]
                    i++;
                    while (i < n && src[i] !== ']') { if (src[i] === '\\') i++; i++; }
                    i++; continue;
                }
                if (c === '/') {
                    // end of regex: skip optional flags
                    while (i + 1 < n && /[gimsuy]/.test(src[i + 1])) i++;
                    state = 'N'; lastNonWs = '/'; i++; continue;
                }
                if (c === '\n') { state = 'N'; i++; continue; } // unterminated regex
                i++; continue;
            }

            // ── Normal state ──────────────────────────────────────────────────

            // Close a template-literal expression (${...}) when depth returns
            if (c === '}' && tmplStack.length > 0 && depth === tmplStack[tmplStack.length - 1] + 1) {
                tmplStack.pop(); depth--; state = 'Tl'; i++; continue;
            }

            if (c === '/' && src[i + 1] === '/') { state = 'LC'; i += 2; continue; }
            if (c === '/' && src[i + 1] === '*') { state = 'BC'; i += 2; continue; }

            // / is a regex literal when preceded by an operator or at start of expression
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

            // Rule 1: explicit semicolon at top level
            if (c === ';' && depth === 0) {
                flush(i); i++; continue;
            }

            // Rule 2: newline-triggered ASI at top level
            if (c === '\n' && depth === 0 && canEndStmt(lastNonWs)) {
                const next = peekNext();
                if (next && !isContinuation(next)) {
                    flush(i); i++; continue;
                }
            }

            if (/\S/.test(c)) lastNonWs = c;
            i++;
        }

        const tail = src.slice(start).trim();
        if (tail) stmts.push(tail);
        return stmts;
    }

    // ── Collection.find() intercept ───────────────────────────────────────────
    // mongosh 2.x uses the class name "Collection" (not "DBCollection"), and
    // it's not a global — reach its prototype through a live collection object.

    try {
        const _collProto = Object.getPrototypeOf(db.getCollection('__robodummy__'));
        const _origFind = _collProto.find;
        _collProto.find = function (query, projection) {
            const cursor = _origFind.apply(this, arguments);
            try {
                cursor.__robo_meta = { ns: this.getFullName(), query: query || {},
                    projection: projection || {}, sort: {}, skip: 0, limit: -1 };
            } catch (_) {}
            return cursor;
        };
    } catch (_) {}

    // ── __robo_exec ───────────────────────────────────────────────────────────
    //
    // Splits the user's script into top-level statements and evaluates each
    // one separately, collecting a typed result per statement.
    //
    // Indirect eval ((0, eval)) is used so that `var` declarations from one
    // statement are visible in subsequent statements (they go onto globalThis,
    // which is the mongosh REPL context object where `db` etc. also live).
    // `let`/`const` do not propagate across statements; users should use `var`
    // for cross-statement variables, which is standard MongoDB shell practice.

    globalThis.__robo_exec = function (scriptB64, execId) {
        const script = Buffer.from(scriptB64, 'base64').toString('utf8');
        const START = '<<<ROBO_START_' + execId + '>>>';
        const END   = '<<<ROBO_END_'   + execId + '>>>';

        const stmts = _splitStatements(script);
        const output = [];

        // Capture print/printjson output per statement; save originals once.
        const origPrint     = globalThis.print;
        const origPrintjson = globalThis.printjson;

        for (const stmt of stmts) {
            // Handle the "use <dbname>" shell command (not valid JS, handled here)
            const useMatch = stmt.trim().match(/^use\s+(\S+)$/);
            if (useMatch) {
                try {
                    db = db.getSiblingDB(useMatch[1]);
                    output.push({ type: 'text', text: 'switched to db ' + useMatch[1] });
                } catch (e) {
                    output.push({ type: 'error', message: e.message, code: e.code || 0, codeName: e.codeName || '' });
                    break;
                }
                continue;
            }

            const captured = [];
            globalThis.print     = function () { captured.push(Array.from(arguments).join(' ')); };
            globalThis.printjson = function (v) { captured.push(tojson(v)); };

            let result, error;
            try {
                result = (0, eval)(stmt);  // eslint-disable-line no-eval
            } catch (e) {
                error = { type: 'error', message: e.message, code: e.code || 0, codeName: e.codeName || '' };
            }

            // Always restore before processing results (even on error path)
            globalThis.print     = origPrint;
            globalThis.printjson = origPrintjson;

            if (error) {
                output.push(error);
                break; // fail fast on error — same behaviour as original Robomongo
            }

            const classified = _classify(result, captured);
            if (classified) output.push(classified);
            if (captured.length > 0 && classified && classified.type !== 'text')
                output.push({ type: 'text', text: captured.join('\n') });
        }

        _print(START);
        _print(JSON.stringify(output));
        _print(END);
    };

    // ── __robo_use ────────────────────────────────────────────────────────────

    globalThis.__robo_use = function (dbNameB64) {
        const name = Buffer.from(dbNameB64, 'base64').toString('utf8');
        try { db = db.getSiblingDB(name); _print('<<<ROBO_USE_OK>>>'); }
        catch (e) { _print('<<<ROBO_USE_ERR>>>' + e.message); }
    };

    // ── __robo_ping ───────────────────────────────────────────────────────────

    globalThis.__robo_ping = function () {
        try { db.runCommand({ ping: 1 }); _print('<<<ROBO_PING_OK>>>'); }
        catch (e) { _print('<<<ROBO_PING_ERR>>>' + e.message); }
    };

    // ── __robo_complete ───────────────────────────────────────────────────────

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

    // ── __robo_set_batch ──────────────────────────────────────────────────────

    globalThis.__robo_set_batch = function (n) {
        globalThis.__robo_batchSize = n;
        _print('<<<ROBO_BATCH_OK>>>');
    };

    _print('<<<ROBO_PREAMBLE_READY>>>');
})();
