#include "file_association.hpp"

#ifdef Q_OS_WIN
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QDebug>

namespace fap {

void registerFileAssociation() {
    QString exePath = QCoreApplication::applicationFilePath();
    QString exeNative = QDir::toNativeSeparators(exePath);

    QSettings regExt(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\.fap"),
                     QSettings::NativeFormat);
    regExt.setValue("Default", "FAP.Document");

    QSettings regClass(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\FAP.Document"),
                       QSettings::NativeFormat);
    regClass.setValue("Default", "Free Animation Power Project");

    QSettings regIcon(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\FAP.Document\\DefaultIcon"),
                      QSettings::NativeFormat);
    regIcon.setValue("Default", exeNative + ",0");

    QSettings regOpen(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\FAP.Document\\shell\\open\\command"),
                      QSettings::NativeFormat);
    regOpen.setValue("Default", QStringLiteral("\"%1\" \"%2\"").arg(exeNative, "%1"));

    qDebug() << "File association registered for .fap:" << exeNative;
}

} // namespace fap
#else

namespace fap {
void registerFileAssociation() {}
} // namespace fap

#endif
