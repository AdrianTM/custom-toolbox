#ifndef CMD_H
#define CMD_H

#include <QProcess>
#include <QString>


class Cmd
{
public:
    Cmd();
    QString getCmdOut(const QString &cmd);
    QProcess *proc;
};

#endif // CMD_H
