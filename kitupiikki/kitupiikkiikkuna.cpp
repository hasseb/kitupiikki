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

#include <QAction>
#include <QActionGroup>

#include <QStackedWidget>
#include <QToolBar>
#include <QSettings>
#include <QStatusBar>
#include <QFileDialog>
#include <QDateEdit>
#include <QMouseEvent>

#include <QMenuBar>

#include <QDebug>
#include <QDockWidget>

#include "kitupiikkiikkuna.h"

#include "aloitussivu/aloitussivu.h"
#include "kirjaus/kirjaussivu.h"
#include "maaritys/maarityssivu.h"
#include "selaus/selauswg.h"
#include "raportti/raporttisivu.h"
#include "uusikp/uusikirjanpito.h"
#include "ohje/ohjesivu.h"

#include "db/kirjanpito.h"


KitupiikkiIkkuna::KitupiikkiIkkuna(QWidget *parent) : QMainWindow(parent),
    nykysivu(0)
{

    connect( Kirjanpito::db(), SIGNAL(tietokantaVaihtui()), this, SLOT(kirjanpitoLadattu()));

    setWindowIcon(QIcon(":/pic/Possu64.png"));

    // Nämä valikot ovat tässä lähinnä käyttöliittymätestin kannalta ;)
    // menuBar()->addMenu("Kirjanpito");
    // menuBar()->addMenu("Tosite");
    // menuBar()->addMenu("Raportit");



    aloitussivu = new AloitusSivu();

    kirjaussivu =  new KirjausSivu();
    selaussivu = new SelausWg();
    maarityssivu = new MaaritysSivu();
    raporttisivu = new RaporttiSivu();
    ohjesivu = new OhjeSivu();


    pino = new QStackedWidget;
    setCentralWidget(pino);

    lisaaSivut();
    luoHarjoitusDock();


    // Himmennetään ne valinnat, jotka mahdollisia vain kirjanpidon ollessa auki
    for(int i=KIRJAUSSIVU; i<OHJESIVU;i++)
        sivuaktiot[i]->setEnabled(false);

    QSettings settings;
    restoreGeometry( settings.value("geometry").toByteArray());
    // Ladataan viimeksi avoinna ollut kirjanpito
    if( settings.contains("viimeisin"))
        Kirjanpito::db()->avaaTietokanta(settings.value("viimeisin").toString());

    connect( selaussivu, SIGNAL(tositeValittu(int)), this, SLOT(naytaTosite(int)) );
    connect( aloitussivu, SIGNAL(selaus(int,Tilikausi)), this, SLOT(selaaTilia(int,Tilikausi)));
    connect( kirjaussivu, SIGNAL(palaaEdelliselleSivulle()), this, SLOT(palaaSivulta()));


}

KitupiikkiIkkuna::~KitupiikkiIkkuna()
{
    QSettings settings;
    settings.setValue("geometry",saveGeometry());
    settings.setValue("viimeisin", Kirjanpito::db()->hakemisto().absoluteFilePath("kitupiikki.sqlite"));
}

void KitupiikkiIkkuna::valitseSivu(int mikasivu, bool paluu)
{
    if( !paluu )
        edellisetIndeksit.push( pino->currentIndex() );

    if( nykysivu && !nykysivu->poistuSivulta())
    {
        // Sivulta ei saa poistua!
        // Palautetaan valinta nykyiselle sivulle
        sivuaktiot[ pino->currentIndex() ]->setChecked(true);
        return;
    }

    nykysivu = sivut[mikasivu];
    sivuaktiot[mikasivu]->setChecked(true);

    // Laittaa sivun valmiiksi
    nykysivu->siirrySivulle();

    // Sivu esille
    pino->setCurrentWidget( nykysivu);

}


void KitupiikkiIkkuna::kirjanpitoLadattu()
{
    if( !Kirjanpito::db()->asetus("Nimi").isEmpty())
    {
        if( Kirjanpito::db()->onkoHarjoitus())
            setWindowTitle( tr("%1 - Kitupiikki [Harjoittelu]").arg(Kirjanpito::db()->asetus("Nimi")));
        else
            setWindowTitle( tr("%1 - Kitupiikki").arg(Kirjanpito::db()->asetus("Nimi")));

        harjoitusDock->setVisible( Kirjanpito::db()->onkoHarjoitus());

        for(int i=KIRJAUSSIVU; i<OHJESIVU;i++)
            sivuaktiot[i]->setEnabled(true);

        selaussivu->alusta();
    }

    valitseSivu(ALOITUSSIVU);
    edellisetIndeksit.clear();  // Tyhjennetään "selaushistoria"
}

void KitupiikkiIkkuna::palaaSivulta()
{
    if( !edellisetIndeksit.isEmpty())
        valitseSivu( edellisetIndeksit.pop(), true );
}

void KitupiikkiIkkuna::selaaTilia(int tilinumero, Tilikausi tilikausi)
{
    valitseSivu( SELAUSSIVU );
    selaussivu->selaa(tilinumero, tilikausi);
}

void KitupiikkiIkkuna::aktivoiSivu(QAction *aktio)
{
    int sivu = aktio->data().toInt();
    if( sivu == KIRJAUSSIVU)
    {
        // Kun kirjaussivu valitaan, tyhjennetään edellisten luettelo jottei
        // tule paluuta kirjauksen jälkeen
        edellisetIndeksit.clear();
        valitseSivu(KIRJAUSSIVU, true);
    }
    else
        valitseSivu(sivu);
}

void KitupiikkiIkkuna::naytaTosite(int tositeid)
{
    valitseSivu( KIRJAUSSIVU );
    kirjaussivu->naytaTosite(tositeid);
}

void KitupiikkiIkkuna::mousePressEvent(QMouseEvent *event)
{
    // Vähän kokeellista: palataan edelliselle sivulle, jos menty Käy-valinnalla ;)
    if( event->button() == Qt::BackButton )
        palaaSivulta();
}

QAction *KitupiikkiIkkuna::lisaaSivu(const QString &nimi, const QString &kuva, const QString &vihje, const QString &pikanappain, Sivu sivutunnus,
                                     KitupiikkiSivu *sivu)
{
    QAction *uusi = new QAction( nimi, aktioryhma);
    uusi->setIcon( QIcon(kuva));
    uusi->setStatusTip(vihje);
    uusi->setShortcut(QKeySequence(pikanappain));
    uusi->setCheckable(true);
    uusi->setActionGroup(aktioryhma);
    uusi->setData(sivutunnus);
    toolbar->addAction(uusi);

    sivuaktiot[sivutunnus] = uusi;
    sivut[sivutunnus] = sivu;

    pino->addWidget(sivu);

    return uusi;
}

void KitupiikkiIkkuna::lisaaSivut()
{
    // Luodaan vasemman reunan työkalupalkki
    toolbar = new QToolBar(this);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolbar->setIconSize(QSize(64,64));
    toolbar->setStyleSheet("QToolBar {background-color: darkGray; spacing: 5px; }  QToolBar::separator { border: none; margin-bottom: 16px; }  QToolButton { border: 0px solid lightgray; margin-right: 0px; font-size: 8pt; }  QToolButton:checked {background-color: lightGray; } QToolButton:hover { font-size: 9pt; font-weight: bold; } ");
    toolbar->setMovable(false);

    aktioryhma = new QActionGroup(this);
    lisaaSivu("Aloita",":/pic/Possu64.png","Erilaisia ohjattuja toimia","Home", ALOITUSSIVU, aloitussivu);
    lisaaSivu("Uusi\ntosite",":/pic/uusitosite.png","Kirjaa uusi tosite","Ctrl+N", KIRJAUSSIVU, kirjaussivu);
    lisaaSivu("Selaa",":/pic/Paivakirja64.png","Selaa kirjauksia aikajärjestyksessä","F3", SELAUSSIVU, selaussivu);
    lisaaSivu("Tulosteet",":/pic/print.png","Tulosta erilaisia raportteja","Ctrl+P", TULOSTESIVU, raporttisivu);
    lisaaSivu("Määritykset",":/pic/ratas.png","Kirjanpitoon liittyvät määritykset","F6", MAARITYSSIVU, maarityssivu);
    lisaaSivu("Ohje",":/pic/ohje.png","Kitupiikin ohjeet","F1", OHJESIVU, ohjesivu);
    aktioryhma->actions().first()->setChecked(true);

    connect(aktioryhma, SIGNAL(triggered(QAction*)), this, SLOT(aktivoiSivu(QAction*)));

    addToolBar(Qt::LeftToolBarArea, toolbar);
}


void KitupiikkiIkkuna::luoHarjoitusDock()
{
    QLabel *teksti = new QLabel("<b>Harjoittelutila käytössä</b> Voit nopeuttaa ajan kulumista");
    teksti->setStyleSheet("color: white;");

    QDateEdit *pvmedit = new QDateEdit;
    pvmedit->setDate( QDate::currentDate());
    pvmedit->setStyleSheet("background: white;");

    QHBoxLayout *leiska = new QHBoxLayout;
    leiska->addWidget(teksti, 3);
    leiska->addWidget(pvmedit,1, Qt::AlignRight);

    QWidget *wg = new QWidget;
    wg->setLayout(leiska);

    harjoitusDock = new QDockWidget;
    harjoitusDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    harjoitusDock->setWidget(wg);
    harjoitusDock->setStyleSheet("background: green");
    harjoitusDock->setTitleBarWidget(new QWidget(this));

    addDockWidget(Qt::TopDockWidgetArea, harjoitusDock);
    connect( pvmedit, SIGNAL(dateChanged(QDate)), Kirjanpito::db(), SLOT(asetaHarjoitteluPvm(QDate)));
    harjoitusDock->setVisible(false);
}
