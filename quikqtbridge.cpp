#include "quikqtbridge.h"
#include "quikcoast.h"
#include <QDebug>

QuikQtBridge *QuikQtBridge::global_bridge = nullptr;

QuikQtBridge *QuikQtBridge::initQuikQtBridge()
{
    if(!global_bridge)
        global_bridge = new QuikQtBridge();
    return global_bridge;
}

void QuikQtBridge::deinitQuikQtBridge()
{
    if(global_bridge)
    {
        QuikQtBridge *toDel = global_bridge;
        delete toDel;
    }
}


void QuikQtBridge::getVariable(QString varname, QVariant &res)
{
    getQuikVariable(varname, res);
}

void QuikQtBridge::setRecentStack(Qt::HANDLE ctid, lua_State *l)
{
    recentStackMap.insert(ctid, l);
}

lua_State *QuikQtBridge::getRecentStackForThreadId(Qt::HANDLE ctid)
{
    if(recentStackMap.contains(ctid))
        return recentStackMap.value(ctid);
    return nullptr;
}


bool QuikQtBridge::registerCallback(QuikCallbackHandler *handler, QString name)
{
    if(m_handlers.contains(name))
        return false;
    if(name!="OnStop")
    {
        if(!registerNamedCallback(name))
            return false;
    }
    m_handlers.insert(name, handler);
    return true;
}

void QuikQtBridge::callbackRequest(QString name, const QVariantList &args, QVariant &vres)
{
#ifdef QT_DEBUG
    qDebug() << "callbackRequest:" << name;
#endif
    if(m_handlers.contains(name))
    {
        m_handlers.value(name)->callbackRequest(name, args, vres);
    }
}


void QuikQtBridge::invokeMethod(QString method, const QVariantList &args, QVariantList &res, QuikCallbackHandler *errOut)
{
    QString errMsg;
    if(!invokeQuik(method, args, res, errMsg))
        errOut->sendStderrLine(errMsg);
}

void QuikQtBridge::invokeObjectMethod(int objid, QString method, const QVariantList &args, QVariantList &res, QuikCallbackHandler *errOut)
{
    QString errMsg;
    if(!invokeQuikObject(objid, method, args, res, errMsg))
        errOut->sendStderrLine(errMsg);
}

void QuikQtBridge::deleteObject(int objid)
{
    deleteQuikObject(objid);
}


QuikQtBridge::QuikQtBridge() : QObject() {}

QuikQtBridge::~QuikQtBridge()
{
    unregisterAllNamedCallbacks();
}
