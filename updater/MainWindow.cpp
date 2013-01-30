/***************************************************************************
 *   Copyright © 2011 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "MainWindow.h"

// Qt includes
#include <QtCore/QTimer>
#include <QtGui/QApplication>
#include <QtGui/QVBoxLayout>

// KDE includes
#include <KAction>
#include <KActionCollection>
#include <KDebug>
#include <KMessageBox>
#include <KMessageWidget>
#include <KProcess>
#include <KProtocolManager>
#include <KStandardDirs>
#include <Solid/Device>
#include <Solid/AcAdapter>

// Own includes
#include <MuonBackendsFactory.h>
#include <resources/AbstractResourcesBackend.h>
#include <resources/AbstractBackendUpdater.h>
#include "ChangelogWidget.h"
#include "ProgressWidget.h"
#include "config/UpdaterSettingsDialog.h"
#include "UpdaterWidget.h"

MainWindow::MainWindow()
    : MuonMainWindow()
    , m_settingsDialog(nullptr)
    , m_checkerProcess(nullptr)
{
    //FIXME: load all backends!
    MuonBackendsFactory f;
    m_apps = f.backend("muon-dummybackend");
    m_updater = m_apps->backendUpdater();
    connect(m_apps, SIGNAL(backendReady()), SLOT(initBackend()));
    connect(m_updater, SIGNAL(progressingChanged(bool)), SLOT(progressingChanged(bool)));
    connect(m_updater, SIGNAL(updatesFinnished()), SLOT(updatesFinished()));

    initGUI();
}

void MainWindow::initGUI()
{
    setWindowTitle(i18nc("@title:window", "Software Updates"));

    QWidget *mainWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(mainWidget);

    m_powerMessage = new KMessageWidget(mainWidget);
    m_powerMessage->setText(i18nc("@info Warning to plug in laptop before updating",
                                  "It is safer to plug in the power adapter before updating."));
    m_powerMessage->hide();
    m_powerMessage->setMessageType(KMessageWidget::Warning);
    checkPlugState();

    m_distUpgradeMessage = new KMessageWidget(mainWidget);
    m_distUpgradeMessage->hide();
    m_distUpgradeMessage->setMessageType(KMessageWidget::Information);
    m_distUpgradeMessage->setText(i18nc("Notification when a new version of Kubuntu is available",
                                        "A new version of Kubuntu is available."));

    m_progressWidget = new ProgressWidget(mainWidget);
    m_progressWidget->setTransaction(m_updater);
    m_progressWidget->hide();

    m_updaterWidget = new UpdaterWidget(mainWidget);
    m_updaterWidget->setEnabled(false);

    m_changelogWidget = new ChangelogWidget(this);
    m_changelogWidget->hide();
    connect(m_updaterWidget, SIGNAL(selectedResourceChanged(AbstractResource*)),
            m_changelogWidget, SLOT(setResource(AbstractResource*)));

    mainLayout->addWidget(m_powerMessage);
    mainLayout->addWidget(m_distUpgradeMessage);
    mainLayout->addWidget(m_progressWidget);
    mainLayout->addWidget(m_updaterWidget);
    mainLayout->addWidget(m_changelogWidget);

    m_apps->integrateMainWindow(this);
    setupActions();
    mainWidget->setLayout(mainLayout);
    setCentralWidget(mainWidget);

    checkDistUpgrade();
    connect(m_apps, SIGNAL(reloadStarted()), SLOT(startedReloading()));
    connect(m_apps, SIGNAL(reloadFinished()), SLOT(finishedReloading()));
}

void MainWindow::setupActions()
{
    MuonMainWindow::setupActions();

    KAction* updateAction = actionCollection()->addAction("update");
    updateAction->setIcon(KIcon("system-software-update"));
    updateAction->setText(i18nc("@action Checks the Internet for updates", "Check for Updates"));
    updateAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
    connect(updateAction, SIGNAL(triggered()), SLOT(checkForUpdates()));
    if (!isConnected()) {
        updateAction->setDisabled(true);
    }
    connect(this, SIGNAL(shouldConnect(bool)), updateAction, SLOT(setEnabled(bool)));

    m_applyAction = actionCollection()->addAction("apply");
    m_applyAction->setIcon(KIcon("dialog-ok-apply"));
    m_applyAction->setText(i18nc("@action Downloads and installs updates", "Install Updates"));
    connect(m_applyAction, SIGNAL(triggered()), this, SLOT(startCommit()));

    KStandardAction::preferences(this, SLOT(editSettings()), actionCollection());

    KAction *distUpgradeAction = new KAction(KIcon("system-software-update"), i18nc("@action", "Upgrade"), this);
    connect(distUpgradeAction, SIGNAL(activated()), this, SLOT(launchDistUpgrade()));

    m_distUpgradeMessage->addAction(distUpgradeAction);

    setActionsEnabled(false);

    setupGUI(StandardWindowOption(KXmlGuiWindow::Default & ~KXmlGuiWindow::StatusBar));
}

void MainWindow::initBackend()
{
    m_updaterWidget->setBackend(m_apps);

    setActionsEnabled();
}

void MainWindow::progressingChanged(bool active)
{
    QApplication::restoreOverrideCursor();
    m_progressWidget->setVisible(active);
    m_updaterWidget->setVisible(!active);
}

void MainWindow::updatesFinished()
{
    m_progressWidget->animatedHide();
    m_updaterWidget->setCurrentIndex(0);
    setActionsEnabled();
}

void MainWindow::startedReloading()
{
    setCanExit(false);
    setActionsEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_changelogWidget->setResource(0);
}

void MainWindow::finishedReloading()
{
    QApplication::restoreOverrideCursor();
    checkPlugState();
    setCanExit(true);
}

void MainWindow::setActionsEnabled(bool enabled)
{
    MuonMainWindow::setActionsEnabled(enabled);
    if (!enabled) {
        return;
    }

    m_applyAction->setEnabled(m_updater->hasUpdates());
    m_updaterWidget->setEnabled(true);
}

void MainWindow::checkForUpdates()
{
    m_updaterWidget->reload();
}

void MainWindow::startCommit()
{
    setActionsEnabled(false);
    m_updaterWidget->setEnabled(false);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    m_changelogWidget->animatedHide();

    m_updater->start();
}

void MainWindow::editSettings()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new UpdaterSettingsDialog(this);
        connect(m_settingsDialog, SIGNAL(okClicked()), SLOT(closeSettingsDialog()));
        m_settingsDialog->show();
    } else {
        m_settingsDialog->raise();
    }
}

void MainWindow::closeSettingsDialog()
{
    m_settingsDialog->deleteLater();
    m_settingsDialog = nullptr;
}

void MainWindow::checkPlugState()
{
    const QList<Solid::Device> acAdapters = Solid::Device::listFromType(Solid::DeviceInterface::AcAdapter);

    if (acAdapters.isEmpty()) {
        updatePlugState(true);
        return;
    }
    
    bool isPlugged = false;

    for(Solid::Device device_ac : acAdapters) {
        Solid::AcAdapter* acAdapter = device_ac.as<Solid::AcAdapter>();
        isPlugged |= acAdapter->isPlugged();
        connect(acAdapter, SIGNAL(plugStateChanged(bool,QString)),
                this, SLOT(updatePlugState(bool)), Qt::UniqueConnection);
    }

    updatePlugState(isPlugged);
}

void MainWindow::updatePlugState(bool plugged)
{
    m_powerMessage->setVisible(!plugged);
}

void MainWindow::checkDistUpgrade()
{
    QString checkerFile = KStandardDirs::locate("data", "muon-notifier/releasechecker");

    //TODO: Port to backends
    m_checkerProcess = new KProcess(this);
    m_checkerProcess->setProgram(QStringList() << "/usr/bin/python" << checkerFile);
    connect(m_checkerProcess, SIGNAL(finished(int)), this, SLOT(checkerFinished(int)));
    m_checkerProcess->start();
}

void MainWindow::checkerFinished(int res)
{
    if (res == 0) {
        m_distUpgradeMessage->show();
    }

    m_checkerProcess->deleteLater();
    m_checkerProcess = nullptr;
}

void MainWindow::launchDistUpgrade()
{
    KProcess::startDetached(QStringList() << "python"
                            << "/usr/share/pyshared/UpdateManager/DistUpgradeFetcherKDE.py");
}
