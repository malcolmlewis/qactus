/*
 *  Qactus - A Qt based OBS notifier
 *
 *  Copyright (C) 2015 Javier Llorente <javier@opensuse.org>
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

#ifndef ROWEDITOR_H
#define ROWEDITOR_H

#include <QDialog>
#include <QCompleter>
#include <QStringListModel>
#include <QDate>
#include <QSettings>
#include <QProgressDialog>
#include "obsaccess.h"
#include "obsxmlreader.h"

namespace Ui {
class RowEditor;
}

class RowEditor : public QDialog
{
    Q_OBJECT

public:
    explicit RowEditor(QWidget *parent = 0);
    ~RowEditor();
    QString getProject();
    QString getPackage();
    QString getRepository();
    QString getArch();
    void setProject(const QString &);
    void setPackage(const QString &);
    void setRepository(const QString &);
    void setArch(const QString &);

private:
    Ui::RowEditor *ui;
    QString getLastUpdateDate();
    void setLastUpdateDate(const QString &date);
    QStringList getListFor(const QString &name);
    QStringList projectList;
    QCompleter *projectCompleter;
    void initProjectAutocompleter();
    QStringList packageList;
    QCompleter *packageCompleter;
    QStringList repositoryList;
    QCompleter *repositoryCompleter;
    QStringList archList;
    QCompleter *archCompleter;
    OBSxmlReader *xmlReader;

private slots:
    void refreshProjectAutocompleter(const QString &);
    void autocompletedProjectName_clicked(const QString &projectName);
    void refreshPackageAutocompleter(const QString&);
    void autocompletedPackageName_clicked(const QString&);
    void refreshRepositoryAutocompleter(const QString&);
    void autocompletedRepositoryName_clicked(const QString&repository);
    void refreshArchAutocompleter(const QString&);
};

#endif // ROWEDITOR_H
