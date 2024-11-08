/*
   Copyright (C) 2019 Arto Hyvättinen

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
#ifndef ERAPROXYMODEL_H
#define ERAPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QDate>


class EraProxyModel : public QSortFilterProxyModel
{
public:
    EraProxyModel(QObject *parent = nullptr);

    void suodata(const QDate& min, const QDate& max, const QString& teksti);

protected:
    virtual bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

    QDate minDate_;
    QDate maxDate_;
    QString teksti_;
};

#endif // ERAPROXYMODEL_H
