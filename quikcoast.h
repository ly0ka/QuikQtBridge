#ifndef QUIKCOAST_H
#define QUIKCOAST_H

#include <lua.hpp>

#include <QVariantList>

int luaopenImp(lua_State *l);
bool getQuikVariable(QString varname, QVariant &res);
bool invokeQuik(QString method, const QVariantList &args, QVariantList &res, QString &errMsg);
bool invokeQuikObject(int objid, QString method, const QVariantList &args, QVariantList &res, QString &errMsg);
void deleteQuikObject(int objid);
bool registerNamedCallback(QString cbName);
void unregisterAllNamedCallbacks();

#endif // QUIKCOAST_H
