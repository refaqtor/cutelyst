/*
 * Copyright (C) 2013 Daniel Nicoletti <dantti12@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "cutelystdispatcher_p.h"

#include "cutelystcontext.h"
#include "cutelystcontroller.h"
#include "cutelystaction.h"
#include "cutelystrequest_p.h"
#include "cutelystdispatchtypepath.h"
#include "cutelystdispatchtypeindex.h"
#include "cutelystdispatchtypedefault.h"

#include <QUrl>
#include <QMetaMethod>
#include <QStringBuilder>
#include <QRegularExpression>
#include <QDebug>

CutelystDispatcher::CutelystDispatcher(QObject *parent) :
    QObject(parent),
    d_ptr(new CutelystDispatcherPrivate)
{
    Q_D(CutelystDispatcher);
    d->dispatchers << new CutelystDispatchTypePath(this);
    d->dispatchers << new CutelystDispatchTypeIndex(this);
    d->dispatchers << new CutelystDispatchTypeDefault(this);
}

CutelystDispatcher::~CutelystDispatcher()
{
    qDebug() << Q_FUNC_INFO;
    delete d_ptr;
}

void CutelystDispatcher::setupActions()
{
    Q_D(CutelystDispatcher);

    // Find all the User classes
    int metaType = QMetaType::User;
    while (QMetaType::isRegistered(metaType)) {
        const QMetaObject *meta = QMetaType::metaObjectForType(metaType);
        if (qstrcmp(meta->superClass()->className(), "CutelystController") == 0) {
            // App controller
            CutelystController *controller = qobject_cast<CutelystController*>(meta->newInstance());
            qDebug() << "Found a controller:" << controller << meta->className();

            int i = 0;
            while (i < meta->methodCount()) {
                QMetaMethod method = meta->method(i);
                if (method.methodType() == QMetaMethod::Method) {
//                    qDebug() << Q_FUNC_INFO << method.name() << method.attributes() << method.methodType() << method.methodSignature();
//                    qDebug() << Q_FUNC_INFO << method.parameterTypes() << method.tag() << method.access();
                    CutelystAction *action = new CutelystAction(method, controller);
                    if (!d->actionHash.contains(action->privateName())) {
                        d->actionHash.insert(action->privateName(), action);
                        d->containerHash[action->ns()] << action;

                        if (!action->attributes().contains(QLatin1String("Private"))) {
                            bool registered = false;

                            // Register the action with each dispatcher
                            foreach (CutelystDispatchType *dispatch, d->dispatchers) {
                                if (dispatch->registerAction(action)) {
                                    registered = true;
                                }
                            }

                            if (!registered) {
                                qWarning() << "***Could NOT register the action" << action->name() << "with any dispatcher";
                            }
                        }
                    } else {
                        delete action;
                    }
                }
                ++i;
            }
        }

        if (meta->classInfoCount()) {
            QMetaClassInfo classInfo = meta->classInfo(0);
            qDebug() << Q_FUNC_INFO << classInfo.name() << classInfo.value();
        }
        ++metaType;
    }

    printActions();
}

bool CutelystDispatcher::dispatch(CutelystContext *c)
{
    if (c->action()) {
        return c->forward(QLatin1Char('/') % c->action()->ns() % QLatin1String("/_DISPATCH"));
    }
    return false;
}

bool CutelystDispatcher::forward(CutelystContext *c, const QString &opname, const QStringList &arguments)
{
    CutelystAction *action = command2Action(c, opname);
    if (action) {
        return action->dispatch(c);
    }

    qWarning() << Q_FUNC_INFO << "Action not found" << action;
    return false;
}

bool CutelystDispatcher::prepareAction(CutelystContext *c)
{
    Q_D(CutelystDispatcher);

    QString path = c->req()->path();
    QStringList pathParts = path.split(QLatin1Char('/'));
    QStringList args;
    CutelystDispatchType *dispatch = 0;

    while (!pathParts.isEmpty()) {
        path = pathParts.join(QLatin1Char('/'));
        if (path.startsWith(QLatin1Char('/'))) {
            path.remove(0, 1);
        }

        foreach (CutelystDispatchType *type, d->dispatchers) {
            if (type->match(c, path)) {
                qDebug() << Q_FUNC_INFO << "Found a dispatcher type" << type;
                dispatch = type;
                break;
            }
        }

        if (dispatch) {
            break;
        }

        args.prepend(pathParts.takeLast());
        c->req()->d_ptr->args = unexcapedArgs(args);

    }

    qDebug() << Q_FUNC_INFO << "Path is " << path;
    qDebug() << Q_FUNC_INFO << "Arguments are " << c->args().join(QLatin1Char('/'));

    return dispatch;
}

CutelystAction *CutelystDispatcher::getAction(const QString &name, const QString &ns)
{
    Q_D(CutelystDispatcher);
    if (name.isEmpty()) {
        return 0;
    }

    QString _ns = cleanNamespace(ns);

    QString action = _ns % QLatin1Char('/') % name;
    if (d->actionHash.contains(action)) {
        return d->actionHash.value(action);
    }

    return 0;
}

CutelystActionList CutelystDispatcher::getActions(const QString &name, const QString &ns)
{
    Q_D(CutelystDispatcher);

    CutelystActionList ret;
    if (name.isEmpty()) {
        return ret;
    }

    QString _ns = cleanNamespace(ns);

    CutelystActionList containers = d->getContainers(_ns);
    foreach (CutelystAction *action, containers) {
        if (action->name() == name) {
            ret << action;
        }
    }

    return ret;
}

void CutelystDispatcher::printActions()
{
    Q_D(CutelystDispatcher);

    bool showInternalActions = true;
    qDebug() << "Loaded Private actions:";
    QString privateTitle("Private");
    QString classTitle("Class");
    QString methodTitle("Method");
    int privateLength = privateTitle.length();
    int classLength = classTitle.length();
    int actionLength = methodTitle.length();
    QHash<QString, CutelystAction*>::ConstIterator it = d->actionHash.constBegin();
    while (it != d->actionHash.constEnd()) {
        CutelystAction *action = it.value();
        QString path = it.key();
        if (!path.startsWith(QLatin1String("/"))) {
            path.prepend(QLatin1String("/"));
        }
        privateLength = qMax(privateLength, path.length());
        classLength = qMax(classLength, action->className().length());
        actionLength = qMax(actionLength, action->name().length());
        ++it;
    }

    qDebug() << "." << QString().fill(QLatin1Char('-'), privateLength).toUtf8().data()
             << "+" << QString().fill(QLatin1Char('-'), classLength).toUtf8().data()
             << "+" << QString().fill(QLatin1Char('-'), actionLength).toUtf8().data()
             << ".";
    qDebug() << "|" << privateTitle.leftJustified(privateLength).toUtf8().data()
             << "|" << classTitle.leftJustified(classLength).toUtf8().data()
             << "|" << methodTitle.leftJustified(actionLength).toUtf8().data()
             << "|";
    qDebug() << "." << QString().fill(QLatin1Char('-'), privateLength).toUtf8().data()
             << "+" << QString().fill(QLatin1Char('-'), classLength).toUtf8().data()
             << "+" << QString().fill(QLatin1Char('-'), actionLength).toUtf8().data()
             << ".";

    it = d->actionHash.constBegin();
    while (it != d->actionHash.constEnd()) {
        CutelystAction *action = it.value();
        if (showInternalActions || !action->name().startsWith(QLatin1Char('_'))) {
            QString path = it.key();
            if (!path.startsWith(QLatin1String("/"))) {
                path.prepend(QLatin1String("/"));
            }
            qDebug() << "|" << path.leftJustified(privateLength).toUtf8().data()
                     << "|" << action->className().leftJustified(classLength).toUtf8().data()
                     << "|" << action->name().leftJustified(actionLength).toUtf8().data()
                     << "|";
        }
        ++it;
    }

    qDebug() << "." << QString().fill(QLatin1Char('-'), privateLength).toUtf8().data()
             << "+" << QString().fill(QLatin1Char('-'), classLength).toUtf8().data()
             << "+" << QString().fill(QLatin1Char('-'), actionLength).toUtf8().data()
             << ".\n";

    // List all public actions
    foreach (CutelystDispatchType *dispatch, d->dispatchers) {
        dispatch->list();
    }
}

CutelystAction *CutelystDispatcher::command2Action(CutelystContext *c, const QString &command, const QStringList &extraParams)
{
    Q_D(CutelystDispatcher);
    qDebug() << Q_FUNC_INFO << "Command" << command;

    CutelystAction *ret = 0;
    if (d->actionHash.contains(command)) {
        ret = d->actionHash.value(command);
    } else {
        ret = invokeAsPath(c, command, c->args());
    }

    return ret;
}

QStringList CutelystDispatcher::unexcapedArgs(const QStringList &args)
{
    QStringList ret;
    foreach (const QString &arg, args) {
        ret << QUrl::fromPercentEncoding(arg.toLocal8Bit());
    }
    return ret;
}

QString CutelystDispatcher::actionRel2Abs(CutelystContext *c, const QString &path)
{
    QString ret = path;
    if (!ret.startsWith(QLatin1Char('/'))) {
        // TODO at Catalyst it uses
        // c->stack->last()->namespace
        QString ns = c->action()->ns();
        ret = ns % QLatin1Char('/') % path;
    }

    if (ret.startsWith(QLatin1Char('/'))) {
        ret.remove(0, 1);
    }

    return ret;
}

CutelystAction *CutelystDispatcher::invokeAsPath(CutelystContext *c, const QString &relativePath, const QStringList &args)
{
    Q_D(CutelystDispatcher);

    CutelystAction *ret = 0;
    QString path = actionRel2Abs(c, relativePath);

    QRegularExpression re("^(?:(.*)/)?(\\w+)?$");
    while (!path.isEmpty()) {
        QRegularExpressionMatch match = re.match(path);
        if (match.hasMatch()) {
            path = match.captured(1);
            const QString &tail = match.captured(2);
            if (ret = getAction(tail, path)) {
                break;
            }
        } else {
            break;
        }
    }

    return ret;
}

QString CutelystDispatcher::cleanNamespace(const QString &ns) const
{
    QStringList ret;
    foreach (const QString &part, ns.split(QLatin1Char('/'))) {
        if (!part.isEmpty()) {
            ret << part;
        }
    }
    return ret.join(QLatin1Char('/'));
}


CutelystActionList CutelystDispatcherPrivate::getContainers(const QString &ns)
{
    CutelystActionList ret;

    QString _ns = ns;
    if (_ns == QLatin1String("/")) {
        _ns = QLatin1String("");
    }

    while (!_ns.isEmpty()) {
        ret << containerHash.value(_ns);
        _ns = _ns.section(QLatin1Char('/'), 0, -2);
    }
    ret << containerHash.value(_ns);

    return ret;
}