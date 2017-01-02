/*
 *   Copyright (C) 2012 Aleix Pol Gonzalez <aleixpol@blue-systems.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library/Lesser General Public License
 *   version 2, or (at your option) any later version, as published by the
 *   Free Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library/Lesser General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <resources/StandardBackendUpdater.h>
#include <resources/AbstractResourcesBackend.h>
#include <resources/AbstractResource.h>
#include "ResourcesModel.h"
#include <Transaction/Transaction.h>
#include <Transaction/TransactionModel.h>
#include <KLocalizedString>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QIcon>

StandardBackendUpdater::StandardBackendUpdater(AbstractResourcesBackend* parent)
    : AbstractBackendUpdater(parent)
    , m_backend(parent)
    , m_settingUp(false)
    , m_progress(0)
    , m_lastUpdate(QDateTime())
{
    connect(m_backend, &AbstractResourcesBackend::fetchingChanged, this, &StandardBackendUpdater::refreshUpdateable);
    connect(m_backend, &AbstractResourcesBackend::resourcesChanged, this, &StandardBackendUpdater::resourcesChanged);
    connect(TransactionModel::global(), &TransactionModel::transactionRemoved, this, &StandardBackendUpdater::transactionRemoved);
    connect(TransactionModel::global(), &TransactionModel::transactionAdded, this, &StandardBackendUpdater::transactionAdded);
}

void StandardBackendUpdater::resourcesChanged(AbstractResource* /*res*/, const QVector<QByteArray>& props)
{
    if (props.contains("state"))
        refreshUpdateable();
}

bool StandardBackendUpdater::hasUpdates() const
{
    return !m_upgradeable.isEmpty();
}

void StandardBackendUpdater::start()
{
    m_settingUp = true;
    emit progressingChanged(true);
    setProgress(-1);
    Q_EMIT progressingChanged(true);

    foreach(AbstractResource* res, m_toUpgrade) {
        m_pendingResources += res;
        m_backend->installApplication(res);
    }
    m_settingUp = false;
    emit statusMessageChanged(statusMessage());

    if(m_pendingResources.isEmpty()) {
        cleanup();
    } else {
        setProgress(1);
    }
}

void StandardBackendUpdater::transactionAdded(Transaction* newTransaction)
{
    if (!m_pendingResources.contains(newTransaction->resource()))
        return;

    connect(newTransaction, &Transaction::progressChanged, this, &StandardBackendUpdater::transactionProgressChanged);
}

void StandardBackendUpdater::transactionProgressChanged(int percentage)
{
    Transaction* t = qobject_cast<Transaction*>(sender());
    Q_EMIT resourceProgressed(t->resource(), percentage);
}

void StandardBackendUpdater::transactionRemoved(Transaction* t)
{
    const bool fromOurBackend = t->resource() && t->resource()->backend()==m_backend;
    if (!fromOurBackend) {
        return;
    }

    const bool found = fromOurBackend && m_pendingResources.remove(t->resource());

    if(found && !m_settingUp) {
        setStatusDetail(i18n("%1 has been updated", t->resource()->name()));
        qreal p = 1-(qreal(m_pendingResources.size())/m_toUpgrade.size());
        setProgress(100*p);
        if(m_pendingResources.isEmpty()) {
            cleanup();
        }
    }
    refreshUpdateable();
}

void StandardBackendUpdater::refreshUpdateable()
{
    if (m_backend->isFetching()) {
        return;
    }

    m_settingUp = true;
    Q_EMIT progressingChanged(true);
    AbstractResourcesBackend::Filters f;
    f.state = AbstractResource::Upgradeable;
    m_upgradeable.clear();
    auto r = m_backend->search(f);
    connect(r, &ResultsStream::resourcesFound, this, [this](const QVector<AbstractResource*> &resources){
        for(auto res : resources)
            if (res->state() == AbstractResource::Upgradeable)
                m_upgradeable.insert(res);
    });
    connect(r, &ResultsStream::destroyed, this, [this](){
        m_settingUp = false;
        Q_EMIT progressingChanged(false);
        Q_EMIT updatesCountChanged(updatesCount());
    });
}

qreal StandardBackendUpdater::progress() const
{
    return m_progress;
}

void StandardBackendUpdater::setProgress(qreal p)
{
    if(p>m_progress || p<0) {
        m_progress = p;
        emit progressChanged(p);
    }
}

long unsigned int StandardBackendUpdater::remainingTime() const
{
    return 0;
}

void StandardBackendUpdater::prepare()
{
    m_lastUpdate = QDateTime::currentDateTime();
    m_toUpgrade = m_upgradeable;
}

int StandardBackendUpdater::updatesCount() const
{
    return m_upgradeable.count();
}

void StandardBackendUpdater::addResources(const QList< AbstractResource* >& apps)
{
    Q_ASSERT(m_upgradeable.contains(apps.toSet()));
    m_toUpgrade += apps.toSet();
}

void StandardBackendUpdater::removeResources(const QList< AbstractResource* >& apps)
{
    Q_ASSERT(m_upgradeable.contains(apps.toSet()));
    Q_ASSERT(m_toUpgrade.contains(apps.toSet()));
    m_toUpgrade -= apps.toSet();
}

void StandardBackendUpdater::cleanup()
{
    m_lastUpdate = QDateTime::currentDateTime();
    m_toUpgrade.clear();
    emit progressingChanged(false);
}

QList<AbstractResource*> StandardBackendUpdater::toUpdate() const
{
    return m_toUpgrade.toList();
}

bool StandardBackendUpdater::isMarked(AbstractResource* res) const
{
    return m_toUpgrade.contains(res);
}

QDateTime StandardBackendUpdater::lastUpdate() const
{
    return m_lastUpdate;
}

bool StandardBackendUpdater::isCancelable() const
{
    //We don't really know when we can cancel, so we never let
    return false;
}

bool StandardBackendUpdater::isProgressing() const
{
    return m_settingUp || !m_pendingResources.isEmpty();
}

QString StandardBackendUpdater::statusDetail() const
{
    return m_statusDetail;
}

void StandardBackendUpdater::setStatusDetail(const QString& msg)
{
    if (m_statusDetail != msg) {
        m_statusDetail = msg;
        emit statusDetailChanged(msg);
    }
}

QString StandardBackendUpdater::statusMessage() const
{
    if(m_settingUp)
        return i18n("Setting up for install...");
    else
        return i18n("Installing...");
}

quint64 StandardBackendUpdater::downloadSpeed() const
{
    return 0;
}
