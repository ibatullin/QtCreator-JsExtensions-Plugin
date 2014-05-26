#include "JepAPI.h"
#include <QMenu>
#include <QAction>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QToolButton>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QToolBar>
#include <QToolButton>

#include <QtQml>
#include <QQuickView>

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>

static int coreTypeId = qmlRegisterType<GCore>();
static int messageManagerTypeId = qmlRegisterType<GMessageManager>();
static int modeManagerTypeId = qmlRegisterType<GModeManager>();
static int commandTypeId = qmlRegisterType<GCommand>();
static int editorCommandTypeId = qmlRegisterType<Core::IEditor>();
static int documentCommandTypeId = qmlRegisterType<GDocument>();
static int actionContainerTypeId = qmlRegisterType<GActionContainer>();
static int actionManagerTypeId = qmlRegisterType<GActionManager>();
static int editorManagerTypeId = qmlRegisterType<GEditorManager>();

#define REG_O_FACTORY(ClassName) \
    m_factories.insert(#ClassName, [](QObject* parent)->QObject* { \
        return new ClassName(parent); \
    })

#define REG_W_FACTORY(ClassName) \
    m_factories.insert(#ClassName, [](QObject* parent)->QObject* { \
        return new ClassName(qobject_cast<QWidget*>(parent)); \
    })

JsPlugin::JsPlugin(QObject* parent)
    : QObject(parent),
      m_order(0),
      m_isDisabled(false),
      m_trace(false),
      m_debugIndent(0)
{
    // register creatable objects
    REG_O_FACTORY(QAction);

    REG_W_FACTORY(QWidget);
    REG_W_FACTORY(QPushButton);
    REG_W_FACTORY(QCheckBox);
    REG_W_FACTORY(QRadioButton);
    REG_W_FACTORY(QToolButton);
    REG_W_FACTORY(QLabel);
    REG_W_FACTORY(QLineEdit);
    REG_W_FACTORY(QMessageBox);
    REG_W_FACTORY(QToolBar);
    REG_W_FACTORY(QToolButton);
}

JsPlugin::~JsPlugin()
{
    // first of all delete JS engine
    m_jsEngine.reset();
}

class JepAPIException
{
public:
    JepAPIException(QString error)
        : error(error)
    {}

    QString error;
};

bool JsPlugin::loadPlugin(QString pluginPath)
{
    if (!m_jsEngine.isNull())
        return error("JepAPI is initialized already.");

    // save plugin path and name
    m_pluginPath = pluginPath;
    m_pluginName = QFileInfo(m_pluginPath).fileName();

    // open plugin file
    QFile scriptFile(pluginPath);
    if (!scriptFile.open(QIODevice::ReadOnly))
        return error("Cannot open file.");

    // read script
    QTextStream stream(&scriptFile);
    QString contents = stream.readAll();
    scriptFile.close();

    // prepare java script engine
    m_jsEngine.reset(new QJSEngine());
    installJsContext(m_jsEngine.data());

    // load script
    QJSValue res = m_jsEngine->evaluate(contents, pluginPath);
    if (res.isError())
        return error(QString("Script error: '%1'.").arg(res.toString()));

    // try to find "pluginDisable" variable
    res = m_jsEngine->evaluate("pluginDisable");
    if (res.isBool())
    {
        m_isDisabled = res.toBool();
    }

    // try to find "pluginOrder" variable
    res = m_jsEngine->evaluate("pluginOrder");
    if (res.isNumber())
    {
        m_order = res.toInt();
    }

    // try to find "pluginTrace" variable
    res = m_jsEngine->evaluate("pluginTrace");
    if (res.isBool())
    {
        m_trace = res.toBool();
    }

    return true;
}

void JsPlugin::installJsContext(QJSEngine* jsEngine)
{
    QJSValue globalObject = jsEngine->globalObject();

    GContext gContext;
    gContext.plugin = this;
    gContext.jsEngine = jsEngine;

    // setup objects to js context
    GCore* c(new GCore(gContext));
    globalObject.setProperty("core", jsEngine->newQObject(c));

    GMessageManager* msm(new GMessageManager(gContext));
    globalObject.setProperty("messageManager", jsEngine->newQObject(msm));

    GActionManager* am(new GActionManager(gContext));
    globalObject.setProperty("actionManager", jsEngine->newQObject(am));

    GEditorManager* em(new GEditorManager(gContext));
    globalObject.setProperty("editorManager", jsEngine->newQObject(em));

    GModeManager* mm(new GModeManager(gContext));
    globalObject.setProperty("modeManager", jsEngine->newQObject(mm));

    globalObject.setProperty("jepAPI", jsEngine->toScriptValue(static_cast<QObject*>(this)));
}

void JsPlugin::installQmlContext(QQmlEngine* qmlEngine)
{
    QQmlContext* context = qmlEngine->rootContext();

    GContext gContext;
    gContext.plugin = this;
    gContext.jsEngine = qmlEngine;

    // setup objects to qml context
    QObject* c(new GCore(gContext));
    context->setContextProperty("core", c);

    QObject* msm(new GMessageManager(gContext));
    context->setContextProperty("messageManager", msm);

    QObject* am(new GActionManager(gContext));
    context->setContextProperty("actionManager", am);

    QObject* em(new GEditorManager(gContext));
    context->setContextProperty("editorManager", em);

    QObject* mm(new GModeManager(gContext));
    context->setContextProperty("modeManager", mm);

    context->setContextProperty("jepAPI", static_cast<QObject*>(this));
}

void JsPlugin::changeDebugIndent(qint32 delta)
{
    m_debugIndent += delta;
    Q_ASSERT(m_debugIndent >= 0);
    if (m_debugIndent < 0)
        m_debugIndent = 0;
}

QPair<QWidget*, QQuickView*> JsPlugin::createQuickViewWidget(QString qmlUrl, QObject* parent)
{
    QPair<QWidget*, QQuickView*> result(nullptr, nullptr);

    QScopedPointer<QQmlEngine> qmlEngine(new QQmlEngine(this));
    installQmlContext(qmlEngine.data());

    QScopedPointer<QQuickView> view(new QQuickView(qmlEngine.data(), nullptr));

    // make absolute file path related to plugin dir
    QFileInfo fi(QFileInfo(m_pluginPath).absolutePath(), qmlUrl);
    if (fi.isFile())
    {
        qmlUrl = fi.absoluteFilePath();
        view->setSource(QUrl::fromLocalFile(qmlUrl));
    }
    else
    {
        view->setSource(QUrl(qmlUrl));
    }

    if (view->status() == QQuickView::Error)
    {
        foreach (const QQmlError& error, view->errors())
        {
            debug(error.toString());
        }

        return result;
    }

    QScopedPointer<QWidget> container(QWidget::createWindowContainer(view.data(), qobject_cast<QWidget*>(parent)));

    container->setFocusPolicy(Qt::TabFocus);

    QSize s = view->initialSize();
    container->setMinimumSize(s);

    qmlEngine.take();
    result.first = container.take();
    result.second =view.take();

    return result;
}

QString JsPlugin::errorString() const
{
    return m_errorString;
}

QJSValue JsPlugin::createQuickView(QString qmlUrl, QObject* parent)
{
    G_TRACE2(this);

    QPair<QWidget*, QQuickView*> result = createQuickViewWidget(qmlUrl, parent);
    QJSValue res = m_jsEngine->toScriptValue(static_cast<QObject*>(result.first));
    res.setProperty("quickView", m_jsEngine->toScriptValue(static_cast<QObject*>(result.second)));
    return res;
}

QJSValue JsPlugin::createQObject(QString type, QObject* parent)
{
    G_TRACE2(this);

    auto it = m_factories.find(type.toLatin1());
    if (it == m_factories.end())
        return QJSValue();

    QObject* object = it.value()(parent);

    return m_jsEngine->toScriptValue(object);
}

GNavigationWidgetFactory::GNavigationWidgetFactory(JsPlugin* owner, QJSValue factory, QString displayName, int priority, QString id, QString activationSequence)
    : m_owner(owner),
      m_factory(factory),
      m_displayName(displayName),
      m_priority(priority),
      m_id(id),
      m_activationSequence(activationSequence)
{
    Q_ASSERT(m_owner);
    setObjectName(QString("JEP_NavigationWidgetFactory_%1").arg(m_displayName));
}

Core::NavigationView GNavigationWidgetFactory::createWidget()
{
    G_TRACE2(m_owner);

    Core::NavigationView nv;
    nv.widget = nullptr;

    if (m_factory.isString())
    {
        // try load QML view
        QPair<QWidget*, QQuickView*> result = m_owner->createQuickViewWidget(m_factory.toString(), this);
        nv.widget = result.first;

        result.second->setResizeMode(QQuickView::SizeRootObjectToView);
    }
    else if (m_factory.isCallable())
    {
        // try call factory
        QJSValue res = m_factory.call();
        if (res.isQObject())
        {
            nv.widget = qobject_cast<QWidget*>(res.toQObject());
        }

        if (!nv.widget)
        {
            m_owner->debug("NavigationWidgetFactory: function doesn't return QWidget*.");
        }
    }
    else
    {
        m_owner->debug("NavigationWidgetFactory: factory is not string or function.");
    }

    return nv;
}

bool JsPlugin::registerNavigationWidgetFactory(QJSValue factory, QString displayName, int priority, QString id, QString activationSequence)
{
    ExtensionSystem::IPlugin* plugin = qobject_cast<ExtensionSystem::IPlugin*>(parent());
    if (!plugin)
        return false;

    plugin->addAutoReleasedObject(new GNavigationWidgetFactory(this, factory, displayName, priority, id, activationSequence));
    return true;
}

void JsPlugin::dumpPluginManagerObjects()
{
    QList<QObject*> objects = ExtensionSystem::PluginManager::allObjects();
    int i = 0;
    foreach (QObject* obj, objects)
    {
        debug(QString("%1: %2 (%3)").arg(i++).arg(obj->objectName()).arg(obj->metaObject()->className()));
    }
}

void JsPlugin::dumpCommands()
{
    QList<Core::Command*> commands = Core::ActionManager::commands();
    int i = 0;
    foreach (Core::Command* cmd, commands)
    {
        debug(QString("%1: <%2> (%3)").arg(i++).arg(cmd->id().toString()).arg(cmd->description()));
    }
}

bool JsPlugin::error(const QString &errorString)
{
    m_errorString = QString("%1\nScript file: '%2'").arg(errorString, m_pluginPath);
    return false;
}

bool JsPlugin::loadAPI(QString libFileName)
{
    G_TRACE2(this);

    QLibrary library(libFileName, this);
    if (!library.load())
    {
        debug(tr("Cannot load '%1'.").arg(libFileName));
        return false;
    }

    // library should export C function
    // extern "C" MY_EXPORT bool loadAPI(QJSEngine* jsEngine, QString* errors)
    typedef bool (*LoadAPIFunction)(QJSEngine*, QString*);

    LoadAPIFunction loadAPIFunc = (LoadAPIFunction)library.resolve("loadAPI");
    if (!loadAPIFunc)
    {
        debug(tr("Cannot resolve 'loadAPI' function."));
        library.unload();
        return false;
    }

    QString errors;
    if (!loadAPIFunc(m_jsEngine.data(), &errors))
    {
        debug(tr("'loadAPI' function failed (%1).").arg(errors));
        library.unload();
        return false;
    }

    return true;
}

bool JsPlugin::enableDebug()
{
    if (!m_debugStream.isNull())
        return true;

    if (m_jsEngine.isNull())
        return false;

    QFile* file(new QFile(this));
    file->setFileName(m_pluginPath + tr(".log"));
    if (!file->open(QIODevice::WriteOnly))
    {
        delete file;
        return false;
    }

    m_debugStream.reset(new QTextStream(file));
    return true;
}

void JsPlugin::debug(QString str)
{
    qDebug() << QString("%1 : %2").arg(m_pluginName, str);

    enableDebug();

    if (m_debugStream)
    {
        QString indent(m_debugIndent, QChar::Space);
        *m_debugStream << indent << str << endl;
    }
}

