#include "groupdata.h"

#include "db/kirjanpito.h"
#include "pilvi/pilvimodel.h"

GroupData::GroupData(QObject *parent)
    : QObject{parent},
      books_{new GroupBooksModel(this)},
      members_{new GroupMembersModel(this)}
{

}

void GroupData::load(const int groupId)
{
    KpKysely* kysymys = kp()->pilvi()->loginKysely(
        QString("/groups/%1").arg(groupId)
    );
    if( kysymys ) {
        connect(kysymys, &KpKysely::vastaus, this, &GroupData::dataIn);
        kysymys->kysy();
    }
}

void GroupData::addBook(const QVariant &velhoMap)
{
    KpKysely *kysymys = kp()->pilvi()->loginKysely(
        QString("/groups/%1/book").arg(id()),
        KpKysely::POST
    );
    connect( kysymys, &KpKysely::vastaus, this, [this]
        { this->reload(); kp()->pilvi()->paivitaLista(); });

    kysymys->kysy(velhoMap);
}

void GroupData::dataIn(QVariant *data)
{
    const QVariantMap map = data->toMap();

    const QVariantMap groupMap = map.value("group").toMap();
    id_ = groupMap.value("id").toInt();
    name_ = groupMap.value("name").toString();
    businessId_ = groupMap.value("businessid").toString();

    const QString typeString = groupMap.value("type").toString();
    if( typeString == "UNIT")
        type_ = GroupNode::UNIT;
    else if( typeString == "GROUP")
        type_ = GroupNode::GROUP;
    else
        type_ = GroupNode::OFFICE;

    admin_ = map.value("admin").toStringList();
    books_->load(map.value("books").toList());
    members_->load(map.value("members").toList());

    const QVariantMap officeMap = groupMap.value("office").toMap();
    officeName_ = officeMap.value("name").toString();
    officeType_ = officeMap.value("type").toString();

    officeTypes_ = map.value("officetypes").toList();
    products_ = map.value("products").toList();

    emit loaded();
}

void GroupData::reload()
{
    if( id() ) {
        load( id());
    }
}