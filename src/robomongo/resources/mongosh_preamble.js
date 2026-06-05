// ROBOMONGO MONGOSH PREAMBLE — injected once at shell startup
// Protocol version: 1
(function () {
    'use strict';
    const _print = (...args) => print(...args);

    function _isCursor(val) {
        if (!val || typeof val !== 'object') return false;
        const name = val.constructor ? val.constructor.name : '';
        return (
            name === 'Cursor' || name === 'DBCursor' || name === 'AggregationCursor' ||
            name === 'CommandCursor' ||
            (typeof val.hasNext === 'function' && typeof val.toArray === 'function')
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
        if (Array.isArray(val)) return { type: 'value', value: val.map(d => _serialize(d)) };
        if (typeof val === 'object') return { type: 'value', value: _serialize(val) };
        return { type: 'text', text: String(val) };
    }

    try {
        const _origFind = DBCollection.prototype.find;
        DBCollection.prototype.find = function (query, projection) {
            const cursor = _origFind.apply(this, arguments);
            cursor.__robo_meta = { ns: this.getFullName(), query: query || {},
                projection: projection || {}, sort: {}, skip: 0, limit: -1 };
            return cursor;
        };
    } catch (_) {}

    globalThis.__robo_exec = function (scriptB64, execId) {
        const script = Buffer.from(scriptB64, 'base64').toString('utf8');
        const START = '<<<ROBO_START_' + execId + '>>>';
        const END   = '<<<ROBO_END_'   + execId + '>>>';
        const captured = [];
        const origPrint = globalThis.print;
        const origPrintjson = globalThis.printjson;
        globalThis.print = function () { captured.push(Array.from(arguments).join(' ')); };
        globalThis.printjson = function (v) { captured.push(tojson(v)); };
        let result, error;
        try { result = eval(script); } catch (e) {
            error = { type: 'error', message: e.message, code: e.code || 0, codeName: e.codeName || '' };
        } finally {
            globalThis.print = origPrint;
            globalThis.printjson = origPrintjson;
        }
        const output = [];
        if (error) { output.push(error); }
        else {
            const classified = _classify(result, captured);
            if (classified) output.push(classified);
            if (captured.length > 0 && classified && classified.type !== 'text')
                output.push({ type: 'text', text: captured.join('\n') });
        }
        _print(START);
        _print(JSON.stringify(output));
        _print(END);
    };

    globalThis.__robo_use = function (dbNameB64) {
        const name = Buffer.from(dbNameB64, 'base64').toString('utf8');
        try { db = db.getSiblingDB(name); _print('<<<ROBO_USE_OK>>>'); }
        catch (e) { _print('<<<ROBO_USE_ERR>>>' + e.message); }
    };

    globalThis.__robo_ping = function () {
        try { db.runCommand({ ping: 1 }); _print('<<<ROBO_PING_OK>>>'); }
        catch (e) { _print('<<<ROBO_PING_ERR>>>' + e.message); }
    };

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

    globalThis.__robo_set_batch = function (n) {
        globalThis.__robo_batchSize = n;
        _print('<<<ROBO_BATCH_OK>>>');
    };

    _print('<<<ROBO_PREAMBLE_READY>>>');
})();
