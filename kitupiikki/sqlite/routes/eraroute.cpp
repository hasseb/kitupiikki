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
#include "eraroute.h"
#include "db/tili.h"
#include "db/kirjanpito.h"

#include <QDate>
#include <QDebug>

EraRoute::EraRoute(SQLiteModel *model) :
    SQLiteRoute(model, "/erat")
{

}

QVariant EraRoute::get(const QString &/*polku*/, const QUrlQuery &urlquery)
{
    if( urlquery.hasQueryItem("erittely"))
        return erittely( QDate::fromString(urlquery.queryItemValue("alkaa"),Qt::ISODate),
                         QDate::fromString(urlquery.queryItemValue("loppuu"),Qt::ISODate) );



    QSqlQuery kysely( db() );
    QVariantList lista;

    QString kysymys("select vienti.eraid as eraid, sum(vienti.debetsnt) as sd, sum(vienti.kreditsnt) as sk, a.selite as selite, a.pvm as pvm, a.tili as tili from  Vienti "
                    "join Vienti as a on vienti.eraid = a.id JOIN Tosite ON Vienti.Tosite=Tosite.id  WHERE Tosite.tila >= 100 ");

    if( urlquery.hasQueryItem("tili"))
        kysymys.append(QString("AND vienti.tili=%1 ").arg(urlquery.queryItemValue("tili")));

    kysymys.append("GROUP BY vienti.eraid, a.selite, a.pvm, a.tili "
                   "HAVING sum(vienti.debetsnt) <> sum(vienti.kreditsnt) OR sum(vienti.debetsnt) IS NULL OR sum(vienti.kreditsnt) IS NULL");

    kysely.exec(kysymys);
    while( kysely.next()) {
        QString tili = kysely.value("tili").toString();
        double debet = kysely.value(1).toLongLong() / 100.0;
        double kredit = kysely.value(2).toLongLong() / 100.0;
        double avoin = tili.startsWith('1') ?
                    debet - kredit :
                    kredit - debet;

        QVariantMap map;
        map.insert("eraid", kysely.value(0).toInt());
        map.insert("tili", tili);
        map.insert("avoin", avoin);
        map.insert("selite", kysely.value("selite"));
        map.insert("pvm", kysely.value("pvm").toDate());
        lista.append(map);
    }
    return lista;
}

QVariant EraRoute::erittely(const QDate &mista, const QDate &pvm)
{
    // Tase-erittelyn muodostaminen
    // Haetaan ensin summat loppupäivälle


    QSqlQuery kysely( db() );
    kysely.exec( QString("SELECT tili, sum(debetsnt), sum(kreditsnt) FROM vienti JOIN Tosite ON Vienti.tosite = Tosite.id "
                         "WHERE Vienti.pvm<='%1' AND Tosite.tila >= 100 "
                         "AND CAST (tili AS text) < 3 GROUP BY tili ORDER BY CAST(tili AS text)")
                 .arg(pvm.toString(Qt::ISODate)));

    // Sitten aletaan käymään näitä lävitse)

    QVariantMap ulos;

    while( kysely.next()) {
        Tili* tili = kp()->tilit()->tili( kysely.value(0).toInt() );
        if( !tili )
            continue;
        int erittelytapa = tili->taseErittelyTapa();

        qlonglong loppusaldo = kysely.value(1).toLongLong() -
                kysely.value(2).toLongLong();
        if( tili->onko(TiliLaji::VASTATTAVAA))
            loppusaldo *= -1;

        if( erittelytapa == Tili::TASEERITTELY_TAYSI) {
            QVariantList erat;

            // Tase-erät
            QSqlQuery erakysely(db());
            erakysely.exec(QString("select vienti.eraid, vienti.debetsnt, vienti.kreditsnt, vienti.selite, Tosite.pvm, Tosite.sarja, Tosite.tunniste, tosite.id "
                                   "FROM Vienti JOIN Tosite ON Vienti.tosite = Tosite.id  WHERE Vienti.tili=%1 AND Vienti.id=Vienti.eraid "
                                   "AND Vienti.pvm <= '%2' AND Tosite.tila >= 100 ORDER BY tosite.pvm")
                           .arg( tili->numero() ).arg(pvm.toString(Qt::ISODate)));

            while( erakysely.next()) {
                int eraid = erakysely.value(0).toInt();
                qlonglong alkusentit = tili->onko(TiliLaji::VASTAAVAA) ?
                            erakysely.value(1).toLongLong() - erakysely.value(2).toLongLong() :
                            erakysely.value(2).toLongLong() - erakysely.value(1).toLongLong() ;

                QSqlQuery apukysely( db() );
                qlonglong alkusaldo = 0l;
                apukysely.exec(QString("SELECT sum(debetsnt), sum(kreditsnt) FROM Viennit JOIN Tosite ON Vienti.tosite=Tosite.id WHERE eraid=%1 AND pvm<%2 AND Tosite.tila >= 100 ")
                               .arg(eraid).arg(mista.toString(Qt::ISODate)));
                if( apukysely.next())
                    alkusaldo = tili->onko(TiliLaji::VASTAAVAA) ?
                                apukysely.value(0).toLongLong() - apukysely.value(1).toLongLong() :
                                apukysely.value(1).toLongLong() - apukysely.value(0).toLongLong() ;

                apukysely.exec(QString("select vienti.debetsnt, vienti.kreditsnt, vienti.selite, Tosite.pvm, Tosite.sarja, Tosite.tunniste, tosite.id "
                                       "FROM Vienti  JOIN Tosite ON Vienti.tosite = Tosite.id  WHERE Vienti.eraid=%1 AND Vienti.id<>Vienti.eraid "
                                       "AND Vienti.pvm BETWEEN '%2' AND '%3' AND Tosite.tila >= 100 ORDER BY tosite.pvm")
                               .arg(eraid)
                               .arg(mista.toString(Qt::ISODate))
                               .arg(pvm.toString(Qt::ISODate)));
                QVariantList muutokset;
                // Jos erä alkaa tältä tilikaudelta, on erän aloitus osa muutosta
                qlonglong muutosyht =  erakysely.value(4).toDate() < mista ? 0 : alkusentit;
                while( apukysely.next() )
                {
                    QVariantMap map;
                    qlonglong summa = tili->onko(TiliLaji::VASTAAVAA) ?
                                apukysely.value(0).toLongLong() - apukysely.value(1).toLongLong() :
                                apukysely.value(1).toLongLong() - apukysely.value(0).toLongLong() ;

                    map.insert("pvm", apukysely.value(3).toDate());
                    map.insert("sarja", apukysely.value(4));
                    map.insert("tunniste", apukysely.value(5));
                    map.insert("id", apukysely.value(6).toInt());
                    map.insert("selite", apukysely.value(2).toDate());
                    map.insert("eur", summa / 100.0);
                    muutosyht += summa;
                    muutokset.append(map);
                }
                if( !muutosyht && !alkusaldo )
                    continue;
                QVariantMap era;
                era.insert("id", erakysely.value(7).toInt());
                era.insert("pvm", erakysely.value(4).toDate());
                era.insert("sarja",erakysely.value(5));
                era.insert("tunniste", erakysely.value(6));
                era.insert("eur", alkusentit / 100.0);

                QVariantMap emap;
                emap.insert("era", era);
                emap.insert("ennen", alkusaldo / 100.0);
                emap.insert("kausi", muutokset);
                emap.insert("saldo", ( alkusaldo + muutosyht) / 100.0);
                erat.append(emap);
            }
            ulos.insert( QString("%1T").arg(tili->numero()), erat);


        } else if( erittelytapa == Tili::TASEERITTELY_LISTA) {
            QSqlQuery apukysely( db() );
            QVariantList erat;

            apukysely.exec(QString("select vienti.eraid, sum(vienti.debetsnt) as sd, sum(vienti.kreditsnt) as sk, a.selite, tosite.pvm, tosite.sarja, tosite.tunniste  from  Vienti "
                                   "join Vienti as a on vienti.eraid = a.id "
                                   "join Tosite on a.tosite=tosite.id "
                                   "WHERE vienti.tili=%1 AND vienti.pvm <= '%2'  AND Tosite.tila >= 100 GROUP BY vienti.eraid, a.selite, a.pvm, a.tili "
                                   "HAVING sum(vienti.debetsnt) <> sum(vienti.kreditsnt) OR sum(vienti.debetsnt) IS NULL OR sum(vienti.kreditsnt) IS NULL;"
                                   ).arg(tili->numero()).arg(pvm.toString(Qt::ISODate)));
            while( apukysely.next()) {
                QVariantMap era;
                era.insert("id", apukysely.value(0).toInt());
                era.insert("pvm", apukysely.value(4).toDate() );
                era.insert("sarja", apukysely.value(5));
                era.insert("tunniste", apukysely.value(6));
                era.insert("selite", apukysely.value(3));
                qlonglong summa = tili->onko(TiliLaji::VASTAAVAA) ?
                            apukysely.value(1).toLongLong() - apukysely.value(2).toLongLong() :
                            apukysely.value(2).toLongLong() - apukysely.value(1).toLongLong() ;
                era.insert("eur", summa / 100.0);
                erat.append(era);
            }
            QVariantMap emap;
            ulos.insert(QString("%1E").arg(tili->numero()), erat);

        } else if( erittelytapa == Tili::TASEERITTELY_MUUTOKSET) {

            // TODO: Edellisten tilikausien voitto/tappio: Edellisen voiton/tappion lisääminen jos halutaan (KPA ei vaadi ;)

            QSqlQuery apukysely( db());
            apukysely.exec(QString("select vienti.debetsnt, vienti.kreditsnt, vienti.selite, Tosite.pvm, Tosite.sarja, Tosite.tunniste "
                                   "FROM Vienti JOIN Tosite ON Vienti.tosite = Tosite.id  WHERE Vienti.tili=%1 "
                                   "AND Vienti.pvm BETWEEN '%2' AND '%3' AND Tosite.tila >= 100 ORDER BY tosite.pvm")
                           .arg(tili->numero())
                           .arg(mista.toString(Qt::ISODate))
                           .arg(pvm.toString(Qt::ISODate)));

            QVariantList muutokset;
            qlonglong muutosyht = 0;
            while( apukysely.next() )
            {
                QVariantMap map;
                qlonglong summa = tili->onko(TiliLaji::VASTAAVAA) ?
                            apukysely.value(0).toLongLong() - apukysely.value(1).toLongLong() :
                            apukysely.value(1).toLongLong() - apukysely.value(0).toLongLong() ;

                map.insert("pvm", apukysely.value(3).toDate());
                map.insert("sarja", apukysely.value(4));
                map.insert("tunniste", apukysely.value(5));
                map.insert("selite", apukysely.value(2).toDate());
                map.insert("eur", summa / 100.0);
                muutosyht += summa;
                muutokset.append(map);
            }
            QVariantMap map;
            map.insert("saldo", loppusaldo / 100.0);
            map.insert("kausi", muutokset);
            map.insert("ennnen", (loppusaldo - muutosyht) / 100.0);
            ulos.insert(QString("%1M").arg(tili->numero()), map);

        } else {
            // Tilisaldot
            ulos.insert( QString("%1S").arg(tili->numero()), loppusaldo / 100.0 );
        }

    }
    return ulos;
}