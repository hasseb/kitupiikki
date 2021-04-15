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
#include "eracombo.h"
#include "db/kirjanpito.h"

#include <QDebug>

#include "laskutus/viitenumero.h"

#include "rekisteri/asiakastoimittajalistamodel.h"
#include "huoneistoeranvalintadialog.h"
#include "eraeranvalintadialog.h"
#include "laskutus/huoneisto/huoneistomodel.h"
#include "db/tositetyyppimodel.h"

EraCombo::EraCombo(QWidget *parent) :
    QComboBox (parent)
{
    connect( this, qOverload<int>(&EraCombo::currentIndexChanged),
             this, &EraCombo::vaihtui);
}

int EraCombo::valittuEra() const
{
    return era_.value("id").toInt();
}

EraMap EraCombo::eraMap() const
{
    return era_;
}

void EraCombo::asetaTili(int tili, int asiakas)
{
    tili_ = tili;
    asiakas_ = asiakas;
    asiakasNimi_ = asiakas ? AsiakasToimittajaListaModel::instanssi()->nimi(asiakas) : QString();
    paivita();
}

void EraCombo::valitseUusiEra()
{
    era_.clear();
    era_.insert("id",-1);
}

void EraCombo::valitse(const EraMap &eraMap)
{
    era_ = eraMap;
    paivita();
}

void EraCombo::paivita()
{
    paivitetaan_ = true;
    clear();

    if( era_.eratyyppi() == EraMap::Lasku ) {
        QString txt = QString("%1 %2 %3")
                .arg(era_.pvm().toString("dd.MM.yyyy"))
                .arg(era_.nimi())
                .arg(era_.kumppaniNimi());

        if( era_.saldo() ) {
            txt.append(" (" + era_.saldo().display() + ")");
        }

        addItem( era_.saldo() ? kp()->tositeTyypit()->kuvake(era_.tositetyyppi())
                       : QIcon(":/pic/ok.png"),
                 txt,
                 era_ );
    } else if( era_.eratyyppi() == EraMap::Asiakas) {
        addItem(QIcon(":/pic/mies.png"), era_.value("asiakas").toMap().value("nimi").toString(), era_);
    } else if( era_.contains("huoneisto")) {
        addItem(QIcon(":/pic/talo.png"), era_.value("huoneisto").toMap().value("nimi").toString(), era_);
    }

    addItem(QIcon(":/pic/tyhja.png"), tr("Ei tase-erää"), EraMap(EraMap::EiEraa));
    addItem(QIcon(":/pic/lisaa.png"), tr("Uusi tase-erä"), EraMap(EraMap::Uusi));
    addItem(QIcon(":/pic/lasku.png"), tr("Valitse tase-erä"), EraMap(EraMap::Valitse));

    if( asiakas_ ) {
        QString nimi = AsiakasToimittajaListaModel::instanssi()->nimi(asiakas_);
        addItem(QIcon(":/pic/mies.png"),  asiakasNimi_, EraMap::AsiakasEra(asiakas_, asiakasNimi_) );
    }
    if( kp()->huoneistot()->rowCount() )
        addItem(QIcon(":/pic/talo.png"), tr("Huoneisto"), EraMap(EraMap::Huoneisto));

    setCurrentIndex( findData(era_) );

    paivitetaan_ = false;
}

void EraCombo::vaihtui()
{
    int indeksi = currentIndex();
    if( indeksi < 0 || paivitetaan_) {
        return;
    }

    int vanhaId = era_.id();
    EraMap uusi = currentData().toMap();

    if( uusi.id() == EraMap::Valitse ) {
        EraEranValintaDialog dlg(tili_, asiakas_, era_.id(), this);
        if( dlg.exec() == QDialog::Accepted) {
            valitse( dlg.valittu());
        }     
    } else if( uusi.id() == EraMap::Huoneisto) {
        HuoneistoEranValintaDialog dlg(era_.huoneistoId(), this);
        if( dlg.exec() == QDialog::Accepted) {
            valitse( dlg.valittu() );
        }
    } else {
        era_ = uusi;
    }

    if( vanhaId != era_.id())
        emit valittu( era_);

    paivita();
}



