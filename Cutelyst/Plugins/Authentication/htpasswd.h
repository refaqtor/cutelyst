/*
 * Copyright (C) 2014 Daniel Nicoletti <dantti12@gmail.com>
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

#ifndef AUTHENTICATION_STORE_HTPASSWD_H
#define AUTHENTICATION_STORE_HTPASSWD_H

#include <Cutelyst/cutelyst_global.h>
#include <Cutelyst/Plugins/Authentication/authenticationstore.h>

namespace Cutelyst {

class CUTELYST_PLUGIN_AUTHENTICATION_EXPORT StoreHtpasswd : public AuthenticationStore
{
    Q_OBJECT
public:
    explicit StoreHtpasswd(const QString &file, QObject *parent = nullptr);
    virtual ~StoreHtpasswd();

    void addUser(const ParamsMultiMap &user);

    virtual AuthenticationUser findUser(Context *c, const ParamsMultiMap &userInfo) final;

    virtual QVariant forSession(Context *c, const AuthenticationUser &user) final;

    virtual AuthenticationUser fromSession(Context *c, const QVariant &frozenUser) final;
};

}

#endif // AUTHENTICATION_STORE_HTPASSWD_H
