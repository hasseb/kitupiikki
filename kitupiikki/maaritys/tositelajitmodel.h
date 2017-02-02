/*
   Copyright (C) 2017 Arto Hyvättinen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TOSITELAJITMODEL_H
#define TOSITELAJITMODEL_H

#include <QAbstractTableModel>

#include "db/kirjanpito.h"
#include "db/tositetyyppi.h"


namespace TositeLajiModelSisainen
{

    struct Tositelaji
    {
            int riviId = 0;
            QString tunnus;
            QString nimi;
            bool kaytossa = false;
            bool muokattu = false;
    };
}


/**
 * @brief Tositetyyppien muokkaamiseen käytettävä model
 */
class TositelajitModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Sarake
    {
        TUNNUS, NIMI
    };

    TositelajitModel();

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;

    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QVariant data(const QModelIndex &index, int role) const;

    Qt::ItemFlags flags(const QModelIndex &index) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);

public slots:
    void lataa();
    bool tallenna();
    void lisaaRivi();

protected:
    QList<TositeLajiModelSisainen::Tositelaji> lajit_;
};

#endif // TOSITELAJITMODEL_H
