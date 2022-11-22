#include "avattupilvi.h"
#include "db/yhteysmodel.h"

#include <QDebug>

AvattuPilvi::AvattuPilvi()
{

}

AvattuPilvi::AvattuPilvi(const QVariant &data)
{
    const QVariantMap map = data.toMap();

    id_ =  map.value("id").toInt();
    oikeudet_ = oikeudetListasta(map.value("rights").toList());
    url_ = map.value("url").toString();
    token_ = map.value("token").toString();
    services_= map.value("services").toMap();

    const QVariantMap planMap = map.value("plan").toMap();
    plan_id_ = planMap.value("id").toInt();
    vat_ = planMap.value("vat").toBool();
    trial_period_ = planMap.value("trial").toBool();


}

AvattuPilvi::operator bool() const
{
    return id() != 0;
}

QString AvattuPilvi::service(const QString &serviceName) const
{
    return services_.value(serviceName).toString();
}


qlonglong AvattuPilvi::oikeudetListasta(const QVariantList &lista)
{
    qlonglong bittikartta = 0;
    for(const auto& oikeus : lista) {
        try {
            bittikartta |= oikeustunnukset__.at(oikeus.toString());
        } catch( std::out_of_range )
        {
            qDebug() << "Tuntematon oikeus " << oikeus.toString();
        }
    }
    return bittikartta;
}


std::map<QString,qlonglong> AvattuPilvi::oikeustunnukset__ = {
    {"Ts", YhteysModel::TOSITE_SELAUS},
    {"Tl", YhteysModel::TOSITE_LUONNOS},
    {"Tt", YhteysModel::TOSITE_MUOKKAUS},
    {"Ls", YhteysModel::LASKU_SELAUS},
    {"Ll", YhteysModel::LASKU_LAATIMINEN},
    {"Lt", YhteysModel::LASKU_LAHETTAMINEN},
    {"Kl", YhteysModel::KIERTO_LISAAMINEN},
    {"Kt", YhteysModel::KIERTO_TARKASTAMINEN},
    {"Kh", YhteysModel::KIERTO_HYVAKSYMINEN},
    {"Av", YhteysModel::ALV_ILMOITUS},
    {"Bm", YhteysModel::BUDJETTI},
    {"Tp", YhteysModel::TILINPAATOS},
    {"As", YhteysModel::ASETUKSET},
    {"Ko", YhteysModel::KAYTTOOIKEUDET},
    {"Om", YhteysModel::OMISTAJA},
    {"Xt", YhteysModel::TUOTTEET},
    {"Xr", YhteysModel::RYHMAT},
    {"Ra", YhteysModel::RAPORTIT},
    {"Ks", YhteysModel::KIERTO_SELAAMINEN},
    {"Tk", YhteysModel::TOSITE_KOMMENTTI}
};