#include "mainwindow.h"
#include <QApplication>
#include <QSettings>
#include <QFileInfo>
#include <QDebug>

// Подключаем windows.h только если компилируем под Windows
#ifdef Q_OS_WIN
#include <windows.h>
#include <iostream>
#endif

void setupConsole() {
#ifdef Q_OS_WIN
    // Выделяем консоль для этого процесса
    if (AllocConsole()) {
        // Перенаправляем стандартные потоки вывода в эту консоль
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);

        // Магия для того, чтобы cout и qDebug работали корректно
        std::ios::sync_with_stdio(true);
    }
#endif
    // На Linux консоль и так есть, если запускать из терминала.
    // Если запускать по иконке - логи уходят в системный журнал.
}

void initSettings(const QString &iniPath) {
    // Проверяем, существует ли файл
    if (!QFileInfo::exists(iniPath)) {
        // Если нет - создаем дефолтный
        QSettings settings(iniPath, QSettings::IniFormat);
        settings.beginGroup("Debug");
        settings.setValue("ShowConsole", false); // По умолчанию выключено
        settings.endGroup();
        settings.sync(); // Принудительная запись на диск
        qDebug() << "Settings: Created default config file at" << iniPath;
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. Определяем путь к конфигу (рядом с exe файлом)
    QString iniPath = QCoreApplication::applicationDirPath() + "/config.ini";

    // 2. Создаем файл, если его нет
    initSettings(iniPath);

    // 3. Читаем настройки
    QSettings settings(iniPath, QSettings::IniFormat);
    bool showConsole = settings.value("Debug/ShowConsole", false).toBool();

    // 4. Если включено - открываем черное окно (Только Windows)
    if (showConsole) {
        setupConsole();
        qDebug() << "System: Console attached. Logging enabled.";
    }

    qDebug() << "System: Loading configuration from" << iniPath;

    MainWindow w;
    w.show();
    return a.exec();
}
