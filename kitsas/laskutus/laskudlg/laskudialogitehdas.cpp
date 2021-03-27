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
#include "laskudialogitehdas.h"

#include "model/tosite.h"
#include "kantalaskudialogi.h"
#include "tavallinenlaskudialogi.h"
#include "hyvityslaskudialogi.h"
#include "ryhmalaskudialogi.h"
#include "maksumuistusdialogi.h"

#include "db/tositetyyppimodel.h"
#include "db/kitsasinterface.h"
#include "kieli/kielet.h"
#include "db/asetusmodel.h"
#include "model/tositerivi.h"
#include "model/tositerivit.h"


LaskuDialogiTehdas::LaskuDialogiTehdas(KitsasInterface *kitsas, QObject *parent) :
    QObject(parent),
    kitsas_(kitsas)
{

}

void LaskuDialogiTehdas::kaynnista(KitsasInterface *interface, QObject* parent)
{
    instanssi__ = new LaskuDialogiTehdas(interface, parent);
}

void LaskuDialogiTehdas::naytaLasku(int tositeId)
{
    Tosite* tosite = new Tosite(instanssi__);
    connect( tosite, &Tosite::ladattu, instanssi__, &LaskuDialogiTehdas::tositeLadattu);
    tosite->lataa(tositeId);
}

KantaLaskuDialogi *LaskuDialogiTehdas::myyntilasku(int asiakasId)
{
    Tosite* tosite = new Tosite();
    tosite->asetaTyyppi(TositeTyyppi::MYYNTILASKU);
    tosite->asetaKumppani(asiakasId);

    tosite->asetaLaskupvm( paivamaara() );
    tosite->lasku().setKieli( Kielet::instanssi()->nykyinen().toUpper());
    tosite->asetaErapvm( Lasku::oikaiseErapaiva( paivamaara().addDays( instanssi__->kitsas_->asetukset()->luku(AsetusModel::LaskuMaksuaika) ) ) );
    tosite->lasku().setViivastyskorko( instanssi__->kitsas_->asetukset()->asetus(AsetusModel::LaskuPeruskorko).toDouble() + 7.0 );

    KantaLaskuDialogi *dlg = new TavallinenLaskuDialogi(tosite);
    dlg->show();
    return dlg;
}

KantaLaskuDialogi *LaskuDialogiTehdas::ryhmalasku()
{
    Tosite* tosite = new Tosite();
    tosite->asetaTyyppi(TositeTyyppi::MYYNTILASKU);
    tosite->asetaLaskupvm( paivamaara() );
    tosite->asetaErapvm( paivamaara().addDays( instanssi__->kitsas_->asetukset()->luku(AsetusModel::LaskuMaksuaika) ) );

    RyhmaLaskuDialogi *dlg = new RyhmaLaskuDialogi(tosite);
    dlg->show();
    return dlg;
}

void LaskuDialogiTehdas::hyvityslasku(int hyvitettavaTositeId)
{
    Tosite* tosite = new Tosite(instanssi__);
    connect( tosite, &Tosite::ladattu, instanssi__, &LaskuDialogiTehdas::hyvitettavaLadattu);
    tosite->lataa(hyvitettavaTositeId);
}

void LaskuDialogiTehdas::kopioi(int tositeId)
{
    Tosite* tosite = new Tosite(instanssi__);
    connect( tosite, &Tosite::ladattu, instanssi__, &LaskuDialogiTehdas::ladattuKopioitavaksi);
    tosite->lataa(tositeId);
}

void LaskuDialogiTehdas::naytaDialogi(Tosite *tosite)
{
    KantaLaskuDialogi* dlg = nullptr;

    switch (tosite->tyyppi()) {
    case TositeTyyppi::HYVITYSLASKU:
        dlg = new HyvitysLaskuDialogi(tosite);
        break;
    case TositeTyyppi::MAKSUMUISTUTUS:
        dlg = new MaksumuistusDialogi(tosite);
        break;
    default:
        dlg = new TavallinenLaskuDialogi(tosite);
    }

    if( dlg )
        dlg->show();
}

void LaskuDialogiTehdas::tositeLadattu()
{
    Tosite* tosite = qobject_cast<Tosite*>(sender());
    naytaDialogi( tosite );
}

void LaskuDialogiTehdas::hyvitettavaLadattu()
{
    Tosite* hyvitettava = qobject_cast<Tosite*>(sender());
    const Lasku& hyvitettavaLasku = hyvitettava->constLasku();

    Tosite* uusi = new Tosite(instanssi__);
    uusi->asetaTyyppi( TositeTyyppi::HYVITYSLASKU );

    uusi->lasku().setKieli( hyvitettavaLasku.kieli() );
    uusi->lasku().setLahetystapa( hyvitettavaLasku.lahetystapa() );
    uusi->lasku().setAlkuperaisNumero( hyvitettavaLasku.numero().toLongLong() );
    uusi->lasku().setOsoite( hyvitettavaLasku.osoite() );
    uusi->lasku().setEmail( hyvitettavaLasku.email() );
    uusi->lasku().setAlkuperaisPvm( hyvitettavaLasku.laskunpaiva() );
    uusi->asetaViite( hyvitettavaLasku.viite() );
    uusi->lasku().setViite( hyvitettavaLasku.viite() );
    uusi->asetaKumppani( hyvitettava->kumppani() );

    TositeRivit* rivit = hyvitettava->rivit();
    for(int r=0; r < rivit->rowCount(); r++) {
        TositeRivi rivi = rivit->rivi(r);
        const QString& kpl = rivi.laskutetaanKpl();
        rivi.setLaskutetaanKpl( kpl.startsWith("-") ? kpl.mid(1) : "-" + kpl );
        rivi.setMyyntiKpl( 0 - rivi.myyntiKpl() );
        rivi.laskeYhteensa();
        uusi->rivit()->lisaaRivi(rivi);
    }

    HyvitysLaskuDialogi* dlg = new HyvitysLaskuDialogi(uusi);
    dlg->show();

    hyvitettava->deleteLater();
}

void LaskuDialogiTehdas::ladattuKopioitavaksi()
{
    Tosite* tosite = qobject_cast<Tosite*>(sender());

    tosite->setData(Tosite::ID, 0);
    tosite->asetaTila(Tosite::POISTETTU);

    tosite->lasku().setNumero(QString());
    tosite->asetaLaskupvm( paivamaara() );
    tosite->asetaErapvm( paivamaara().addDays( instanssi__->kitsas_->asetukset()->luku(AsetusModel::LaskuMaksuaika) ) );

    ViiteNumero viite( tosite->viite());
    if( viite.tyyppi() != ViiteNumero::ASIAKAS && viite.tyyppi() != ViiteNumero::HUONEISTO) {
        tosite->asetaViite(QString());
    }

    naytaDialogi( tosite );
}

QDate LaskuDialogiTehdas::paivamaara()
{
    return instanssi__->kitsas_ ? instanssi__->kitsas_->paivamaara() : QDate::currentDate();
}

LaskuDialogiTehdas* LaskuDialogiTehdas::instanssi__ = nullptr;
