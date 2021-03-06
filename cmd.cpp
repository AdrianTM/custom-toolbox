#include <QDebug>
#include <QEventLoop>

#include "cmd.h"

Cmd::Cmd(QObject *parent)
    : QProcess(parent)
{
}

void Cmd::halt()
{
    if (state() != QProcess::NotRunning) {
        terminate();
        waitForFinished(5000);
        kill();
        waitForFinished(1000);
    }
}

bool Cmd::run(const QString &cmd, bool quiet)
{
    QString output;
    return run(cmd, output, quiet);
}

// util function for getting bash command output
QString Cmd::getCmdOut(const QString &cmd, bool quiet)
{
    QString output;
    run(cmd, output, quiet);
    return output;
}

bool Cmd::run(const QString &cmd, QString &output, bool quiet)
{
    if (this->state() != QProcess::NotRunning) {
        qDebug() << "Process already running:" << this->program() << this->arguments();
        return false;
    }
    if (!quiet) qDebug().noquote() << cmd;
    QEventLoop loop;
    connect(this, QOverload<int>::of(&QProcess::finished), &loop, &QEventLoop::quit);
    start("/bin/bash", QStringList() << "-c" << cmd);
    loop.exec();
    disconnect(this, QOverload<int>::of(&QProcess::finished), nullptr, nullptr);
    output = readAll().trimmed();
    return (exitStatus() == QProcess::NormalExit && exitCode() == 0);
}

