/* 
 *  Qactus - A Qt based OBS notifier
 *
 *  Copyright (C) 2010-2015 Javier Llorente <javier@opensuse.org>
 *  Copyright (C) 2010-2011 Sivan Greenberg <sivan@omniqueue.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "trayicon.h"
#include "obsxmlreader.h"
#include "configure.h"
#include "login.h"
#include "roweditor.h"

#include "obsaccess.h"
#include "obspackage.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    obsAccess = OBSaccess::getInstance();
    obsAccess->setApiUrl("https://api.opensuse.org");
    obsPackage = new OBSpackage();

    createToolbar();
    trayIcon = new TrayIcon(this);
    createActions();
    createTreePackages();
    createTreeRequests();
    createStatusBar();

    loginDialog = new Login(this);
    configureDialog = new Configure(this);
    ui->actionConfigure_Qactus->setEnabled(false);

    connect(obsAccess, SIGNAL(isAuthenticated(bool)), this, SLOT(enableButtons(bool)));

    readSettings();

    // Show login dialog on startup if user isn't logged in
    if(!obsAccess->isAuthenticated()) {
        // Centre login dialog
        loginDialog->move(this->geometry().center().x()-loginDialog->geometry().center().x(),
                          this->geometry().center().y()-loginDialog->geometry().center().y());
        loginDialog->show();
    }
}

MainWindow::~MainWindow()
{
    writeSettings();
    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void MainWindow::enableButtons(bool isAuthenticated)
{
    action_Refresh->setEnabled(isAuthenticated);
    action_Timer->setEnabled(isAuthenticated);
    ui->actionConfigure_Qactus->setEnabled(isAuthenticated);

    if (isAuthenticated) {
        qDebug() << "User is authenticated";
        readSettingsTimer();
        statusBar()->showMessage(tr("Online"), 0);
    } else {
        loginDialog->show();
    }
}

void MainWindow::createToolbar()
{
    action_Add = new QAction(tr("&Add"), this);
    action_Add->setIcon(QIcon(":/icons/list-add.png"));
    action_Add->setStatusTip(tr("Add a new row"));
    ui->toolBar->addAction(action_Add);
    connect(action_Add, SIGNAL(triggered()), this, SLOT(addRow()));

    action_Remove = new QAction(tr("&Remove"), this);
    action_Remove->setIcon(QIcon(":/icons/list-remove.png"));
    action_Remove->setStatusTip(tr("Remove selected row"));
    ui->toolBar->addAction(action_Remove);
    connect(action_Remove, SIGNAL(triggered()), this, SLOT(removeRow()));

    action_Refresh = new QAction(tr("&Refresh"), this);
    action_Refresh->setIcon(QIcon(":/icons/view-refresh.png"));
    action_Refresh->setStatusTip(tr("Refresh view"));
    action_Refresh->setEnabled(false);
    ui->toolBar->addAction(action_Refresh);
    connect(action_Refresh, SIGNAL(triggered()), this, SLOT(refreshView()));

    ui->toolBar->addSeparator();

    action_Timer = new QAction(tr("&Timer"), this);
    action_Timer->setIcon(QIcon(":/icons/chronometer.png"));
    action_Timer->setStatusTip(tr("Timer"));
    action_Timer->setEnabled(false);
    ui->toolBar->addAction(action_Timer);
    connect(action_Timer, SIGNAL(triggered()), this, SLOT(on_actionConfigure_Qactus_triggered()));

}

void MainWindow::addRow()
{
    qDebug() << "Launching RowEditor...";
    RowEditor *rowEditor = new RowEditor(this);

    if (rowEditor->exec()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treePackages);
        item->setText(0, rowEditor->getProject());
        item->setText(1, rowEditor->getPackage());
        item->setText(2, rowEditor->getRepository());
        item->setText(3, rowEditor->getArch());
        ui->treePackages->addTopLevelItem(item);
        int index = ui->treePackages->indexOfTopLevelItem(item);
        qDebug() << "Build" << item->text(1) << "added at" << index;
    }
    delete rowEditor;
}

void MainWindow::editRow(QTreeWidgetItem* item, int)
{
    qDebug() << "Launching RowEditor in edit mode...";
    RowEditor *rowEditor = new RowEditor(this);
    rowEditor->setProject(item->text(0));
    rowEditor->setPackage(item->text(1));
    rowEditor->setRepository(item->text(2));
    rowEditor->setArch(item->text(3));
    rowEditor->show();

    if (rowEditor->exec()) {
        int index = ui->treePackages->indexOfTopLevelItem(item);
        item->setText(0, rowEditor->getProject());
        item->setText(1, rowEditor->getPackage());
        item->setText(2, rowEditor->getRepository());
        item->setText(3, rowEditor->getArch());
        item->setText(4, "");
        ui->treePackages->insertTopLevelItem(index, item);
        qDebug() << "Build edited:" << index;
        qDebug() << "Status at" << index << item->text(4) << "(it should be empty)";
    }
    delete rowEditor;
}

void MainWindow::removeRow()
{
//    Check if the tree is not empty to avoid "deleting nothing"
    if (ui->treePackages->topLevelItemCount()!=0) {
//    Remove selected row
    QTreeWidgetItem *item = ui->treePackages->currentItem();
    int index = ui->treePackages->indexOfTopLevelItem(item);

//    Remove statusList for selected row
//    -1 means that there is no row selected
        if (index!=-1) {
            ui->treePackages->takeTopLevelItem(index);
            qDebug() << "Row removed:" << index;
        } else {
            qDebug () << "No row selected";
        }
    } else {
//        If the tree is empty, do nothing
    }    
}

void MainWindow::refreshView()
{
    qDebug() << "Refreshing view...";
    int rows = ui->treePackages->topLevelItemCount();

    for (int r=0; r<rows; r++) {
//        Ignore rows with empty cells and process rows with data
        if (ui->treePackages->topLevelItem(r)->text(0).isEmpty() ||
                ui->treePackages->topLevelItem(r)->text(1).isEmpty() ||
                ui->treePackages->topLevelItem(r)->text(2).isEmpty() ||
                ui->treePackages->topLevelItem(r)->text(3).isEmpty()) {
        } else {
            QStringList tableStringList;
            tableStringList.append(QString(ui->treePackages->topLevelItem(r)->text(0)));
            tableStringList.append(QString(ui->treePackages->topLevelItem(r)->text(2)));
            tableStringList.append(QString(ui->treePackages->topLevelItem(r)->text(3)));
            tableStringList.append(QString(ui->treePackages->topLevelItem(r)->text(1)));
//            Get build status
            statusBar()->showMessage(tr("Getting build statuses..."), 5000);
            obsPackage = obsAccess->getBuildStatus(tableStringList);
            insertBuildStatus(obsPackage, r);
        }
    }

    if (packageErrors.size()>1) {
        QMessageBox::critical(this,tr("Error"), packageErrors, QMessageBox::Ok );
        packageErrors.clear();
    }

//    Get SRs
    statusBar()->showMessage(tr("Getting requests..."), 5000);
    obsRequests = obsAccess->getRequests();
    insertRequests(obsRequests);
    statusBar()->showMessage(tr("Done"), 0);
}

void MainWindow::createTreePackages()
{
    ui->treePackages->setColumnCount(5);
    ui->treePackages->setColumnWidth(0, 150); // Project
    ui->treePackages->setColumnWidth(1, 150); // Package
    ui->treePackages->setColumnWidth(2, 115); // Repository
    ui->treePackages->setColumnWidth(3, 75); // Arch
    ui->treePackages->setColumnWidth(4, 140); // Status

    connect(ui->treePackages, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(editRow(QTreeWidgetItem*, int)));
}

void MainWindow::createTreeRequests()
{
    ui->treeRequests->setColumnCount(7);
    ui->treeRequests->setColumnWidth(0, 140); // Date
    ui->treeRequests->setColumnWidth(1, 60); // SR ID
    ui->treeRequests->setColumnWidth(2, 160); // Source project
    ui->treeRequests->setColumnWidth(3, 160); // Target project
    ui->treeRequests->setColumnWidth(4, 90); // Requester
    ui->treeRequests->setColumnWidth(5, 60); // Type
    ui->treeRequests->setColumnWidth(6, 60); // State

    connect(ui->treeRequests, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(getDescription(QTreeWidgetItem*, int)));
}

void MainWindow::insertBuildStatus(OBSpackage* obsPackage, const int& row)
{
    QString details = obsPackage->getDetails();
    QString status = obsPackage->getStatus();

//    If the line is too long (>250), break it
    details = breakLine(details, 250);
    if (details.size()>0) {
        qDebug() << "Details string size: " << details.size();
    }

    QTreeWidgetItem *item = ui->treePackages->topLevelItem(row);
    QString oldStatus = item->text(4);
    item->setText(4, status);
    QString newStatus = item->text(4);
    item->setToolTip(4, details);
    item->setForeground(4, getColorForStatus(status));

    qDebug() << "Build status" << status << "inserted in" << row
             << "(Total rows:" << ui->treePackages->topLevelItemCount() << ")";

//    If the old status is not empty and it is different from latest one,
//    change the tray icon
    if ((oldStatus != "") && (oldStatus != newStatus)) {
        qDebug() << "Build status has changed!";
        trayIcon->change();
        toggleItemFont(item);
    }
    qDebug() << "Old status:" << oldStatus << "New status:" << newStatus;
}

QString MainWindow::breakLine(QString& details, const int& maxSize)
{
    int i = maxSize;
    if (details.size()>i) {
        for (; i<details.size(); i++) {
            if (details[i]==QChar(',') || details[i]==QChar('-') || details[i]==QChar(' ')) {
                details.insert(++i,QString("<br>"));
                break;
            }
        }
    }
    return details;
}

QColor MainWindow::getColorForStatus(const QString& status)
{
//    Change the status' colour according to the status itself
    QColor color;
    color = Qt::black;

    if(status=="succeeded")
    {
        color = Qt::darkGreen;
    }
    else if(status=="blocked")
    {
        color = Qt::gray;
    }
    else if(status=="scheduled"||status=="building")
    {
        color = Qt::darkBlue;
    }
    else if(status=="failed")
    {
        color = Qt::red;
    }
    else if(status=="unresolvable")
    {
        color = Qt::darkRed;
    }
    else if(status.contains("unknown"))
    {
        color = Qt::red;
    }

    return color;
}

void MainWindow::toggleItemFont(QTreeWidgetItem *item)
{
    QFont font = item->font(0);
    font.setBold(!font.bold());
    for (int i=0; i<5; i++) {
        item->setFont(i, font);
    }
}

void MainWindow::insertRequests(QList<OBSrequest*> obsRequests)
{
//    If we already have inserted submit requests,
//    we remove them and insert the latest ones
    int rows = ui->treeRequests->topLevelItemCount();
    int requests = obsAccess->getRequestNumber();
    qDebug() << "InsertRequests() " << "Rows:" << rows << "Requests:" << requests;

    if (rows>0) {
        ui->treeRequests->clear();
    }

    qDebug() << "RequestNumber: " << obsAccess->getRequestNumber();
    qDebug() << "requests: " << requests;
    qDebug() << "obsRequests size: " << obsRequests.size();

    for (int i=0; i<obsRequests.size(); i++) {
        QTreeWidgetItem *item = new QTreeWidgetItem(ui->treeRequests);
        item->setText(0, obsRequests.at(i)->getDate());
        item->setText(1, obsRequests.at(i)->getId());
        item->setText(2, obsRequests.at(i)->getSource());
        item->setText(3, obsRequests.at(i)->getTarget());
        item->setText(4, obsRequests.at(i)->getRequester());
        item->setText(5, obsRequests.at(i)->getActionType());
        item->setText(6, obsRequests.at(i)->getState());

        ui->treeRequests->insertTopLevelItem(i, item);
    }
}

void MainWindow::getDescription(QTreeWidgetItem* item, int)
{
    qDebug() << "getDescription() " << "Row: " + QString::number(ui->treeRequests->indexOfTopLevelItem(item));
    qDebug() << "Description: " + obsRequests.at(ui->treeRequests->indexOfTopLevelItem(item))->getDescription();
    ui->textBrowser->setText(obsRequests.at(ui->treeRequests->indexOfTopLevelItem(item))->getDescription());
}

void MainWindow::pushButton_Login_clicked()
{
    obsAccess->setCredentials(loginDialog->getUsername(), loginDialog->getPassword());

//    Display a warning if the username/password is empty.
    if (loginDialog->getUsername().isEmpty() || loginDialog->getPassword().isEmpty()) {
        QMessageBox::warning(this,tr("Error"), tr("Empty username/password"), QMessageBox::Ok );
    } else {
        loginDialog->close();
        QProgressDialog progress(tr("Logging in..."), 0, 0, 0, this);
        progress.setWindowModality(Qt::WindowModal);
        progress.show();
        obsAccess->login();
    }
}

void MainWindow::lineEdit_Password_returnPressed()
{
    pushButton_Login_clicked();
}

void MainWindow::on_actionQuit_triggered(bool)
{
    qApp->quit();
}

void MainWindow::on_actionAbout_triggered(bool)
{
    about();
}

void MainWindow::about()
{
    QMessageBox::about(this,tr("About") + " " + QCoreApplication::applicationName(),
                       "<h2 align=\"left\">" + QCoreApplication::applicationName() + "</h2>" +
                       tr("A Qt-based OBS notifier") + "<br>" +
                       tr("Version:") + " " + QCoreApplication::applicationVersion() +
                       "<div align=\"left\">" +
                          "<p>" +
                          "<b>" + tr("Author:") + "</b><br>"
                          "Javier Llorente<br>"
                          "<a href='mailto:javier@opensuse.org'>javier@opensuse.org</a><br><br>"
                          "<b>" + tr("Contributors:") + "</b> <br>"
                          "Sivan Greenberg<br>"
                          "<a href='mailto:sivan@omniqueue.com'>sivan@omniqueue.com</a><br><br>"
                          "<b>" + tr("Artwork:") + "</b> <br>" +
                          tr("Icons by the Oxygen Team") + "<br>"
                          "<a href=\"http://www.oxygen-icons.org/\">http://www.oxygen-icons.org/</a><br><br>" +
                          tr("Tray icon by the Open Build Service") + "<br>"
                          "<a href=\"http://openbuildservice.org/\">http://openbuildservice.org/</a>"
                          "</p>"
                          "<p>" +
                          "<b>" + tr("License:") + "</b> <br>"
                          "<nobr>" + tr("This program is under the GPLv3") + "</nobr>"
                          "</p>"
                          "</div>"
                       );
}

void MainWindow::createActions()
{
    action_aboutQt = new QAction(tr("About &Qt"), this);
    connect(ui->action_aboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

//    action_Configure = new QAction(tr("&Configure"), trayIcon);
//    action_Configure->setIcon(QIcon(":/icons/configure.png"));
//    connect(action_Configure, SIGNAL(triggered()), this, SLOT(on_actionConfigure_Qactus_triggered()));
//    trayIcon->trayIconMenu->addAction(action_Configure);

    trayIcon->trayIconMenu->addSeparator();

    action_Restore = new QAction(tr("&Minimise"), trayIcon);
    connect(action_Restore, SIGNAL(triggered()), this, SLOT(toggleVisibility()));
    trayIcon->trayIconMenu->addAction(action_Restore);

    action_Quit = new QAction(tr("&Quit"), trayIcon);
    action_Quit->setIcon(QIcon(":/icons/application-exit.png"));
    connect(action_Quit, SIGNAL(triggered()), qApp, SLOT(quit()));
    trayIcon->trayIconMenu->addAction(action_Quit);
}

void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Offline"));
}

void MainWindow::toggleVisibility()
{
    if (this->isVisible()) {
        hide();
        action_Restore->setText(tr("Restore"));
    } else {
        showNormal();
        action_Restore->setText(tr("Minimise"));
    }
}

void MainWindow::trayIconClicked(QSystemTrayIcon::ActivationReason reason)
{
    if (reason==QSystemTrayIcon::Trigger) {
        toggleVisibility();
        if (trayIcon->hasChangedIcon()) {
            trayIcon->setTrayIcon("obs.png");
        }
    }

    qDebug() << "trayicon clicked";
}

void MainWindow::writeSettings()
{
    QSettings settings("Qactus","Qactus");
    settings.beginGroup("MainWindow");
    settings.setValue("pos", pos());
    settings.endGroup();

    settings.beginGroup("Auth");
    settings.setValue("Username", obsAccess->getUsername());
    settings.endGroup();

    settings.beginGroup("Timer");
    settings.setValue("Active", configureDialog->isTimerActive());
    settings.setValue("Value", configureDialog->getTimerValue());
    settings.endGroup();

    int rows = ui->treePackages->topLevelItemCount();
    settings.beginWriteArray("Packages");
    settings.remove("");
    for (int i=0; i<rows; ++i)
    {
        settings.setArrayIndex(i);
//        Save settings only if all the items in a row have text
        if (!ui->treePackages->topLevelItem(i)->text(0).isEmpty() &&
                !ui->treePackages->topLevelItem(i)->text(1).isEmpty() &&
                !ui->treePackages->topLevelItem(i)->text(2).isEmpty() &&
                !ui->treePackages->topLevelItem(i)->text(3).isEmpty())
        {
            settings.setValue("Project",ui->treePackages->topLevelItem(i)->text(0));
            settings.setValue("Package",ui->treePackages->topLevelItem(i)->text(1));
            settings.setValue("Repository",ui->treePackages->topLevelItem(i)->text(2));
            settings.setValue("Arch",ui->treePackages->topLevelItem(i)->text(3));
        }
    }
    settings.endArray();
}

void MainWindow::readSettings()
{
    QSettings settings("Qactus","Qactus");
    settings.beginGroup("MainWindow");
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    settings.endGroup();

    settings.beginGroup("auth");
    loginDialog->setUsername(settings.value("username").toString());
    settings.endGroup();   

    int size = settings.beginReadArray("Packages");
    for (int i=0; i<size; ++i)
        {
            settings.setArrayIndex(i);
            QTreeWidgetItem *item = new QTreeWidgetItem(ui->treePackages);
            item->setText(0, settings.value("Project").toString());
            item->setText(1, settings.value("Package").toString());
            item->setText(2, settings.value("Repository").toString());
            item->setText(3, settings.value("Arch").toString());
            ui->treePackages->insertTopLevelItem(i, item);
        }
        settings.endArray();
}

void MainWindow::readSettingsTimer()
{
    QSettings settings("Qactus","Qactus");
    settings.beginGroup("Timer");
    if (settings.value("Active").toBool()) {
        qDebug () << "Timer Active = true";
        configureDialog->setCheckedTimerCheckbox(true);
        configureDialog->startTimer(settings.value("value").toInt()*60000);
    } else {
        qDebug () << "Timer Active = false";
        configureDialog->setTimerValue(settings.value("value").toInt());
    }
    settings.endGroup();
}

void MainWindow::on_actionConfigure_Qactus_triggered()
{
    configureDialog->show();
}

void MainWindow::on_actionLogin_triggered()
{
    loginDialog->show();
}

void MainWindow::on_tabWidget_currentChanged(const int& index)
{
    // Disable add and remove for the request tab
    action_Add->setEnabled(!index);
    action_Remove->setEnabled(!index);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    toggleVisibility();
}

bool MainWindow::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::WindowActivate:
        qDebug() << "Window activated";
        if (trayIcon->hasChangedIcon()) {
            trayIcon->setTrayIcon("obs.png");
            for (int i=0; i<ui->treePackages->topLevelItemCount(); i++) {
                if (ui->treePackages->topLevelItem(i)->font(0).bold()) {
                    toggleItemFont(ui->treePackages->topLevelItem(i));
                }
            }
        }
        break;
    default:
        break;
    }
    return QMainWindow::event(event);
}
