#include "docutaz/gui/widgets/workarea/OutputItemContentWidget.h"

#include <QVBoxLayout>
#include <Qsci/qscilexerjavascript.h>

#include <QFileInfo>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QDesktopServices>
#include <QUrl>

#include "docutaz/core/AppRegistry.h"
#include "docutaz/core/EventBus.h"
#include "docutaz/core/events/MongoEvents.h"
#include "docutaz/core/settings/SettingsManager.h"
#include "docutaz/core/settings/ConnectionSettings.h"
#include "docutaz/core/utils/QtUtils.h"
#include "docutaz/core/utils/BsonUtils.h"
#include "docutaz/core/domain/App.h"
#include "docutaz/core/domain/MongoShell.h"
#include "docutaz/core/domain/MongoServer.h"
#include "docutaz/core/mongodb/MongoWorker.h"
#include "docutaz/core/domain/MongoAggregateInfo.h"
#include "docutaz/gui/dialogs/CopyResultsDialog.h"
#include "docutaz/gui/dialogs/ExportResultsDialog.h"

#include "docutaz/gui/widgets/workarea/OutputWidget.h"
#include "docutaz/gui/widgets/workarea/OutputItemHeaderWidget.h"
#include "docutaz/gui/widgets/workarea/JsonPrepareThread.h"
#include "docutaz/gui/widgets/workarea/BsonTreeView.h"
#include "docutaz/gui/widgets/workarea/BsonTreeModel.h"
#include "docutaz/gui/widgets/workarea/BsonTableView.h"
#include "docutaz/gui/widgets/workarea/BsonTableModel.h"
#include "docutaz/gui/editors/PlainJavaScriptEditor.h"
#include "docutaz/gui/widgets/workarea/CollectionStatsTreeWidget.h"
#include "docutaz/gui/GuiRegistry.h"
#include "docutaz/gui/editors/JSLexer.h"
#include "docutaz/gui/editors/FindFrame.h"

namespace Docutaz
{
    OutputItemContentWidget::OutputItemContentWidget(ViewMode viewMode, MongoShell *shell, 
                                                     const QString &text, double secs, bool multipleResults, 
                                                     bool tabbedResults, bool firstItem, bool lastItem, 
                                                     AggrInfo aggrInfo, QWidget *parent) :
        BaseClass(parent),
        _textView(NULL),
        _bsonTreeview(NULL),
        _thread(NULL),
        _bsonTable(NULL),
        _isTextModeSupported(true),
        _isTreeModeSupported(false),
        _isTableModeSupported(false),
        _isCustomModeSupported(false),
        _isTextModeInitialized(false),
        _isTreeModeInitialized(false),
        _isCustomModeInitialized(false),
        _isTableModeInitialized(false),
        _isFirstPartRendered(false),
        _text(text),
        _shell(shell),
        _outputWidget(dynamic_cast<OutputWidget*>(parentWidget())),
        _initialSkip(0),
        _initialLimit(0),
        _mod(NULL),
        _viewMode(viewMode),
        _aggrInfo(aggrInfo)
    {
        setup(secs, multipleResults, tabbedResults, firstItem, lastItem);
    }

    OutputItemContentWidget::OutputItemContentWidget(ViewMode viewMode, MongoShell *shell, 
                                                     const QString &type, 
                                                     const std::vector<MongoDocumentPtr> &documents, 
                                                     const MongoQueryInfo &queryInfo, double secs, 
                                                     bool multipleResults, bool tabbedResults,
                                                     bool firstItem, bool lastItem, AggrInfo aggrInfo,
                                                     QWidget *parent) :
        BaseClass(parent),
        _textView(NULL),
        _bsonTreeview(NULL),
        _thread(NULL),
        _bsonTable(NULL),
        _isTextModeSupported(true),
        _isTreeModeSupported(true),
        _isTableModeSupported(true),
        // Custom UI is only meaningful for results that have a dedicated
        // renderer. Currently that is collection stats (CollectionStatsTreeWidget).
        // The mongosh layer stamps every result with a non-empty type
        // ("query"/"array"/"value"/…), so the old "!type.isEmpty()" test made the
        // button appear — and do nothing — on every result.
        _isCustomModeSupported(type == "collectionStats"),
        _isTextModeInitialized(false),
        _isTreeModeInitialized(false),
        _isCustomModeInitialized(false),
        _isTableModeInitialized(false),
        _isFirstPartRendered(false),
        _documents(documents),
        _queryInfo(queryInfo),
        _type(type),
        _shell(shell),
        _initialSkip(queryInfo._skip),
        _initialLimit(queryInfo._limit),
        _outputWidget(dynamic_cast<OutputWidget*>(parentWidget())),
        _mod(NULL),
        _viewMode(viewMode),
        _aggrInfo(aggrInfo)
    {
        setup(secs, multipleResults, tabbedResults, firstItem, lastItem);
    }

    void OutputItemContentWidget::setup(double secs, bool multipleResults, bool tabbedResults,
                                        bool firstItem, bool lastItem)
    {      
        setContentsMargins(0, 0, 0, 0);
        _header = new OutputItemHeaderWidget(this, multipleResults, tabbedResults, firstItem, lastItem);

        if (_queryInfo._info.isValid()) {
            _header->setCollection(QtUtils::toQString(_queryInfo._info._ns.collectionName()));
            _header->paging()->setBatchSize(_queryInfo._batchSize);
            _header->paging()->setSkip(_queryInfo._skip);
            if (!_queryInfo._limit)
                _queryInfo._limit = 50;
            // "Copy to…" is find-only: aggregations carry a valid queryInfo
            // namespace too, but their output is synthetic, so exclude them.
            if (!_aggrInfo.isValid) {
                _header->setCopyResultsEnabled(true);
                VERIFY(connect(_header, SIGNAL(copyResultsRequested()), this, SLOT(copyResultsTo())));
            }
        }
        else if (_aggrInfo.isValid) {
            _initialLimit = 0;
            _initialSkip = 0;
            _header->setCollection(QtUtils::toQString(_aggrInfo.collectionName));
            _header->paging()->setBatchSize(_aggrInfo.batchSize);
            _header->paging()->setSkip(_aggrInfo.skip);
        }

        // "Export…" works for both find and aggregation results (unlike copy):
        // the worker re-runs the find — respecting its projection — or the
        // aggregation pipeline, and writes the output to a file.
        if (_queryInfo._info.isValid() || _aggrInfo.isValid) {
            _header->setExportEnabled(true);
            VERIFY(connect(_header, SIGNAL(exportRequested()), this, SLOT(exportResults())));
        }

        _header->setTime(QString("%1 sec.").arg(secs, 0, 'g', 3));

        QVBoxLayout *layout = new QVBoxLayout();
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(_header);
        _stack = new QStackedWidget;
        layout->addWidget(_stack);
        setLayout(layout);
        configureModel();

        VERIFY(connect(_header->paging(), SIGNAL(refreshed(int, int)), this, SLOT(refresh(int, int))));
        VERIFY(connect(_header->paging(), SIGNAL(leftClicked(int, int)), this, SLOT(paging_leftClicked(int, int))));
        VERIFY(connect(_header->paging(), SIGNAL(rightClicked(int, int)), this, SLOT(paging_rightClicked(int, int))));
        VERIFY(connect(_header, SIGNAL(maximizedPart()), this, SIGNAL(maximizedPart())));
        VERIFY(connect(_header, SIGNAL(restoredSize()), this, SIGNAL(restoredSize())));

        refreshOutputItem();
    }

    namespace
    {
        // Emit a double-quoted JS string literal for an identifier (db/collection
        // name, URI) so names with quotes/backslashes can't break the script.
        std::string jsString(const std::string &s)
        {
            std::string out = "\"";
            for (char c : s) {
                switch (c) {
                    case '\\': out += "\\\\"; break;
                    case '"':  out += "\\\""; break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:   out += c;      break;
                }
            }
            out += "\"";
            return out;
        }
    }

    void OutputItemContentWidget::copyResultsTo()
    {
        if (!_queryInfo._info.isValid() || _aggrInfo.isValid || !_shell)
            return;

        MongoServer *server = _shell->server();
        ConnectionSettings *srcConn = server->connectionRecord();
        const std::string srcDb   = _queryInfo._info._ns.databaseName();
        const std::string srcColl = _queryInfo._info._ns.collectionName();

        CopyResultsDialog dlg(srcConn, QtUtils::toQString(srcDb), QtUtils::toQString(srcColl),
                              AppRegistry::instance().settingsManager()->connections(),
                              server->getDatabasesNames(), this);
        if (dlg.exec() != QDialog::Accepted)
            return;

        ConnectionSettings *target = dlg.targetConnection();
        const std::string targetDb   = QtUtils::toStdString(dlg.targetDatabase());
        const std::string targetColl = QtUtils::toStdString(dlg.targetCollection());
        const int  limit       = dlg.limit();     // 0 == no limit
        const bool dropFirst   = dlg.dropFirst();
        const bool copyIndexes = dlg.copyIndexes();
        const bool sameConn    = (target == srcConn) ||
            (!target->uuid().isEmpty() && target->uuid() == srcConn->uuid());

        // Serialize the query to shell-executable (TenGen) JSON so types — ObjectId,
        // ISODate, NumberLong, … — round-trip as shell constructors mongosh accepts.
        // The projection is intentionally NOT applied: it's a display setting, so a
        // copy materialises the full source documents, not just the viewed fields.
        const auto enc = AppRegistry::instance().settingsManager()->uuidEncoding();
        const auto tz  = AppRegistry::instance().settingsManager()->timeZone();
        const std::string filter = BsonUtils::jsonString(_queryInfo._query, mongo::TenGen, 0, enc, tz);
        const std::string sort   = _queryInfo._sort.isEmpty()
            ? "" : BsonUtils::jsonString(_queryInfo._sort, mongo::TenGen, 0, enc, tz);

        // Build the copy script. It runs inside the source connection's shell:
        // reads from the source collection, writes to the target. For a different
        // connection it opens a second connection via connect(); for the same one
        // it writes through the current connection (getSiblingDB).
        const std::string srcCollExpr = "db.getSiblingDB(" + jsString(srcDb) +
                                        ").getCollection(" + jsString(srcColl) + ")";
        // NOTE: top-level declarations use `var`, not const/let. Docutaz runs this
        // through mongosh's REPL rewriter, which splits the script into separate
        // statements; block-scoped const/let wouldn't be visible across them,
        // whereas `var` is function-scoped and persists (see mongosh-async-rewriter).
        std::string s;
        s += "// Docutaz — copy query results\n";
        std::string dstColl;
        if (sameConn) {
            dstColl = "db.getSiblingDB(" + jsString(targetDb) + ").getCollection(" +
                      jsString(targetColl) + ")";
        } else {
            const std::string uri = ConnectionSettings::buildMongoUri(target);
            s += "var __dst = connect(" + jsString(uri) + ");\n";
            dstColl = "__dst.getSiblingDB(" + jsString(targetDb) + ").getCollection(" +
                      jsString(targetColl) + ")";
        }
        s += "var __dstColl = " + dstColl + ";\n";
        if (dropFirst)
            s += "__dstColl.drop();\n";

        s += "var __cur = " + srcCollExpr + ".find(" + filter + ")";
        if (!sort.empty())
            s += ".sort(" + sort + ")";
        if (_queryInfo._skip > 0)
            s += ".skip(" + std::to_string(_queryInfo._skip) + ")";
        if (limit > 0)
            s += ".limit(" + std::to_string(limit) + ")";
        s += ";\n";

        // Insert unordered so a re-copy onto existing documents skips duplicates
        // instead of aborting at the first one; count what actually went in.
        s += "var __buf = [], __n = 0, __dups = 0;\n";
        s += "function __flush() {\n";
        s += "  if (!__buf.length) return;\n";
        s += "  try {\n";
        s += "    const __r = __dstColl.insertMany(__buf, { ordered: false });\n";
        s += "    __n += (__r.insertedCount !== undefined ? __r.insertedCount : __buf.length);\n";
        s += "  } catch (__e) {\n";
        s += "    const __ins = (__e.result && __e.result.insertedCount) || __e.insertedCount || 0;\n";
        s += "    __n += __ins; __dups += (__buf.length - __ins);\n";
        s += "  }\n";
        s += "  __buf = [];\n";
        s += "}\n";
        s += "__cur.forEach(function(__d) { __buf.push(__d); if (__buf.length === 500) __flush(); });\n";
        s += "__flush();\n";

        if (copyIndexes) {
            // Recreate the source indexes that aren't already on the target. Keep
            // the index name/options; drop fields createIndex doesn't accept.
            s += "var __have = new Set(__dstColl.getIndexes().map(function(ix) { return ix.name; }));\n";
            s += "var __idx = 0;\n";
            s += srcCollExpr + ".getIndexes().forEach(function(ix) {\n";
            s += "  if (ix.name === \"_id_\" || __have.has(ix.name)) return;\n";
            s += "  const __o = Object.assign({}, ix);\n";
            s += "  delete __o.key; delete __o.v; delete __o.ns;\n";
            s += "  __dstColl.createIndex(ix.key, __o); __idx++;\n";
            s += "});\n";
            s += "print(\"Created \" + __idx + \" index(es) on the target.\");\n";
        }

        s += "print(\"Copied \" + __n + \" document(s) to " + targetDb + "." + targetColl +
             "\" + (__dups ? (\" (\" + __dups + \" skipped: already present)\") : \"\"));\n";

        // Run in a new shell tab so the user sees exactly what executed (and its
        // output) without disturbing the current editor.
        AppRegistry::instance().app()->openShell(
            server, QtUtils::toQString(s), srcDb, true, "Copy results");
    }

    void OutputItemContentWidget::exportResults()
    {
        if (!_shell || !(_queryInfo._info.isValid() || _aggrInfo.isValid))
            return;
        if (_exportProgress)        // an export is already running for this result
            return;

        MongoServer *server = _shell->server();
        ConnectionSettings *conn = server->connectionRecord();
        const std::string db   = _queryInfo._info._ns.databaseName();
        std::string coll = _queryInfo._info._ns.collectionName();
        if (coll.empty())
            coll = _aggrInfo.collectionName;
        const QString label = QString("%1  —  %2.%3%4")
            .arg(QtUtils::toQString(conn->getReadableName()),
                 QtUtils::toQString(db), QtUtils::toQString(coll),
                 _aggrInfo.isValid ? "  (aggregation)" : "");

        ExportResultsDialog dlg(label, QtUtils::toQString(coll), this);
        if (dlg.exec() != QDialog::Accepted)
            return;

        ExportOptions opts;
        opts.format        = dlg.format();
        opts.jsonArray     = dlg.jsonArray();
        opts.flattenNested = dlg.flattenNested();
        opts.uuidEncoding  = AppRegistry::instance().settingsManager()->uuidEncoding();
        opts.timeZone      = AppRegistry::instance().settingsManager()->timeZone();

        // Busy dialog (application-modal, no cancel) so the result widget can't be
        // torn down while the worker streams to disk and replies back to it.
        _exportProgress = new QProgressDialog("Exporting documents…", QString(), 0, 0, this);
        _exportProgress->setWindowTitle("Export");
        _exportProgress->setWindowModality(Qt::ApplicationModal);
        _exportProgress->setCancelButton(nullptr);
        _exportProgress->setMinimumDuration(0);
        _exportProgress->show();

        AppRegistry::instance().bus()->send(server->worker(),
            new ExportRequest(this, _queryInfo, _aggrInfo, opts,
                              QtUtils::toStdString(dlg.filePath()), dlg.limit()));
    }

    void OutputItemContentWidget::handle(ExportResponse *event)
    {
        if (_exportProgress) {
            _exportProgress->close();
            _exportProgress->deleteLater();
            _exportProgress = nullptr;
        }

        if (event->isError()) {
            QMessageBox::warning(this, "Export failed",
                QtUtils::toQString(event->error().errorMessage()));
            return;
        }

        const QString path = QtUtils::toQString(event->filePath);
        QMessageBox box(QMessageBox::Information, "Export complete",
            QString("Exported %1 document(s) to:\n%2").arg(event->count).arg(path),
            QMessageBox::Ok, this);
        QPushButton *openBtn = box.addButton("Open Folder", QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == openBtn)
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    }

    void OutputItemContentWidget::paging_leftClicked(int skip, int limit)
    {
        int s = skip - limit;

        if (s < 0)
            s = 0;

        refresh(s, limit);
    }

    void OutputItemContentWidget::refreshOutputItem()
    {
        switch(_viewMode) {
            case Text: showText(); break;
            case Tree: showTree(); break;
            case Table: showTable(); break;
            case Custom: showCustom(); break;
            default: showTree();
        }
    }

    void OutputItemContentWidget::paging_rightClicked(int skip, int limit)
    {
        skip += limit;
        refresh(skip, limit);
    }

    void OutputItemContentWidget::refresh(int skip, int batchSize)
    {
        // Cannot set skip lower than in the text query
        if (skip <  _initialSkip) {
            _header->paging()->setSkip(_initialSkip);
            skip = _initialSkip;
        }

        int skipDelta = skip - _initialSkip;
        int limit = batchSize;

        // If limit is set to 0 it means UNLIMITED number of documents (limited only by batch size)
        // This is according to MongoDB documentation.
        if (_initialLimit != 0) {
            limit = _initialLimit - skipDelta;
            if (limit <= 0)
                limit = -1; // It means that we do not need to load documents

            if (limit > batchSize)
                limit = batchSize;
        }

        MongoQueryInfo info(_queryInfo);
        info._limit = limit;
        info._skip = skip;
        info._batchSize = batchSize;
        _outputWidget->showProgress();
                
        _shell->setScriptExecutable(true);
        if (_aggrInfo.isValid) {
            // Build original pipeline object, and append extra skip and limit for paging
            std::string pipelineModified = "[";
            for (int i = 0; ; i++) {
                auto const obj = mongo::tojson(_aggrInfo.pipeline.getObjectField(std::to_string(i)));
                if (obj.empty() || "{}" == obj)
                    break;

                pipelineModified.append(obj + ",");
            }
            pipelineModified.append("{$skip:" + std::to_string(skip) + "}, " +
                                    "{$limit:" + std::to_string(batchSize) + "}" + 
                                    "]");

            std::string const query = "db.getCollection('" + _aggrInfo.collectionName + "').aggregate(" +
                                      pipelineModified + ", " + _aggrInfo.options.toString() + ")";
            
            // Create aggr. info with new skip and batchsize
            AggrInfo const aggrInfo { _aggrInfo.collectionName, skip, batchSize, _aggrInfo.pipeline, 
                                      _aggrInfo.options, _outputWidget->resultIndex(this) };
            _shell->setAggrInfo(aggrInfo);
            _shell->execute(query);
        }
        else
            _shell->query(_outputWidget->resultIndex(this), info);
    }

    void OutputItemContentWidget::updateWithInfo(const MongoQueryInfo &inf, 
                                                 const std::vector<MongoDocumentPtr> &documents)
    {
        update(documents, inf._skip, inf._batchSize);
    }

    void OutputItemContentWidget::updateWithInfo(const AggrInfo &aggrInfo, 
                                                 const std::vector<MongoDocumentPtr> &documents)
    {
        update(documents, aggrInfo.skip, aggrInfo.batchSize);
    }

    void OutputItemContentWidget::update(const std::vector<MongoDocumentPtr> &documents, int skip, int batchSize)
    {
        _documents = documents;

        _header->paging()->setSkip(skip);
        _header->paging()->setBatchSize(batchSize);

        _text.clear();
        _isFirstPartRendered = false;
        markUninitialized();

        if (_bsonTable) {
            _stack->removeWidget(_bsonTable);
            delete _bsonTable;
            _bsonTable = NULL;
        }

        if (_bsonTreeview) {
            _stack->removeWidget(_bsonTreeview);
            delete _bsonTreeview;
            _bsonTreeview = NULL;
        }

        if (_textView) {
            _stack->removeWidget(_textView);
            delete _textView;
            _textView = NULL;
        }
        configureModel();
    }

    void OutputItemContentWidget::showText()
    {
        _viewMode = Text;
        _header->showText();
        if (!_isTextModeSupported)
            return;

        if (!_isTextModeInitialized)
        {
            _textView = configureLogText();
            if (!_text.isEmpty()) {
                _textView->sciScintilla()->setText(_text);
            }
            else {
                if (_documents.size() > 0) {
                    _textView->sciScintilla()->setText("Loading...");
                    _thread = new JsonPrepareThread(_documents, AppRegistry::instance().settingsManager()->uuidEncoding(), AppRegistry::instance().settingsManager()->timeZone());
                    VERIFY(connect(_thread, SIGNAL(partReady(const QString&)), this, SLOT(jsonPartReady(const QString&))));
                    VERIFY(connect(_thread, SIGNAL(finished()), _thread, SLOT(deleteLater())));
                    _thread->start();
                }
            }
            _stack->addWidget(_textView);
            _isTextModeInitialized = true;
        }

        _stack->setCurrentWidget(_textView);
    }

    void OutputItemContentWidget::showTree()
    {
        _viewMode = Tree;
        _header->showTree();
        if (!_isTreeModeSupported) {
            // try to downgrade to text mode
            showText();
            _viewMode = Tree;
            return;
        }

        if (!_isTreeModeInitialized) {
            _bsonTreeview = new BsonTreeView(_shell, _queryInfo, this);
            _bsonTreeview->setModel(_mod);
            _stack->addWidget(_bsonTreeview);

            if (true == AppRegistry::instance().settingsManager()->autoExpand())
                // Expanding only one level, because on large
                // documents it can take much time
                _bsonTreeview->expand(_mod->index(0, 0, QModelIndex()));

            _isTreeModeInitialized = true;
        }

        _stack->setCurrentWidget(_bsonTreeview);
    }

    void OutputItemContentWidget::showCustom()
    {
        _viewMode = Custom;
        _header->showCustom();

        if (!_isCustomModeSupported) {
            // try to downgrade to tree mode
            showTree();
            _viewMode = Custom;
            return;
        }

        if (!_isCustomModeInitialized) {

            if (_type == "collectionStats") {
                _collectionStats = new CollectionStatsTreeWidget(_documents, NULL);
                _stack->addWidget(_collectionStats);
            }               
            _isCustomModeInitialized = true;
        }

        if (_collectionStats)
            _stack->setCurrentWidget(_collectionStats);
    }

    void OutputItemContentWidget::showTable()
    {
        _viewMode = Table;
        _header->showTable();
        if (!_isTableModeSupported) {
            // try to downgrade to text mode
            showText();
            _viewMode = Table;
            return;
        }

        if (!_isTableModeInitialized) {
            _bsonTable = new BsonTableView(_shell, _queryInfo);
            BsonTableModelProxy *modp = new BsonTableModelProxy(_bsonTable);
            modp->setSourceModel(_mod);
            _bsonTable->setModel(modp);
            _stack->addWidget(_bsonTable);
            _isTableModeInitialized = true;
        }

        _stack->setCurrentWidget(_bsonTable);
    }

    void OutputItemContentWidget::markUninitialized()
    {
        _isTextModeInitialized = false;
        _isTreeModeInitialized = false;
        _isCustomModeInitialized = false;
        _isTableModeInitialized = false;
    }

    void OutputItemContentWidget::applyDockUndockSettings(bool isDocking) const
    {
        _header->applyDockUndockSettings(isDocking);
    }

    void OutputItemContentWidget::toggleOrientation(Qt::Orientation orientation) const
    {
        _header->toggleOrientation(orientation);
    }

    void OutputItemContentWidget::jsonPartReady(const QString &json)
    {
        // check that this is our current thread
        JsonPrepareThread *thread = qobject_cast<JsonPrepareThread *>(sender());
        if (thread && thread != _thread)
        {
            // close previous thread
            thread->stop();
            thread->wait();
        }
        else
        {
            if (_textView)
            {
                if (_isFirstPartRendered)
                    _textView->sciScintilla()->append(json);
                else
                    _textView->sciScintilla()->setText(json);
                _isFirstPartRendered = true;
            }
        }
    }
    
    BsonTreeModel *OutputItemContentWidget::configureModel()
    {
        delete _mod;
        _mod = new BsonTreeModel(_documents, this);
        return _mod;
    }

    FindFrame *Docutaz::OutputItemContentWidget::configureLogText()
    {
        const QFont &textFont = GuiRegistry::instance().font();

        QsciLexerJavaScript *javaScriptLexer = new JSLexer(this);
        javaScriptLexer->setFont(textFont);

        FindFrame *_logText = new FindFrame(this);
        _logText->sciScintilla()->setLexer(javaScriptLexer);
        _logText->sciScintilla()->setTabWidth(4);        
        _logText->sciScintilla()->setAppropriateBraceMatching();
        _logText->sciScintilla()->setFont(textFont);
        _logText->sciScintilla()->setReadOnly(true);
        _logText->sciScintilla()->setWrapMode((QsciScintilla::WrapMode) QsciScintilla::SC_WRAP_NONE);
        _logText->sciScintilla()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        _logText->sciScintilla()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        // Wrap mode turned off because it introduces huge performance problems
        // even for medium size documents.    
        _logText->sciScintilla()->setStyleSheet("QFrame {background-color: rgb(73, 76, 78); border: 1px solid #c7c5c4; border-radius: 0px; margin: 0px; padding: 0px;}");
        return _logText;
    }
}
