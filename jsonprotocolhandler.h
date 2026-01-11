#ifndef JSONPROTOCOLHANDLER_H
#define JSONPROTOCOLHANDLER_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <QTcpSocket>
#include <QTextStream>
#include <QFile>

class JsonProtocolHandler : public QObject
{
    Q_OBJECT
public:
    JsonProtocolHandler(QTcpSocket * sock, QString logFileName=QString(), QObject *parent=0);
    ~JsonProtocolHandler();

    void safeAbort();
    void forceDisconnect();
    QString lastErrorString()
    {
        if(socket)
            return socket->errorString();
        return "There is no socket";
    }

    quint16 peerPort()
    {
        if(socket)
            return socket->peerPort();;
        return 0;
    }
    QString peerAddressPort()
    {
        if(socket)
            return QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
        return QString();
    }
    QHostAddress peerAddress()
    {
        if(socket)
            return socket->peerAddress();
        return QHostAddress();
    }

    int getSocketDescriptor()
    {
        if(socket)
            return socket->socketDescriptor();
        return 0;
    }
    QTcpSocket * getTcpSocket()
    {
        return socket;
    }
    QAbstractSocket::SocketState getTcpSocketState()
    {
        return socket ? socket->state() : QAbstractSocket::UnconnectedState;
    }
public slots:
    void sendReq(int id, QJsonValue data, bool showInLog=true);
    void sendAns(int id, QJsonValue data, bool showInLog=true);
    void sendVer(int ver);
    void end(bool force=false);
private:
    QFile *logf;
    QTextStream *logts;
    QTcpSocket * socket;

    bool weEnded;
    bool peerEnded;
    QByteArray incommingBuf;

    bool socketValid();
    void processBuffer();
    void logIncoming(const QByteArray &msg);
    void logOutgoing(const QByteArray &msg);
signals:
    void reqArrived(int id, QJsonValue data);
    void ansArrived(int id, QJsonValue data);
    void verArrived(int ver);
    void endArrived();
    void finished();

    void parseError(QByteArray trash);
    void error(QAbstractSocket::SocketError err);
private slots:
    void readyRead();
    void disconnected();
    void errorThunk(QAbstractSocket::SocketError err);
};

#endif // JSONPROTOCOLHANDLER_H
