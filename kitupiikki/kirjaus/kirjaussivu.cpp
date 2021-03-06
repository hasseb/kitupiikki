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


#include <QSplitter>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDir>

#include "kitupiikkiikkuna.h"

#include "kirjaussivu.h"

#include "kirjauswg.h"
#include "naytaliitewg.h"
#include "naytin/naytinview.h"

#include "db/kirjanpito.h"
#include "db/tositemodel.h"

KirjausSivu::KirjausSivu(KitupiikkiIkkuna *ikkuna) :
    KitupiikkiSivu(nullptr), ikkuna_(ikkuna)
{
    model = kp()->tositemodel();

    liitewg = new NaytaliiteWg();
    kirjauswg = new KirjausWg(model);

    splitter = new QSplitter(Qt::Vertical);
    splitter->addWidget(liitewg);
    splitter->addWidget(kirjauswg);

    splitter->restoreState(kp()->settings()->value("KirjausSplitter").toByteArray());

    QHBoxLayout *leiska = new QHBoxLayout;
    leiska->addWidget(splitter);

    setLayout(leiska);

    connect( liitewg, SIGNAL(lisaaLiite(QString)), kirjauswg, SLOT(lisaaLiite(QString)));
    connect( liitewg, &NaytaliiteWg::lisaaLiiteDatalla, kirjauswg, &KirjausWg::lisaaLiiteDatasta);

    connect( kirjauswg, SIGNAL(liiteValittu(QByteArray)), liitewg, SLOT(naytaPdf(QByteArray)));
    connect( kirjauswg, SIGNAL(tositeKasitelty()), this, SLOT(tositeKasitelty()));
    connect( kirjauswg, &KirjausWg::avaaLiite, liitewg->liiteView(), &NaytinView::avaaOhjelmalla);
    connect( kirjauswg, &KirjausWg::tulostaLiite, liitewg->liiteView(), &NaytinView::tulosta);

    connect( splitter, SIGNAL(splitterMoved(int,int)), this, SLOT(talletaSplitter()));
}

KirjausSivu::~KirjausSivu()
{
    delete kirjauswg;
}

void KirjausSivu::siirrySivulle()
{
    palataanTakaisin_ = false;
    kirjauswg->tyhjenna();
}

bool KirjausSivu::poistuSivulta(int minne)
{
    if( model->muokattu() && model->vientiModel()->debetSumma() && model->muokkausSallittu())
    {
        if( minne == KitupiikkiIkkuna::SELAUSSIVU && ikkuna_ )
        {
            // Jos muokatusta tositteesta halutaan poistua selaukseen, avataankin uusi selaus
            ikkuna_->uusiSelausIkkuna();
            return false;
        }

        if( QMessageBox::question(this, tr("Kitupiikki"), tr("Nykyistä kirjausta on muokattu. Poistutko sivulta tallentamatta tekemiäsi muutoksia?")) != QMessageBox::Yes)
        {
            return false;
        }
    }
    return true;
}

void KirjausSivu::naytaTosite(int tositeId)
{
    palataanTakaisin_ = true;

    // Tositteeseen -1 siirtyminen tarkoittaa, että ollaan
    // kirjaamassa lisäikkunalla, jolloin hylkää-nappi sulkee
    // ikkunan

    if( tositeId > -1)
        kirjauswg->lataaTosite(tositeId);
}

void KirjausSivu::tositeKasitelty()
{
    if( palataanTakaisin_)
        emit palaaEdelliselleSivulle();
}

void KirjausSivu::talletaSplitter()
{
    kp()->settings()->setValue("KirjausSplitter", splitter->saveState());
}

void KirjausSivu::lisaaKirjattavienKansiosta()
{
    QDir dir( kp()->asetukset()->asetus("KirjattavienKansio"));
    dir.setFilter( QDir::Files );
    dir.setSorting( QDir::Name );
    QFileInfoList list = dir.entryInfoList();
    for( const QFileInfo& info : list)
    {
        QString tiedostonimi = info.fileName().toLower();
        if( tiedostonimi.endsWith(".pdf")  || tiedostonimi.endsWith(".jpg") ||
            tiedostonimi.endsWith(".jpeg") || tiedostonimi.endsWith(".png"))
        {
            kirjausWg()->lisaaLiite( info.absoluteFilePath() );
            break;  // Lisätään vain yksi
        }
    }
}

