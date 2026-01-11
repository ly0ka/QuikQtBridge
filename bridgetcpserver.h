#ifndef BRIDGETCPSERVER_H
#define BRIDGETCPSERVER_H

#include "quikqtbridge.h"
#include "jsonprotocolhandler.h"

#include <QTcpServer>
#include <QMutex>

#define BRIDGE_SERVER_PROTOCOL_VERSION 1
#define FASTCALLBACK_TIMEOUT_SEC 5

class BridgeTCPServer;
class FastCallbackRequestEventLoop;

struct ConnectionData
{
    int outMsgId;
    QString peerIp;
    QList<int> objRefs;
    int peerProtocolVersion;
    QMap<QString, int> callbackSubscriptions;

    bool versionSent;
    BridgeTCPServer *srv;
    JsonProtocolHandler *proto;
    FastCallbackRequestEventLoop *fcbWaitResult;

    ConnectionData()
        : outMsgId(0), peerProtocolVersion(0), versionSent(false)
        , srv(nullptr), proto(nullptr), fcbWaitResult(nullptr) {}
    ~ConnectionData();
};
Q_DECLARE_METATYPE(ConnectionData*)

void sendStdoutLine(QString line);
void sendStderrLine(QString line);

class BridgeTCPServer : public QTcpServer, public QuikCallbackHandler
{
    Q_OBJECT
public:
    BridgeTCPServer(QObject *parent = nullptr);
    ~BridgeTCPServer();

    static BridgeTCPServer *getGlobalServer() {return g_server;}

    void setLogPathPrefix(QString lpp);
    void setDebugLogPathPrefix(QString lpp);
    void setAllowedIPs(const QStringList &aips);

    virtual void sendStdoutLine(QString line);
    virtual void sendStderrLine(QString line);
    virtual void clearFastCallbackData(void *data);
    Q_INVOKABLE virtual void callbackRequest(QString name, const QVariantList &args, QVariant &vres);
    virtual void fastCallbackRequest(void *data, const QVariantList &args, QVariant &res);
private:
    QFile *logf;
    QTextStream *logts;
    QString logPathPrefix;
    int logPathConnections;
    QStringList m_allowedIps;
    QStringList activeCallbacks;

    static BridgeTCPServer * g_server;
    QList<ConnectionData *> m_connections;

    bool ipAllowed(QString ip);
    ConnectionData *getCDByProtoPtr(JsonProtocolHandler *p);
    void sendError(ConnectionData *cd, int id, int errcode, QString errmsg, bool log=false);
protected:
    virtual void incomingConnection(qintptr handle);
private slots:
    void connectionEstablished(ConnectionData *cd);
    void protoReqArrived(int id, QJsonValue data);
    void protoAnsArrived(int id, QJsonValue data);
    void protoVerArrived(int ver);
    void protoEndArrived();
    void protoFinished();
    void protoError(QAbstractSocket::SocketError err);
    void serverError(QAbstractSocket::SocketError err);

    void fastCallbackRequestHandler(ConnectionData *cd, int oid, QString fname, QVariantList args);
signals:
    void fastCallbackRequestSent(ConnectionData *cd, QString fname, int id);
    void fastCallbackReturnArrived(ConnectionData *cd, int id, QVariant res);
};

class FastCallbackRequestEventLoop
{
public:
    FastCallbackRequestEventLoop(ConnectionData *rcd, int oid, QString rfname, BridgeTCPServer *s);
    QVariant sendAndWaitResult(BridgeTCPServer *server, const QVariantList &args);

    void fastCallbackRequestSent(ConnectionData *acd, int oid, QString afname, int aid);
    void fastCallbackReturnArrived(ConnectionData *acd, int aid, QVariant res);
    void connectionDataDeleted(ConnectionData *dcd);
private:
    ConnectionData *cd;
    QString funName;
    int objId;
    int id;
    QVariant result;
    QMutex *waitMux;
    BridgeTCPServer *srv;
};

#endif // BRIDGETCPSERVER_H
