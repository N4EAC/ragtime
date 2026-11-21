// Configuration discovery must not override Ragtime's explicit modem arguments.
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include "Check.h"
#include "app/settings.hpp"
#include "app/station.hpp"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QString beside = QCoreApplication::applicationDirPath() + "/data2g.conf";
    if (QFile::exists(beside)) return 77; // Never replace a developer's configuration.
    {
        QSettings config(beside, QSettings::IniFormat);
        config.setValue("mycall", "NEVERLOAD");
        config.setValue("command_port", 45001);
        config.sync();
    }
    auto selected = data2g::app::open_config(1, argv);
    CHECK(QFileInfo(selected->fileName()).absoluteFilePath() == QFileInfo(beside).absoluteFilePath());
    CHECK(selected->value("mycall").toString() == "NEVERLOAD");
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    const auto explicitPath = (temporary.path() + "/explicit.conf").toLocal8Bit();
    char option[] = "--config";
    char* explicitArgs[] = {argv[0], option, const_cast<char*>(explicitPath.constData())};
    selected = data2g::app::open_config(3, explicitArgs);
    CHECK(selected->fileName() == QString::fromLocal8Bit(explicitPath));
    data2g::app::Args args;
    args.mycall = "W1AW";
    args.command_port = 45002;
    data2g::app::Station modem(args); // No audio, sockets, engine threads or RF are started.
    CHECK(modem.args().mycall == "W1AW");
    CHECK(modem.args().command_port == 45002);
    selected.reset();
    CHECK(QFile::remove(beside));
    return ragtime_test::finish();
}
