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

#include <QFile>
#include <QStringList>
#include <QSqlQuery>
#include <QSslSocket>
#include <QNetworkRequest>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QFileDialog>
#include <QDesktopServices>
#include <QListWidget>
#include <QMessageBox>

#include <QRegularExpression>

#include <QDialog>
#include <QDebug>


#include <QSettings>
#include <QSysInfo>

#include "ui_aboutdialog.h"
#include "ui_muistiinpanot.h"

#include "aloitussivu.h"
#include "db/kirjanpito.h"
#include "versio.h"
#include "pilvi/pilvimodel.h"
#include "sqlite/sqlitemodel.h"

#include "uusikirjanpito/uusivelho.h"
#include "maaritys/tilikarttapaivitys.h"
#include "alv/alvilmoitustenmodel.h"
#include "kitupiikkituonti/vanhatuontidlg.h"
#include "pilvi/pilveensiirto.h"

#include <QJsonDocument>
#include <QTimer>
#include <QSortFilterProxyModel>

#include "tilaus/tilauswizard.h"
#include "versio.h"

#include "luotunnusdialogi.h"
#include "salasananvaihto.h"

#include "tools/kitsaslokimodel.h"
#include "kieli/kielet.h"

#include "maaritys/tilitieto/tilitietopalvelu.h"

#include "loginservice.h"
#include "toffeelogin.h"

#include <QSslError>
#include <QClipboard>
#include <QSize>

AloitusSivu::AloitusSivu(QWidget *parent) :
    KitupiikkiSivu(parent),
    login_{new LoginService(this)}
{

    ui = new Ui::Aloitus;
    ui->setupUi(this);

    initLoginService();

    ui->selain->setOpenLinks(false);

    connect( ui->uusiNappi, &QPushButton::clicked, this, &AloitusSivu::uusiTietokanta);
    connect( ui->avaaNappi, &QPushButton::clicked, this, &AloitusSivu::avaaTietokanta);
    connect( ui->kitupiikkituontiNappi, &QPushButton::clicked, this, &AloitusSivu::tuoKitupiikista);
    connect( ui->tietojaNappi, SIGNAL(clicked(bool)), this, SLOT(abouttiarallaa()));
    connect( ui->tilikausiCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(haeSaldot()));
    connect(ui->varmistaNappi, &QPushButton::clicked, this, &AloitusSivu::varmuuskopioi);
    connect(ui->muistiinpanotNappi, &QPushButton::clicked, this, &AloitusSivu::muistiinpanot);
    connect(ui->poistaNappi, &QPushButton::clicked, this, &AloitusSivu::poistaListalta);

    connect( ui->tilikausiCombo, &QComboBox::currentTextChanged, this, &AloitusSivu::haeSaldot);    

    connect( ui->selain, SIGNAL(anchorClicked(QUrl)), this, SLOT(linkki(QUrl)));

    connect( kp(), SIGNAL(tietokantaVaihtui()), this, SLOT(kirjanpitoVaihtui()));
    connect( kp()->asetukset(), &AsetusModel::asetusMuuttui, this, &AloitusSivu::kirjanpitoVaihtui);
    connect( Kielet::instanssi(), &Kielet::kieliVaihtui, this, &AloitusSivu::haeSaldot );

    connect( kp()->pilvi(), &PilviModel::kirjauduttu, this, &AloitusSivu::kirjauduttu);
    connect( ui->logoutButton, &QPushButton::clicked, this, &AloitusSivu::pilviLogout );
    connect( ui->rekisteroiButton, &QPushButton::clicked, this, [] {
        LuoTunnusDialogi dialogi;
        dialogi.exec();
    });

    connect( ui->viimeisetView, &QListView::clicked,
             [] (const QModelIndex& index) { kp()->sqlite()->avaaTiedosto( index.data(SQLiteModel::PolkuRooli).toString() );} );

    connect( ui->pilviView, &QListView::clicked,
             [](const QModelIndex& index) { kp()->pilvi()->avaaPilvesta( index.data(PilviModel::IdRooli).toInt() ); } );


    connect( ui->tilausButton, &QPushButton::clicked, this,
             [] () { TilausWizard *tilaus = new TilausWizard(); tilaus->nayta(); });

    connect( ui->kopioiPilveenNappi, &QPushButton::clicked, this, &AloitusSivu::siirraPilveen);
    connect( ui->pilviPoistaButton, &QPushButton::clicked, this, &AloitusSivu::poistaPilvesta);

    connect( kp(), &Kirjanpito::logoMuuttui, this, &AloitusSivu::logoMuuttui);

    connect( ui->vaihdaSalasanaButton, &QPushButton::clicked, this, &AloitusSivu::vaihdaSalasanaUuteen);


    QSortFilterProxyModel* sqliteproxy = new QSortFilterProxyModel(this);
    sqliteproxy->setSourceModel( kp()->sqlite());
    ui->viimeisetView->setModel( sqliteproxy );
    sqliteproxy->setSortRole(Qt::DisplayRole);

    ui->pilviView->setModel( kp()->pilvi() );
    ui->tkpilviTab->setCurrentIndex( kp()->settings()->value("TietokonePilviValilehti").toInt() );
    ui->vaaraSalasana->setVisible(false);
    ui->palvelinvirheLabel->setVisible(false);

    ui->inboxFrame->setVisible(false);
    ui->inboxFrame->installEventFilter(this);
    ui->outboxFrame->setVisible(false);
    ui->outboxFrame->installEventFilter(this);
    ui->tilioimattaFrame->setVisible(false);
    ui->tilioimattaFrame->installEventFilter(this);

    if( kp()->pilvi()->kayttaja()) {
        kirjauduttu( kp()->pilvi()->kayttaja() );
    } else if( kp()->settings()->contains("Authkey")) {
        ui->pilviPino->setCurrentIndex(SISAANTULO);
        qApp->processEvents();
        QTimer::singleShot(250, login_, &LoginService::keyLogin );
    }

    pyydaInfo();
}

AloitusSivu::~AloitusSivu()
{
    kp()->settings()->setValue("TietokonePilviValilehti", ui->tkpilviTab->currentIndex() );
    delete ui;
}


void AloitusSivu::siirrySivulle()
{

    if( ui->tilikausiCombo->currentIndex() < 0)
        ui->tilikausiCombo->setCurrentIndex( kp()->tilikaudet()->indeksiPaivalle( QDate::currentDate() ) );
    if( ui->tilikausiCombo->currentIndex() < 0)
        ui->tilikausiCombo->setCurrentIndex( ui->tilikausiCombo->count()-1 );

    if( !sivulla )
    {
        connect( kp(), &Kirjanpito::kirjanpitoaMuokattu, this, &AloitusSivu::haeSaldot);
        sivulla = true;
    }

    // Päivitetään aloitussivua
    if( kp()->yhteysModel() )
    {        
        haeSaldot();
        haeKpInfo();
    }
    else
    {
        naytaTervetuloTiedosto();
    }
    ui->muistiinpanotNappi->setEnabled( kp()->yhteysModel() && kp()->yhteysModel()->onkoOikeutta(YhteysModel::ASETUKSET) );
}

void AloitusSivu::paivitaSivu()
{
    if( kp()->yhteysModel() )
    {
        QString txt("<html><head><link rel=\"stylesheet\" type=\"text/css\" href=\"qrc:/aloitus/aloitus.css\"></head><body>");
        if( !kp()->yhteysModel()->oikeudet()) {

            txt = QString("<h1>%1</h1><p>%2</p><p>%3</p>")
                                .arg(tr("Tämä kirjanpito ei ole käytettävissä"))
                                .arg(tr("Kirjanpidon omistajalla ei ole voimassa olevaa tilausta, tilausmaksu on maksamatta tai "
                                        "palvelun käyttö on estetty käyttösääntöjen vastaisena."))
                                .arg(tr("Kirjanpidon palauttamiseksi käyttöön on oltava yhteydessä Kitsas Oy:n myyntiin."));

        } else {


            txt.append( paivitysInfo );

            int saldoja = saldot_.count();

            if( saldoja == 0) {
                // Lataaminen on kesken..
                txt.append("<h1>" + tr("Avataan kirjanpitoa...") + "</h1>");
            } else if( saldoja < 3) {
                // Ei vielä tilejä avattu, possu
                txt.append( vinkit() );
                txt.append("<p><img src=qrc:/pic/kitsas150.png></p>");
            } else {
                txt.append( vinkit() );
                txt.append(summat());
            }


        }
        ui->selain->setHtml(txt);
    }
    else
    {
        naytaTervetuloTiedosto();
    }
}

bool AloitusSivu::poistuSivulta(int /* minne */)
{
    disconnect( kp(), &Kirjanpito::kirjanpitoaMuokattu, this, &AloitusSivu::haeSaldot);
    sivulla = false;
    return true;
}

void AloitusSivu::kirjanpitoVaihtui()
{
    saldot_.clear();

    bool avoinna = kp()->yhteysModel();

    ui->nimiLabel->setVisible(avoinna && !kp()->asetukset()->onko("LogossaNimi") );
    ui->tilikausiCombo->setVisible(avoinna );
    ui->logoLabel->setVisible( avoinna && !kp()->logo().isNull());
    ui->varmistaNappi->setEnabled(avoinna && qobject_cast<SQLiteModel*>(kp()->yhteysModel()) );
    ui->muistiinpanotNappi->setEnabled(avoinna);
    ui->poistaNappi->setEnabled( avoinna && qobject_cast<SQLiteModel*>(kp()->yhteysModel()));
    ui->pilviPoistaButton->setVisible( avoinna &&
                                       qobject_cast<PilviModel*>(kp()->yhteysModel()) &&
                                       kp()->pilvi()->onkoOikeutta(PilviModel::OMISTAJA) );

    if( avoinna )
    {
        // Kirjanpito avattu
        ui->nimiLabel->setText( kp()->asetukset()->asetus(AsetusModel::OrganisaatioNimi));

        ui->tilikausiCombo->setModel( kp()->tilikaudet() );
        ui->tilikausiCombo->setModelColumn( 0 );

        // Valitaan nykyinen tilikausi
        // Pohjalle kuitenkin viimeinen tilikausi, jotta joku on aina valittuna
        ui->tilikausiCombo->setCurrentIndex( kp()->tilikaudet()->rowCount(QModelIndex()) - 1 );

        for(int i=0; i < kp()->tilikaudet()->rowCount(QModelIndex());i++)
        {
            Tilikausi kausi = kp()->tilikaudet()->tilikausiIndeksilla(i);
            if( kausi.alkaa() <= kp()->paivamaara() && kausi.paattyy() >= kp()->paivamaara())
            {
                ui->tilikausiCombo->setCurrentIndex(i);
                break;
            }
        }
    } else {
        ui->inboxFrame->hide();
        ui->outboxFrame->hide();
        ui->tilioimattaFrame->hide();
    }

    if( !kp()->asetukset()->asetus(AsetusModel::Tilikartta).isEmpty() )
        kp()->settings()->setValue("Tilikartta", kp()->asetukset()->asetus(AsetusModel::Tilikartta));

    bool pilvessa = qobject_cast<PilviModel*>( kp()->yhteysModel() );
    bool paikallinen = qobject_cast<SQLiteModel*>(kp()->yhteysModel());

    ui->paikallinenKuva->setVisible(paikallinen);
    ui->pilviKuva->setVisible( pilvessa );
    ui->kopioiPilveenNappi->setVisible(paikallinen && kp()->pilvi()->kayttaja() && kp()->pilvi()->kayttaja().moodi() == PilviKayttaja::NORMAALI);

    haeSaldot();

    QTimer::singleShot( 800, this, &AloitusSivu::paivitaSivu );
    QTimer::singleShot( 5000, this, &AloitusSivu::paivitaSivu );

}

void AloitusSivu::linkki(const QUrl &linkki)
{
    if( linkki.scheme() == "ohje")
    {
        kp()->ohje( linkki.path() );
    }
    else if( linkki.scheme() == "selaa")
    {
        Tilikausi kausi = kp()->tilikaudet()->tilikausiIndeksilla( ui->tilikausiCombo->currentIndex() );
        QString tiliteksti = linkki.fileName();
        emit selaus( tiliteksti.toInt(), kausi );
    }
    else if( linkki.scheme() == "ktp")
    {
        QString toiminto = linkki.path().mid(1);

        if( toiminto == "uusi")
            uusiTietokanta();
        else
            emit ktpkasky(toiminto);
    }
    else if( linkki.scheme().startsWith("http"))
    {        
        Kirjanpito::avaaUrl( linkki );
    }
}




void AloitusSivu::uusiTietokanta()
{
    UusiVelho velho( parentWidget() );
    if( velho.exec() ) {
        if( velho.field("pilveen").toBool())
            kp()->pilvi()->uusiPilvi(velho.data());
        else {
            if(kp()->sqlite()->uusiKirjanpito(velho.polku(), velho.data())) {
                kp()->sqlite()->avaaTiedosto(velho.polku());
            }
        }
    }
}

void AloitusSivu::avaaTietokanta()
{
    QString polku = QFileDialog::getOpenFileName(this, "Avaa kirjanpito",
                                                 QDir::homePath(),"Kirjanpito (*.kitsas)");
    if( !polku.isEmpty())
        kp()->sqlite()->avaaTiedosto( polku );

}

void AloitusSivu::tuoKitupiikista()
{
    VanhatuontiDlg dlg(this);
    dlg.exec();
}

void AloitusSivu::abouttiarallaa()
{
    Ui::AboutDlg aboutUi;
    QDialog aboutDlg;
    aboutUi.setupUi( &aboutDlg);
    connect( aboutUi.aboutQtNappi, &QPushButton::clicked, qApp, &QApplication::aboutQt);

    QString versioteksti = QString("<b>Versio %1</b><br>Käännetty %2")
            .arg( qApp->applicationVersion(), buildDate().toString("dd.MM.yyyy"));

    QString kooste(KITSAS_BUILD);
    if( !kooste.isEmpty())
        versioteksti.append("<br>Kooste " + kooste);

    aboutUi.versioLabel->setText(versioteksti);

    aboutDlg.exec();
}

void AloitusSivu::infoSaapui()
{    
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
   if( !reply->error()) {

        QVariantMap map = QJsonDocument::fromJson( reply->readAll() ).toVariant().toMap();

        QVariantList lista = map.value("info").toList();
        Kirjanpito::asetaOhjeOsoite( map.value("docs").toString() );
        palauteUrl_ = map.value("feedback").toString();

        paivitysInfo.clear();

        kp()->settings()->setValue("TilastoPaivitetty", QDate::currentDate());

        for( const auto& item : qAsConst(lista)) {
            const auto& map = item.toMap();
            paivitysInfo.append(QString("<table width=100% class=%1><tr><td><h3>%2</h3><p>%3</p></td></tr></table>")
                                .arg(map.value("type").toString(),map.value("title").toString(), map.value("info").toString()));
        }

        paivitaSivu();
    }

    reply->deleteLater();
}

void AloitusSivu::varmuuskopioi()
{
    QString tiedosto = kp()->sqlite()->tiedostopolku();
    bool onnistui = false;

    // Suljetaan tiedostoyhteys, jotta saadaan varmasti varmuuskopioitua kaikki
    const QString& asetusAvain = kp()->asetukset()->asetus(AsetusModel::UID) + "/varmistuspolku";
    const QString varmuushakemisto = kp()->settings()->value( asetusAvain, QDir::homePath()).toString();

    kp()->sqlite()->sulje();

    QFileInfo info(tiedosto);

    QString polku = QString("%1/%2-%3.kitsas")
            .arg(varmuushakemisto)
            .arg(info.baseName())
            .arg( QDate::currentDate().toString("yyMMdd"));

    QString tiedostoon = QFileDialog::getSaveFileName(this, tr("Varmuuskopioi kirjanpito"), polku, tr("Kirjanpito (*.kitsas)") );


    if( tiedostoon == tiedosto)
    {
        QMessageBox::critical(this, tr("Virhe"), tr("Tiedostoa ei saa kopioida itsensä päälle!"));
        tiedostoon.clear();
    }

    if( QFile::exists(tiedostoon))
        QFile::remove(tiedostoon);

    if( !tiedostoon.isEmpty() )
    {
        QFile kirjanpito( tiedosto);
        if( kirjanpito.copy(tiedostoon) ) {
            QMessageBox::information(this, kp()->asetukset()->asetus(AsetusModel::OrganisaatioNimi), tr("Kirjanpidon varmuuskopiointi onnistui."));
            onnistui = true;
        } else {
            QMessageBox::critical(this, tr("Virhe"), tr("Tiedoston varmuuskopiointi epäonnistui."));
        }
    }
    // Avataan tiedosto uudestaan
    kp()->sqlite()->avaaTiedosto(tiedosto);
    if( onnistui ) {
        QFileInfo varmuusinfo(tiedostoon);
        kp()->settings()->setValue( asetusAvain, varmuusinfo.absolutePath() );
        kp()->asetukset()->aseta("Varmuuskopioitu", QDateTime::currentDateTime().toString(Qt::ISODate));
    }
}

void AloitusSivu::muistiinpanot()
{
    QDialog dlg(this);
    Ui::Muistiinpanot ui;
    ui.setupUi(&dlg);

    ui.editori->setPlainText( kp()->asetukset()->asetus(AsetusModel::Muistiinpanot) );
    if( dlg.exec() == QDialog::Accepted )
        kp()->asetukset()->aseta(AsetusModel::Muistiinpanot, ui.editori->toPlainText());

    paivitaSivu();
}

void AloitusSivu::poistaListalta()
{

    if( QMessageBox::question(this, tr("Poista kirjanpito luettelosta"),
                              tr("Haluatko poistaa tämän kirjanpidon viimeisten kirjanpitojen luettelosta?\n"
                                 "Kirjanpitoa ei poisteta levyltä."),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;

    kp()->sqlite()->poistaListalta( kp()->sqlite()->tiedostopolku() );
}

void AloitusSivu::poistaPilvesta()
{
    if( !kp()->onkoHarjoitus()) {
        QMessageBox::critical(this, tr("Kirjanpidon poistaminen"),
                              tr("Turvallisuussyistä voit poistaa vain harjoittelutilassa olevan kirjanpidon.\n\n"
                                 "Poistaaksesi tämän kirjanpidon sinun pitää ensin asettaa Asetukset/Perusvalinnat-sivulla "
                                 "määritys Harjoituskirjanpito."));
        return;
    }

    if( QMessageBox::question(this, tr("Kirjanpidon poistaminen"),
                              tr("Haluatko todella poistaa tämän kirjanpidon %1 pysyvästi?").arg(kp()->asetukset()->nimi()),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    kp()->pilvi()->poistaNykyinenPilvi();
}

void AloitusSivu::pyydaInfo()
{
    QVariantMap tilasto;
    if( kp()->settings()->contains("TilastoPaivitetty")) {
        tilasto.insert("lastasked", kp()->settings()->value("TilastoPaivitetty").toDate());
    }
    tilasto.insert("application", qApp->applicationName());
    tilasto.insert("version", qApp->applicationVersion());
    tilasto.insert("build", KITSAS_BUILD);
    tilasto.insert("os", QSysInfo::prettyProductName());
    tilasto.insert("language", Kielet::instanssi()->uiKieli());
    tilasto.insert("builddate", buildDate().toString(Qt::ISODate));

    // Lista niistä tilikartoista, joita on käytetty viimeisimmän kuukauden aikana
    QStringList kartat;
    kp()->settings()->beginGroup("tilastokartta");
    for(const auto& kartta : kp()->settings()->allKeys()) {
        QDate kaytetty = kp()->settings()->value(kartta).toDate();
        if( kaytetty > QDate::currentDate().addMonths(-1)) {
            kartat.append(kartta);
        }
    }
    kp()->settings()->endGroup();
    tilasto.insert("maps", kartat);

    QByteArray ba = QJsonDocument::fromVariant(tilasto).toJson();
    QString osoite = kp()->pilvi()->pilviLoginOsoite() + "/updateinfo";

    QNetworkRequest pyynto = QNetworkRequest( QUrl(osoite));
    pyynto.setRawHeader("Content-type","application/json");
    QNetworkReply *reply = kp()->networkManager()->post(pyynto, ba);
    connect( reply, &QNetworkReply::finished, this, &AloitusSivu::infoSaapui);
}

void AloitusSivu::saldotSaapuu(QVariant *data)
{
    saldot_ = data->toMap();
    paivitaSivu();
}

void AloitusSivu::kpInfoSaapuu(QVariant *data)
{
    QVariantMap map = data ? data->toMap() : QVariantMap();

    // Asetetaan käyttäjälle näytettävä tuki-info

    QString lisaInfo;
    for(const QString& avain : map.keys()) {
        lisaInfo.append(QString("%1: %2 \n").arg(avain, map.value(avain).toString()));
    }

    ui->inboxFrame->setVisible( map.value("tyolista").toInt() );
    ui->inboxCount->setText( map.value("tyolista").toString() );

    ui->outboxFrame->setVisible( map.value("lahetettavia").toInt());
    ui->outboxCount->setText( map.value("lahetettavia").toString());

    ui->tilioimattaFrame->setVisible( map.value("tilioimatta").toInt() );
    ui->tilioimattaCount->setText( map.value("tilioimatta").toString());

    tilioimatta_ = map.value("tilioimatta").toInt();

    paivitaSivu();
}


QDate AloitusSivu::buildDate()
{
    QString koostepaiva(__DATE__);      // Tämä päivittyy aina versio.h:ta muutettaessa
    return QDate::fromString( koostepaiva.mid(4,3) + koostepaiva.left(3) + koostepaiva.mid(6), Qt::RFC2822Date);
}

bool AloitusSivu::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress && target == ui->inboxFrame) {
        emit ktpkasky("inbox");
        return true;
    } else if (event->type() == QEvent::MouseButtonPress && target == ui->outboxFrame) {
        emit ktpkasky("outbox");
        return true;
    } else if( event->type() == QEvent::MouseButtonPress && target == ui->tilioimattaFrame ) {
        emit ktpkasky("huomio");
        return true;
    }
    return false;
}


void AloitusSivu::kirjauduttu(const PilviKayttaja& kayttaja)
{    
    ui->salaEdit->clear();

    if( !kayttaja && kayttaja.moodi() == PilviKayttaja::NORMAALI) {
        ui->pilviPino->setCurrentIndex(KIRJAUDU);        
        return;
    }

    ui->pilviPino->setCurrentIndex(LISTA);
    ui->kayttajaLabel->setText( kayttaja.nimi() );

    ui->planLabel->setStyleSheet("");

    if( kayttaja.suljettu() ) {
        switch (kayttaja.suljettu()) {
            case PilviKayttaja::MAKSAMATON:
                ui->planLabel->setText(tr("Käyttö estetty maksamattomien tilausmaksujen takia"));
                break;
            case PilviKayttaja::EHTOJEN_VASTAINEN:
                ui->planLabel->setText(tr("Käyttö estetty käyttösääntöjen vastaisen toiminnan takia"));
                break;
        default:
            ui->planLabel->setText(tr("Käyttäjätunnus suljettu"));
        }
        ui->planLabel->setStyleSheet("color: red; font-weight: bold;");
    } else if( kayttaja.planId()) {
        ui->planLabel->setText( kayttaja.planName() );
    } else if( kayttaja.trialPeriod().isValid()) {
        ui->planLabel->setText(tr("Kokeilujakso %1 saakka").arg( kayttaja.trialPeriod().toString("dd.MM.yyyy") ));
    } else {
        ui->planLabel->clear();
    }


    bool naytaNormaalit = kayttaja.moodi() == PilviKayttaja::NORMAALI || kayttaja.planId();

    ui->tilausButton->setVisible( naytaNormaalit );
    ui->uusiNappi->setVisible( naytaNormaalit );
    ui->tkpilviTab->setTabVisible(TIETOKONE_TAB, naytaNormaalit || kp()->sqlite()->rowCount());

    ui->tilausButton->setText( kp()->pilvi()->kayttaja().planId() ? tr("Tilaukseni") : tr("Tee tilaus") );

}

void AloitusSivu::loginVirhe()
{
    ui->pilviPino->setCurrentIndex(KIRJAUDU);
    ui->vaaraSalasana->setVisible(!ui->salaEdit->text().isEmpty());
    ui->salaEdit->clear();
}


void AloitusSivu::pilviLogout()
{
    ui->salaEdit->clear();
    kp()->pilvi()->kirjauduUlos();
    ui->vaaraSalasana->hide();

    // Toffee-moodissa ulos kirjauduttaessa annetaan heti uusi kirjautumisikkuna
    if( kp()->pilvi()->kayttaja().moodi() == PilviKayttaja::TOFFEE) {
        ToffeeLogin dlg(this);
        dlg.exec();
    } else {
        ui->pilviPino->setCurrentIndex(KIRJAUDU);
    }
}

void AloitusSivu::logoMuuttui()
{
    if( kp()->logo().isNull() )
        ui->logoLabel->hide();
    else {
        double skaala = (1.0 * kp()->logo().width() ) / kp()->logo().height();
        ui->logoLabel->setPixmap( QPixmap::fromImage( kp()->logo().scaled( qRound( 64 * skaala),64,Qt::KeepAspectRatio) ) );
        ui->logoLabel->show();
    }
}

void AloitusSivu::haeSaldot()
{
    if(sivulla && kp()->yhteysModel() &&  kp()->yhteysModel()->onkoOikeutta(YhteysModel::TOSITE_SELAUS | YhteysModel::RAPORTIT | YhteysModel::TILINPAATOS)) {
        QDate saldopaiva = ui->tilikausiCombo->currentData(TilikausiModel::PaattyyRooli).toDate();
        KpKysely *kysely = kpk("/saldot");
        if( kysely && saldopaiva.isValid()) {
            kysely->lisaaAttribuutti("pvm", saldopaiva);
            connect( kysely, &KpKysely::vastaus, this, &AloitusSivu::saldotSaapuu);
            kysely->kysy();
        }
    } else {
        paivitaSivu();
    }
}

void AloitusSivu::haeKpInfo()
{
    KpKysely* kysely = kpk("/info");
    if( kysely ) {
        connect( kysely, &KpKysely::vastaus, this, &AloitusSivu::kpInfoSaapuu);
        kysely->kysy();
    }
}

void AloitusSivu::siirraPilveen()
{
    PilveenSiirto *siirtoDlg = new PilveenSiirto();
    siirtoDlg->exec();
}


void AloitusSivu::vaihdaSalasanaUuteen()
{
    Salasananvaihto dlg(this);
    dlg.exec();
}

void AloitusSivu::initLoginService()
{
    login_->registerWidgets( ui->emailEdit, ui->salaEdit,
                             ui->palvelinvirheLabel, ui->muistaCheck,
                             ui->loginButton, ui->salasanaButton);

}

QString AloitusSivu::taulu(const QString &luokka, const QString &otsikko, const QString &teksti, const QString &linkki, const QString &kuva, const QString ohjelinkki)
{
    QString ulos = QString("<table class=%1 width='100%'><tr>").arg(luokka);
    if( !kuva.isEmpty()) {
        ulos.append(QString("<td width='80px'><img src=\"qrc:/pic/%1\" width=64 height=64></td><td>").arg(kuva));
    }
    ulos.append("\n<td class=content width='100%'><h3>");
    if(!linkki.isEmpty()){
        ulos.append(QString("<a href=\"%1\">").arg(linkki));
    }
    ulos.append(otsikko);
    if( !linkki.isEmpty()) {
        ulos.append("</a>");
    }
    ulos.append("</h3>\n<p>");
    ulos.append(teksti);
    if( !ohjelinkki.isEmpty()) {
        ulos.append(QString(" <a href=\"ohje:/%1\">%2</a>").arg(ohjelinkki, tr("Ohje")));
    }
    ulos.append("</p></td></tr></table>\n");
    return ulos;
}

QString AloitusSivu::vinkit()
{
    QString vinkki;

    if( tilioimatta_ ) {
        vinkki.append(taulu("tilioimatta", tr("Tositteiden tiliöinti kesken"),
                            tr("Tiliöinti on kesken %1 tiliotteessa. Kirjanpitosi ei täsmää ennen "
                               "kuin nämä tositteet on tiliöity loppuun saakka.").arg(tilioimatta_),
                            "ktp:/huomio", "oranssi.png"));
    }

    if( qobject_cast<PilviModel*>( kp()->yhteysModel() ) &&
        kp()->pilvi()->tilitietoPalvelu()) {
        QDateTime uusinta = kp()->pilvi()->tilitietoPalvelu()->seuraavaUusinta();
        if( uusinta.isValid() && uusinta < QDateTime::currentDateTime()) {
            vinkki.append(taulu("varoitus", tr("Pankkiyhteyden valtuutus vanhentunut"),
                                tr("Valtuutus on vanhentunut %1. Tilitapahtumia ei voi hakea ennen valtuutuksen uusimista.").arg(uusinta.toString("dd.MM.yyyy")),
                                "ktp:/maaritys/tilitiedot", "verkossa.png")) ;
        } else if ( uusinta.isValid()) {
            const int jaljella = QDateTime::currentDateTime().daysTo(uusinta);
            if( jaljella < 7) {
                vinkki.append(taulu("info", tr("Pankkiyhteyden valtuutus vanhenemassa"),
                                    tr("Pankkiyhteyden valtuutus vanhenee %1. Uusi valtuutus jatkaaksesi tilitapahtumien hakemista.").arg(uusinta.toString("dd.MM.yyyy")),
                                    "ktp:/maaritys/tilitapahtumat", "verkossa.png")) ;
            } else if ( jaljella < 21 ) {
                vinkki.append(taulu("vinkki", tr("Uusi pankkiyhteyden valtuutus"),
                                    tr("Pankkiyhteyden valtuutus vanhenee %1.").arg(uusinta.toString("dd.MM.yyyy")),
                                    "ktp:/maaritys/tilitapahtumat", "verkossa.png")) ;
            }
        }
    }

    // Mahdollinen varmuuskopio
    SQLiteModel *sqlite = qobject_cast<SQLiteModel*>(kp()->yhteysModel());
    QRegularExpression reku("(\\d{2})(\\d{2})(\\d{2}).kitsas");
    if( sqlite && sqlite->tiedostopolku().contains(reku) ) {
        QRegularExpressionMatch matsi = reku.match(sqlite->tiedostopolku());
        vinkki.append(taulu("varoitus", tr("Varmuuskopio käytössä?"),
                            tr("Tämä tiedosto on todennäköisesti kirjanpitosi varmuuskopio päivämäärällä %1<br>"
                            "Tähän tiedostoon tehdyt muutokset eivät tallennu varsinaiseen kirjanpitoosi.")
                      .arg( QString("<b>%1.%2.20%3</b>" ).arg(matsi.captured(3), matsi.captured(2), matsi.captured(1)) ),
                            "","tiedostoon.png","aloittaminen/tietokone/#varoitus-varmuuskopion-käsittelemisestä"));
    } else if ( sqlite &&  kp()->settings()->value("PilveenSiirretyt").toString().contains( kp()->asetukset()->uid() ) ) {
        vinkki.append(taulu("info", tr("Paikallinen kirjanpito käytössä"),
                            tr("Käytössäsi on omalle tietokoneellesi tallennettu kirjanpito. Muutokset eivät tallennu Kitsaan pilveen.</p>"
                                "<p>Pilvessä olevan kirjanpitosi voit avata Pilvi-välilehdeltä."),
                            "", "computer-laptop.png")) ;
    }

    if( qobject_cast<PilviModel*>(kp()->yhteysModel()) && !kp()->pilvi()->pilvi().vat() && kp()->asetukset()->onko(AsetusModel::AlvVelvollinen)) {
        vinkki.append(taulu("varoitus", tr("Tilaus on tarkoitettu arvonlisäverottomaan toimintaan."),
                            tr("Pilvikirjanpidon omistajalla on tilaus, jota ei ole tarkoitettu arvonlisäverolliseen toimintaan. "
                               "Arvonlisäilmoitukseen liittyviä toimintoja ei siksi ole käytössä tälle kirjanpidolle."),
                            "", "euromerkki.png"));

    }

    if( TilikarttaPaivitys::onkoPaivitettavaa() && kp()->yhteysModel()->onkoOikeutta(YhteysModel::ASETUKSET) )
    {
        vinkki.append(taulu("info", tr("Päivitä tilikartta"),
                            tr("Tilikartasta saatavilla uudempi versio %1.")
                            .arg(TilikarttaPaivitys::paivitysPvm().toString("dd.MM.yyyy")),
                            "ktp:/maaritys/paivita", "paivita.png","asetukset/paivitys/"));

    }

    // Ensin tietokannan alkutoimiin
    if( saldot_.count() < 3)
    {
        QString t("<ol><li> <a href=ktp:/maaritys/perus>" + tr("Tarkista perusvalinnat ja arvonlisäverovelvollisuus") + "</a> <a href='ohje:/asetukset/perusvalinnat'>(" + tr("Ohje") +")</a></li>");
        t.append("<li> <a href=ktp:/maaritys/yhteys>" + tr("Tarkista yhteystiedot ja logo") + "</a> <a href='ohje:/asetukset/yhteystiedot'>(" + tr("Ohje") +")</a></li>");
        t.append("<li> <a href=ktp:/maaritys/tilit>" + tr("Tutustu tilikarttaan ja tee tarpeelliset muutokset") +  "</a> <a href='ohje:/asetukset/tililuettelo/'>(" + tr("Ohje") + ")</a></li>");
        t.append("<li> <a href=ktp:/maaritys/kohdennukset>" + tr("Lisää tarvitsemasi kohdennukset") + "</a> <a href='ohje:/asetukset/kohdennukset/'>(" + tr("Ohje") + ")</a></li>");
        if( kp()->asetukset()->luku("Tilinavaus")==2)
            t.append("<li><a href=ktp:/maaritys/tilinavaus>" + tr("Tee tilinavaus") + "</a> <a href='ohje:/asetukset/tilinavaus/'>(Ohje)</a></li>");
        t.append("<li><a href=ktp:/kirjaa>" + tr("Voit aloittaa kirjausten tekemisen") +"</a> <a href='ohje:/kirjaus'>("+ tr("Ohje") + ")</a></li></ol>");

        vinkki.append(taulu("vinkki", tr("Kirjanpidon aloittaminen"),t,
                            "", "info64.png"));

    }
    else if( kp()->asetukset()->luku("Tilinavaus")==2 && kp()->asetukset()->pvm("TilinavausPvm") <= kp()->tilitpaatetty() &&
             kp()->yhteysModel()->onkoOikeutta(YhteysModel::ASETUKSET))
        vinkki.append(taulu("vinkki", tr("Tee tilinavaus"),
                            tr("Syötä viimeisimmältä tilinpäätökseltä tilien "
                               "avaavat saldot %1 järjestelmään.").arg(kp()->asetukset()->pvm("TilinavausPvm").toString("dd.MM.yyyy")),
                            "ktp:/maaritys/tilinavaus", "rahaa.png", "asetukset/tilinavaus"));

    // Muistutus arvonlisäverolaskelmasta
    if(  kp()->asetukset()->onko("AlvVelvollinen") && kp()->yhteysModel()->onkoOikeutta(YhteysModel::ALV_ILMOITUS) )
    {
        QDate kausialkaa = kp()->alvIlmoitukset()->viimeinenIlmoitus().addDays(1);
        QDate laskennallinenalkupaiva = kausialkaa;
        int kausi = kp()->asetukset()->luku("AlvKausi");
        if( kausi == 1)
            laskennallinenalkupaiva = QDate( kausialkaa.year(), kausialkaa.month(), 1);
        else if( kausi == 3) {
            int kk = kausialkaa.month();
            if( kk < 4)
                kk = 1;
            else if( kk < 7)
                kk = 4;
            else if( kk < 10)
                kk = 7;
            else
                kk = 10;
            laskennallinenalkupaiva = QDate( kausialkaa.year(), kk, 1);
        } else if( kausi == 12)
            laskennallinenalkupaiva = QDate( kausialkaa.year(), 1, 1);

        QDate kausipaattyy = laskennallinenalkupaiva.addMonths( kausi ).addDays(-1);
        QDate erapaiva = AlvIlmoitustenModel::erapaiva(kausipaattyy);

        qlonglong paivaaIlmoitukseen = kp()->paivamaara().daysTo( erapaiva );
        if( paivaaIlmoitukseen < 0)
        {
            vinkki.append(taulu("varoitus", tr("Arvonlisäveroilmoitus myöhässä"),
                                tr("Arvonlisäveroilmoitus kaudelta %1 - %2 olisi pitänyt antaa %3 mennessä.")
                                .arg(kausialkaa.toString("dd.MM.yyyy"),kausipaattyy.toString("dd.MM.yyyy"),erapaiva.toString("dd.MM.yyyy")),
                                "ktp:/alvilmoitus", "vero64.png"));
        }
        else if( paivaaIlmoitukseen < 12)
        {
            vinkki.append(taulu("vinkki", tr("Tee arvonlisäverotilitys"),
                                tr("Arvonlisäveroilmoitus kaudelta %1 - %2 on annettava %3 mennessä.")
                                .arg(kausialkaa.toString("dd.MM.yyyy"),kausipaattyy.toString("dd.MM.yyyy"),erapaiva.toString("dd.MM.yyyy")),
                                "ktp:/alvilmoitus", "vero64.png"));
        }
    }


    // Uuden tilikauden aloittaminen
    if( kp()->paivamaara().daysTo(kp()->tilikaudet()->kirjanpitoLoppuu()) < 30 && kp()->yhteysModel()->onkoOikeutta(YhteysModel::ASETUKSET))
    {
        vinkki.append(taulu("vinkki", tr("Aloita uusi tilikausi"),
                            tr("Tilikausi päättyy %1, jonka jälkeiselle ajalle ei voi tehdä kirjauksia ennen kuin uusi tilikausi aloitetaan.</p>"
                               "<p>Voit tehdä kirjauksia myös aiempaan tilikauteen, kunnes se on päätetty.").arg( kp()->tilikaudet()->kirjanpitoLoppuu().toString("dd.MM.yyyy") ),
                            "ktp:/uusitilikausi", "kansiot.png","tilikaudet/uusi/"));

    }

    // Tilinpäätöksen laatiminen
    if( kp()->yhteysModel()->onkoOikeutta(YhteysModel::TILINPAATOS)) {
        for(int i=0; i<kp()->tilikaudet()->rowCount(QModelIndex()); i++)
        {
            Tilikausi kausi = kp()->tilikaudet()->tilikausiIndeksilla(i);
            if( kausi.paattyy().daysTo(kp()->paivamaara()) > 1 &&
                                       kausi.paattyy().daysTo( kp()->paivamaara()) < 5 * 30
                    && ( kausi.tilinpaatoksenTila() == Tilikausi::ALOITTAMATTA || kausi.tilinpaatoksenTila() == Tilikausi::KESKEN) )
            {               
                vinkki.append(taulu("vinkki", tr("Aika laatia tilinpäätös tilikaudelle %1").arg(kausi.kausivaliTekstina()),
                                    kausi.tilinpaatoksenTila() == Tilikausi::ALOITTAMATTA ?
                                         tr("Tee loppuun kaikki tilikaudelle kuuluvat kirjaukset ja laadi sen jälkeen tilinpäätös")
                                       : tr("Viimeistele ja vahvista tilinpäätös"),
                                    "ktp:/tilinpaatos", "kansiot.png","tilikaudet/tilinpaatos/"));
            }
        }
    }


    // Viimeisenä muistiinpanot
    if( kp()->asetukset()->onko("Muistiinpanot") )
    {
        vinkki.append(" <table class=memo width=100%><tr><td><pre>");
        vinkki.append( kp()->asetukset()->asetus(AsetusModel::Muistiinpanot));
        vinkki.append("</pre></td></tr></table");
    }

    return vinkki;
}

QString AloitusSivu::summat()
{
    QString txt;

    Tilikausi tilikausi = kp()->tilikaudet()->tilikausiIndeksilla( ui->tilikausiCombo->currentIndex() );
    txt.append(tr("<p><h2 class=kausi>Tilikausi %1 - %2 </h1>").arg(tilikausi.alkaa().toString("dd.MM.yyyy"))
             .arg(tilikausi.paattyy().toString("dd.MM.yyyy")));


    txt.append("<table width=100% class=saldot>");

    int rivi=0;
    QString edellinen = "0";

    QMapIterator<QString,QVariant> iter(saldot_);
    while( iter.hasNext()) {
        iter.next();
        QString tiliteksti = iter.key();
        int tilinumero = iter.key().toInt();
        double saldo = iter.value().toDouble();

        if( tiliteksti.at(0) == '1' && edellinen.at(0) != '1')
            txt.append("<tr><td colspan=2 class=saldootsikko>" + tr("Vastaavaa") + "</td></tr>");
        if( tiliteksti.at(0) == '2' && edellinen.at(0) != '2')
            txt.append("<tr><td colspan=2 class=saldootsikko>" + tr("Vastattavaa") + "</td></tr>");
        if( tiliteksti.at(0) > '2' && edellinen.at(0) <= '2')
            txt.append("<tr><td colspan=2 class=saldootsikko>" + tr("Tuloslaskelma") + "</td></tr>");
        edellinen = tiliteksti;

        txt.append( QString("<tr class=%4><td><a href=\"selaa:%1\">%1 %2</a></td><td class=euro>%L3 €</td></tr>")
                    .arg(tilinumero)
                    .arg(kp()->tilit()->nimi(tilinumero).toHtmlEscaped())
                    .arg(saldo,0,'f',2)
                    .arg(rivi++ % 4 == 3 ? "tumma" : "vaalea"));

    }
    QString tulostili = QString::number(kp()->tilit()->tiliTyypilla(TiliLaji::KAUDENTULOS).numero()) ;
    double saldo = saldot_.value( tulostili ).toDouble();

    txt.append( QString("<tr class=tulosrivi><td>" + tr("Tilikauden tulos") + "</td><td class=euro>%L1 €</td></tr>")
                .arg(saldo,0,'f',2) );
    txt.append("</table>");

    return txt;

}

void AloitusSivu::naytaTervetuloTiedosto()
{
    QFile tttiedosto(Kielet::instanssi()->uiKieli() == "sv" ? ":/aloitus/svenska.html" : ":/aloitus/tervetuloa.html");
    tttiedosto.open(QIODevice::ReadOnly);
    QTextStream in(&tttiedosto);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    in.setCodec("utf8");
#endif
    QString teksti = in.readAll();
    teksti.replace("<INFO>", paivitysInfo);

    ui->selain->setHtml( teksti );
}


