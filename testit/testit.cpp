#include <QTest>
#include <QApplication>

#include <map>
#include <memory>

#include "tuontitesti.h"
#include "tilitesti.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStringList arguments = QCoreApplication::arguments();

    std::map<QString, std::unique_ptr<QObject>> tests;

    // Tässä kaikki suoritettavat testit!
    tests.emplace("tuonti", std::make_unique<TuontiTesti>());
    tests.emplace("tili", std::make_unique<TiliTesti>());


    // Mahdollisuus testin valintaan
    if( arguments.size() > 3 && arguments[1] == "-select") {

        QString testname = arguments.at(2);
        auto iter = tests.begin();
        while( iter != tests.end()) {
            if( iter->first != testname) {
                iter = tests.erase(iter);
            } else {
                ++iter;
            }
        }
        arguments.removeOne("-select");
        arguments.removeOne(testname);
    }

  int status = 0;
  for( auto& test : tests) {
      status |= QTest::qExec( test.second.get(), arguments );
  }

  return status;
}