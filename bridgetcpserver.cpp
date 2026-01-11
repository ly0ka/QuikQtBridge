#include "bridgetcpserver.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QThread>

#define ALLOW_LOCAL_IP

BridgeTCPServer * BridgeTCPServer::g_server = nullptr;

struct FastCallbackFunctionData
{
    ConnectionData *cd;
    int objId;
    QString funName;
    FastCallbackFunctionData():cd(nullptr),objId(-1){}
    FastCallbackFunctionData(QString fn):cd(nullptr),objId(-1),funName(fn){}
};

BridgeTCPServer::BridgeTCPServer(QObject *parent)
    : QTcpServer(parent), logf(nullptr), logts(nullptr), logPathConnections(0)
{
    g_server = this;
    connect(this, SIGNAL(acceptError(QAbstractSocket::SocketError)), this, SLOT(serverError(QAbstractSocket::SocketError)));
    qqBridge->registerCallback(this, "OnStop");
    activeCallbacks.append("OnStop");
}

BridgeTCPServer::~BridgeTCPServer()
{
    while(!m_connections.isEmpty())
    {
        ConnectionData *cd = m_connections.takeLast();
        delete cd;
    }
}

void BridgeTCPServer::setLogPathPrefix(QString lpp)
{
    logPathPrefix = lpp;
}

void BridgeTCPServer::setDebugLogPathPrefix(QString lpp)
{
    if(!lpp.isEmpty())
    {
        QString logPath = lpp + ".log";
        QFile *tmpf=new QFile(logPath);
        if(tmpf->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            logf=tmpf;
            logts=new QTextStream(logf);
        }
        else
            delete tmpf;
    }
}

void BridgeTCPServer::setAllowedIPs(const QStringList &aips)
{
    m_allowedIps = aips;
}


void BridgeTCPServer::sendStdoutLine(QString line)
{
#ifdef QT_DEBUG
    qDebug() << line;
#endif
    if(logts)
    {
        *logts << Qt::endl << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") << " OUT: " << line;
        logts->flush();
    }
}

void BridgeTCPServer::sendStderrLine(QString line)
{
#ifdef QT_DEBUG
    qDebug() << line;
#endif
    if(logts)
    {
        *logts << Qt::endl << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz") << " ERR: " << line;
        logts->flush();
    }
}

void BridgeTCPServer::clearFastCallbackData(void *data)
{
    FastCallbackFunctionData *fcfdata = reinterpret_cast<FastCallbackFunctionData *>(data);
    if(fcfdata)
        delete fcfdata;
}

void BridgeTCPServer::callbackRequest(QString name, const QVariantList &args, QVariant &vres)
{
    if(!activeCallbacks.contains(name))
    {
        sendStderrLine(QString("Called callback %1 was not registered").arg(name));
        return;
    }
    QJsonObject cbCall
    {
        {"method", "callback"},
        {"name", name},
        {"arguments", QJsonArray::fromVariantList(args)}
    };
    ConnectionData *cd;
    foreach (cd, m_connections)
    {
        if(cd->callbackSubscriptions.contains(name))
        {
            int id = cd->callbackSubscriptions.value(name);
            cd->proto->sendReq(id, cbCall, false);
        }
    }
    if(name == "OnStop")
    {
        qApp->quit();
        vres = (int)100;
    }
}

void BridgeTCPServer::fastCallbackRequest(void *data, const QVariantList &args, QVariant &res)
{
    FastCallbackFunctionData *fcfdata = reinterpret_cast<FastCallbackFunctionData *>(data);
    if(fcfdata->cd)
    {
        if(m_connections.contains(fcfdata->cd))
        {
            FastCallbackRequestEventLoop el(fcfdata->cd, fcfdata->objId, fcfdata->funName, this);
            res = el.sendAndWaitResult(this, args);
        }
    }
}


bool BridgeTCPServer::ipAllowed(QString ip)
{
    sendStdoutLine(QString("Checking ip: ") + ip);
    foreach(QString mask, m_allowedIps)
    {
        QString rexpstr = QString("%1").arg(mask);
        sendStdoutLine(QString("rexp: ") + rexpstr);
        QRegularExpression rexp(rexpstr);
        if(rexp.match(ip).hasMatch())
        {
            sendStdoutLine(QString("matched"));
            return true;
        }
    }
    sendStdoutLine(QString("IP disabled"));
    return false;
}

ConnectionData *BridgeTCPServer::getCDByProtoPtr(JsonProtocolHandler *p)
{
    int i;
    ConnectionData *cd;
    for(i=0; i<m_connections.count(); i++)
    {
        cd = m_connections.at(i);
        if(cd->proto == p)
            return cd;
    }
    return nullptr;
}

void BridgeTCPServer::sendError(ConnectionData *cd, int id, int errcode, QString errmsg, bool log)
{
    QJsonObject errObj
    {
        {"method", "error"},
        {"code", errcode},
        {"text", errmsg}
    };
    if(log)
        sendStderrLine(errmsg);
    if(cd)
        cd->proto->sendAns(id, errObj, log);
}


void BridgeTCPServer::incomingConnection(qintptr handle)
{
    Qt::HANDLE thh = QThread::currentThreadId();
    QString hstr = QString("0x%1").arg((quintptr)thh, QT_POINTER_SIZE * 2, 16, QChar('0'));
    sendStdoutLine(QString("incomingConnection thread: %1").arg(hstr));
    QTcpSocket * sock = new QTcpSocket();
    sock->setSocketDescriptor(handle);
    sock->setSocketOption(QAbstractSocket::LowDelayOption, true);
    bool allowed = false;
    if(ipAllowed(sock->peerAddress().toString()))
        allowed = true;
    if(!allowed)
    {
        QString msg = QString("Connection from %1 refused").arg(sock->peerAddress().toString());
        sock->close();
        sock->deleteLater();
        sendStderrLine(msg);
        return;
    }
    QString logPath;
    if(!logPathPrefix.isEmpty())
    {
        ++logPathConnections;
        logPath = logPathPrefix+sock->peerAddress().toString()+'-'+QString::number(logPathConnections)+".log";
    }
    ConnectionData *cd = new ConnectionData();
    cd->srv = this;
    cd->peerIp = sock->peerAddress().toString();
    cd->proto = new JsonProtocolHandler(sock, logPath, this);
    connect(cd->proto, SIGNAL(reqArrived(int,QJsonValue)), this, SLOT(protoReqArrived(int,QJsonValue)));
    connect(cd->proto, SIGNAL(ansArrived(int,QJsonValue)), this, SLOT(protoAnsArrived(int,QJsonValue)));
    connect(cd->proto, SIGNAL(verArrived(int)), this, SLOT(protoVerArrived(int)));
    connect(cd->proto, SIGNAL(endArrived()), this, SLOT(protoEndArrived()));
    connect(cd->proto, SIGNAL(finished()), this, SLOT(protoFinished()));
    connect(cd->proto, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(protoError(QAbstractSocket::SocketError)));

    QString msg = QString("New connection from %1 established").arg(cd->peerIp);
    sendStdoutLine(msg);

    QMetaObject::invokeMethod(this, "connectionEstablished", Qt::QueuedConnection,
                              Q_ARG(ConnectionData*, cd));
}

void BridgeTCPServer::connectionEstablished(ConnectionData *cd)
{
    m_connections.append(cd);
    cd->proto->sendVer(BRIDGE_SERVER_PROTOCOL_VERSION);
    cd->versionSent = true;
}

void BridgeTCPServer::protoReqArrived(int id, QJsonValue data)
{
    Qt::HANDLE thh = QThread::currentThreadId();
    QString hstr = QString("0x%1").arg((quintptr)thh, QT_POINTER_SIZE * 2, 16, QChar('0'));
    sendStdoutLine(QString("protoReqArrived thread: %1").arg(hstr));
    ConnectionData *cd = getCDByProtoPtr(qobject_cast<JsonProtocolHandler *>(sender()));
    if(!cd)
        return;
    QJsonObject defaultObj
    {
        {"method", "nop"}
    };
    QJsonObject reqObj = data.toObject(defaultObj);
    if(!reqObj.contains("method"))
    {
        sendError(cd, id, 0, "Wrong format", true);
        return;
    }
    QString method = reqObj.value("method").toString("nop").toLower();
    if(method == "nop")
    {
        sendError(cd, id, 0, "Wrong request method", true);
        return;
    }
    if(method == "register")
    {
        if(!reqObj.contains("callback"))
        {
            sendError(cd, id, 1, "Method 'register' must have 'callback' argument", true);
            return;
        }
        QString callbackName = reqObj.value("callback").toString("__unknown__");
        if(callbackName == "__unknown__")
        {
            sendError(cd, id, 2, "Unknown callback name", true);
            return;
        }
        if(cd->callbackSubscriptions.contains(callbackName))
        {
            sendError(cd, id, 3, QString("Callback %1 already registered").arg(callbackName), true);
            return;
        }
        if(!activeCallbacks.contains(callbackName))
        {
            qqBridge->registerCallback(this, callbackName);
            activeCallbacks.append(callbackName);
        }
        cd->callbackSubscriptions.insert(callbackName, id);
        QJsonObject regRes
        {
            {"method", "registered"},
            {"callback", callbackName}
        };
        cd->proto->sendAns(id, regRes, false);
        return;
    }
    if(method == "invoke")
    {
        int objId=-1;
        if(reqObj.contains("object"))
            objId = reqObj.value("object").toInt(-1);
        QString funName = "__unknown__";
        if(reqObj.contains("function"))
                funName = reqObj.value("function").toString("__unknown__");
        if(funName == "__unknown__")
        {
            sendError(cd, id, 4, "Unknown function name", true);
            return;
        }
        QVariantList oargs, args;
        if(reqObj.contains("arguments"))
        {
            oargs = reqObj.value("arguments").toArray().toVariantList();
            int k;
            for(k=0; k<oargs.count(); k++)
            {
                QVariant carg = oargs[k];
                bool isCallable = false;
                if(carg.typeId() == QMetaType::QVariantMap)
                {
                    QVariantMap pcabl = carg.toMap();
                    if(pcabl.contains("type") && pcabl.value("type").toString()=="callable")
                    {
                        if(pcabl.contains("function"))
                        {
                            QString fname = pcabl.value("function").toString();
                            if(!fname.isEmpty())
                            {
                                isCallable = true;
                                BridgeCallableObject cobj;
                                FastCallbackFunctionData *fcfdata = new FastCallbackFunctionData();
                                fcfdata->cd = cd;
                                fcfdata->objId = objId;
                                fcfdata->funName = fname;
                                cobj.data = reinterpret_cast<void *>(fcfdata);
                                cobj.handler = this;
                                args.append(QVariant::fromValue(cobj));
                            }
                        }
                    }
                }
                if(!isCallable)
                {
                    args.append(carg);
                }
            }
        }
        QVariantList res;
        if(objId > 0)
            qqBridge->invokeObjectMethod(objId, funName, args, res, this);
        else
            qqBridge->invokeMethod(funName, args, res, this);
        int k;
        for(k=0;k<res.count();k++)
        {
            QVariant val = res[k];
            if(val.canConvert<QuikCallableObject>())
            {
                QuikCallableObject qco = val.value<QuikCallableObject>();
                res[k] = QVariant(qco.objid);
                cd->objRefs.append(qco.objid);
            }
        }
        QJsonObject invRes
        {
            {"method", "return"},
            {"result", QJsonArray::fromVariantList(res)}
        };
        cd->proto->sendAns(id, invRes, false);
        return;
    }
    if(method == "delete")
    {
        int objId=-1;
        if(reqObj.contains("object"))
            objId = reqObj.value("object").toInt(-1);
        if(objId > 0)
        {
            qqBridge->deleteObject(objId);
            QJsonObject delRes
            {
                {"method", "deleted"},
                {"object", objId}
            };
            cd->objRefs.removeAll(objId);
            cd->proto->sendAns(id, delRes, false);
            return;
        }
        else
        {
            sendError(cd, id, 5, QString("Object %1 is unknown").arg(objId), true);
            return;
        }
    }
}

void BridgeTCPServer::protoAnsArrived(int id, QJsonValue data)
{
    Qt::HANDLE thh = QThread::currentThreadId();
    QString hstr = QString("0x%1").arg((quintptr)thh, QT_POINTER_SIZE * 2, 16, QChar('0'));
    sendStdoutLine(QString("protoAnsArrived thread: %1").arg(hstr));
    ConnectionData *cd = getCDByProtoPtr(qobject_cast<JsonProtocolHandler *>(sender()));
    if(!cd)
        return;
    QJsonObject defaultObj
    {
        {"method", "nop"}
    };
    QJsonObject reqObj = data.toObject(defaultObj);
    if(!reqObj.contains("method"))
    {
        sendError(cd, id, 0, "Wrong format", true);
        return;
    }
    QString method = reqObj.value("method").toString("nop").toLower();
    if(method == "nop")
    {
        sendError(cd, id, 0, "Wrong request method", true);
        return;
    }
    if(method == "return")
    {
        QVariant res = reqObj.value("result").toVariant();
        if(cd->fcbWaitResult)
            cd->fcbWaitResult->fastCallbackReturnArrived(cd, id, res);
        return;
    }
}

void BridgeTCPServer::protoVerArrived(int ver)
{
    if(ver<1)
        return;
    ConnectionData *cd = getCDByProtoPtr(qobject_cast<JsonProtocolHandler *>(sender()));
    if(cd)
    {
        cd->peerProtocolVersion = ver;
        if(!cd->versionSent)
        {
            cd->proto->sendVer(BRIDGE_SERVER_PROTOCOL_VERSION);
            cd->versionSent = true;
        }
    }
}

void BridgeTCPServer::protoEndArrived()
{
    ConnectionData *cd = getCDByProtoPtr(qobject_cast<JsonProtocolHandler *>(sender()));
    if(cd)
    {
        QString msg = QString("Close connection request from %1").arg(cd->peerIp);
        sendStdoutLine(msg);
    }
}

void BridgeTCPServer::protoFinished()
{
    ConnectionData *cd = getCDByProtoPtr(qobject_cast<JsonProtocolHandler *>(sender()));
    if(cd)
    {
        QString msg = QString("Connection %1 closed").arg(cd->peerIp);
        sendStdoutLine(msg);
        m_connections.removeAll(cd);
        delete cd;
    }
}

void BridgeTCPServer::protoError(QAbstractSocket::SocketError err)
{
    ConnectionData *cd = getCDByProtoPtr(qobject_cast<JsonProtocolHandler *>(sender()));
    if(cd)
    {
        sendStderrLine(QString("Socket error %1: ").arg((int)err)+cd->proto->lastErrorString());
        m_connections.removeAll(cd);
        delete cd;
    }
}

void BridgeTCPServer::serverError(QAbstractSocket::SocketError err)
{
    sendStderrLine(QString("Server accepting error: ") + errorString());
}

void BridgeTCPServer::fastCallbackRequestHandler(ConnectionData *cd, int oid, QString fname, QVariantList args)
{
    if(m_connections.contains(cd))
    {
        QJsonObject invReq
        {
            {"method", "invoke"},
            {"function", fname},
            {"arguments", QJsonArray::fromVariantList(args)}
        };
        if(oid > 0)
            invReq["object"] = oid;
        int id = ++(cd->outMsgId);
        cd->fcbWaitResult->fastCallbackRequestSent(cd, oid, fname, id);
        cd->proto->sendReq(id, invReq, false);
    }
}


FastCallbackRequestEventLoop::FastCallbackRequestEventLoop(ConnectionData *rcd, int oid, QString rfname, BridgeTCPServer *s)
    : cd(rcd), funName(rfname), objId(oid), waitMux(nullptr), srv(s) {}

QVariant FastCallbackRequestEventLoop::sendAndWaitResult(BridgeTCPServer *server, const QVariantList &args)
{
    QMutex waitMutex;
    waitMux = &waitMutex;
    cd->fcbWaitResult = this;
    sendStdoutLine(QString("sendAndWaitResult: invoke fastCallbackRequestHandler..."));
    QMetaObject::invokeMethod(server, "fastCallbackRequestHandler", Qt::QueuedConnection,
                              Q_ARG(ConnectionData*, cd),
                              Q_ARG(int, objId),
                              Q_ARG(QString, funName),
                              Q_ARG(QVariantList, args));
    sendStdoutLine(QString("sendAndWaitResult: wait result..."));
    waitMutex.lock();
    waitMutex.tryLock(FASTCALLBACK_TIMEOUT_SEC * 1000);
    sendStdoutLine(QString("sendAndWaitResult: Event loop finished"));
    waitMux = nullptr;
    waitMutex.unlock();
    if(cd)
        cd->fcbWaitResult = nullptr;
    return result;
}

void FastCallbackRequestEventLoop::fastCallbackRequestSent(ConnectionData *acd, int oid, QString afname, int aid)
{
    if(cd==acd && funName==afname && objId==oid)
    {
        sendStdoutLine(QString("Fast callback request sent"));
        id = aid;
    }
}

void FastCallbackRequestEventLoop::fastCallbackReturnArrived(ConnectionData *acd, int aid, QVariant res)
{
    if(cd==acd && id==aid)
    {
        result = res;
        sendStdoutLine(QString("Wake fast callback waiter"));
        if(waitMux)
            waitMux->unlock();
        else
        {
            sendStderrLine(QString("Fast callback return arrived without locking?"));
        }
    }
}

void FastCallbackRequestEventLoop::connectionDataDeleted(ConnectionData *dcd)
{
    if(cd == dcd)
    {
        cd = nullptr;
        if(waitMux)
        {
            waitMux->unlock();
            sendStdoutLine(QString("Wake fast callback waiter(connectionDataDeleted)"));
        }
        else
        {
            sendStderrLine(QString("Connection data deleted unexpected call"));
        }
    }
}


ConnectionData::~ConnectionData()
{
    while(!objRefs.isEmpty())
    {
        int objid = objRefs.takeLast();
        qqBridge->deleteObject(objid);
        sendStdoutLine(QString("Object %1 deleted").arg(objid));
    }
    if(fcbWaitResult)
        fcbWaitResult->connectionDataDeleted(this);
    if(proto)
        delete proto;
}

void sendStdoutLine(QString line)
{
    BridgeTCPServer *gsrv = BridgeTCPServer::getGlobalServer();
    if(gsrv)
        gsrv->sendStdoutLine(line);
}

void sendStderrLine(QString line)
{
    BridgeTCPServer *gsrv = BridgeTCPServer::getGlobalServer();
    if(gsrv)
        gsrv->sendStderrLine(line);
}
