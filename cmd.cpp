#include "cmd.h"

Cmd::Cmd()
{

}
// Util function
QString Cmd::getCmdOut(const QString &cmd) {
    proc = new QProcess();
    proc->start("/bin/bash", QStringList() << "-c" << cmd);
    proc->setReadChannel(QProcess::StandardOutput);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->waitForFinished(-1);
    auto result = proc->readAllStandardOutput().trimmed();
    delete proc;
    return result;
}
