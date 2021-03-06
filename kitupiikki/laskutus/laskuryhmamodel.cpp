/*
   Copyright (C) 2018 Arto Hyvättinen

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

#include "laskuryhmamodel.h"
#include "db/kirjanpito.h"

#include "ryhmantuontidlg.h"
#include "ryhmantuontimodel.h"

#include "tuonti/csvtuonti.h"
#include <QMimeData>
#include <QDebug>

LaskuRyhmaModel::LaskuRyhmaModel(QObject *parent)
    : QAbstractTableModel (parent)
{
    pohjaviite_ = kp()->asetukset()->isoluku("LaskuSeuraavaId") / 10;
}

int LaskuRyhmaModel::rowCount(const QModelIndex & /* parent */) const
{
    return ryhma_.count();
}

int LaskuRyhmaModel::columnCount(const QModelIndex & /* parent */) const
{
    return 3;
}

QVariant LaskuRyhmaModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if( role == Qt::DisplayRole && orientation == Qt::Horizontal)
    {
        switch (section)
        {
            case VIITE:
                return tr("Viite");
            case NIMI:
                return tr("Nimi");
            case SAHKOPOSTI:
                return tr("Sähköposti");
        }
    }
    return {};
}

QVariant LaskuRyhmaModel::data(const QModelIndex &index, int role) const
{
    if( !index.isValid())
        return {};

    if( role == Qt::DisplayRole)
    {
        if( index.column() == VIITE)
        {
            qulonglong pohjanumero =   pohjaviite_ + static_cast<qulonglong>( index.row() );
            return pohjanumero * 10 + LaskuModel::laskeViiteTarkiste(pohjanumero);
        }

        Laskutettava laskutettava = ryhma_.at(index.row());
        switch (index.column()) {
            case NIMI:
                return laskutettava.nimi;
            case SAHKOPOSTI:
                return laskutettava.sahkoposti;
        }
    }

    else if( role == ViiteRooli)
    {
        qulonglong pohjanumero =   pohjaviite_ + static_cast<qulonglong>( index.row() );
        return pohjanumero * 10 + LaskuModel::laskeViiteTarkiste(pohjanumero);
    }
    else if( role == NimiRooli)
        return ryhma_.at(index.row()).nimi;
    else if( role == OsoiteRooli)
        return  ryhma_.at(index.row()).osoite;
    else if( role == SahkopostiRooli)
        return  ryhma_.at(index.row()).sahkoposti;
    else if( role == YTunnusRooli)
        return  ryhma_.at(index.row()).ytunnus;
    else if( role == VerkkoLaskuOsoiteRooli)
        return  ryhma_.at(index.row()).verkkolaskuosoite;
    else if( role == VerkkoLaskuValittajaRooli)
        return  ryhma_.at(index.row()).verkkolaskuvalittaja;

    else if( role == Qt::DecorationRole && index.column() == SAHKOPOSTI)
    {
        if( ryhma_.at(index.row()).lahetetty)
            return QIcon(":/pic/ok.png");
    }
    else if( role == Qt::DecorationRole && index.column() == NIMI)
    {
        if(ryhma_.at(index.row()).verkkolaskutettu )
            return QIcon(":/pic/ok.png");
        if( !ryhma_.at(index.row()).verkkolaskuosoite.isEmpty() &&  !ryhma_.at(index.row()).verkkolaskuvalittaja.isEmpty() )
            return QIcon(":/pic/verkkolasku.png");
    }

    return {};
}

Qt::ItemFlags LaskuRyhmaModel::flags(const QModelIndex &index) const
{
    return QAbstractItemModel::flags(index) | Qt::ItemIsDropEnabled;
}

void LaskuRyhmaModel::lisaa(const QString &nimi, const QString &osoite, const QString &sahkoposti, const QString& ytunnus,
                            const QString& verkkolaskuosoite, const QString& verkkolaskuvalittaja)
{
    beginInsertRows(QModelIndex(), ryhma_.count(), ryhma_.count() );
    Laskutettava uusi;
    uusi.nimi = nimi;
    uusi.osoite = osoite;
    uusi.sahkoposti = sahkoposti;
    if( !ytunnus.isEmpty())
    {
        uusi.ytunnus = ytunnus;
        uusi.verkkolaskuosoite = verkkolaskuosoite;
        uusi.verkkolaskuvalittaja = verkkolaskuvalittaja;
    }
    ryhma_.append(uusi);
    endInsertRows();
}

void LaskuRyhmaModel::poista(int indeksi)
{
    beginRemoveRows(QModelIndex(), indeksi, indeksi);
    ryhma_.removeAt(indeksi);
    endRemoveRows();
}

bool LaskuRyhmaModel::onkoNimella(const QString &nimi)
{

    for( const Laskutettava& rivi : ryhma_)
        if( rivi.nimi == nimi)
            return true;

    return false;
}

void LaskuRyhmaModel::sahkopostiLahetetty(int indeksiin)
{
    ryhma_[indeksiin].lahetetty = true;
    emit dataChanged( index(indeksiin, SAHKOPOSTI), index(indeksiin, SAHKOPOSTI) );
}

void LaskuRyhmaModel::finvoiceMuodostettu(int indeksiin)
{

    ryhma_[indeksiin].verkkolaskutettu = true;
    emit dataChanged( index(indeksiin, NIMI), index(indeksiin, NIMI) );
}

bool LaskuRyhmaModel::canDropMimeData(const QMimeData *data, Qt::DropAction /*action*/, int /*row*/, int /*column*/, const QModelIndex &/*parent*/) const
{
    // Testataan, onko tuotavana csv-tiedostoja
    if( data->hasUrls())
    {
        QList<QUrl> urlit = data->urls();
        for(auto url : urlit)
        {
            if( url.isLocalFile())
            {
                QFileInfo info( url.toLocalFile());
                QString polku = info.absoluteFilePath();


                QFile tiedosto(polku);
                if(!tiedosto.open(QIODevice::ReadOnly))
                {
                    qDebug() << tiedosto.errorString();
                    continue;
                }
                QByteArray ba = tiedosto.readAll();

                if( CsvTuonti::onkoCsv(ba) )
                    return true;
            }
        }
    }
    return false;
}

bool LaskuRyhmaModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int /*row*/, int /*column*/, const QModelIndex &/*parent*/)
{
    if( action == Qt::IgnoreAction)
        return true;

    int lisatty = 0;
    if( data->hasUrls())
    {
        QList<QUrl> urlit = data->urls();
        for(auto url : urlit)
        {
            if( url.isLocalFile())
            {
                QFileInfo info( url.toLocalFile());
                QString polku = info.absoluteFilePath();

                QFile tiedosto(polku);
                if(!tiedosto.open(QIODevice::ReadOnly))
                    continue;
                if( !CsvTuonti::onkoCsv(tiedosto.readAll()))
                    continue;

                RyhmanTuontiDlg dlg(polku, nullptr);
                if( dlg.exec() == QDialog::Accepted )
                {
                    dlg.data()->lisaaLaskuun(this);
                    lisatty++;
                }
            }
        }
    }
    return lisatty > 0;
}

