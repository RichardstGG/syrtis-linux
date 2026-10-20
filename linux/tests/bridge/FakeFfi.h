#pragma once

#include <QByteArray>
#include <QList>
#include <QSemaphore>

namespace FakeFfi {

struct Call {
    QByteArray name;
    QList<QByteArray> arguments;
    bool onMainThread;
};

struct Stats {
    QList<Call> calls;
    int allocated = 0;
    int freed = 0;
    int invalidFrees = 0;
    bool freedOnMainThread = false;
    bool gateTimedOut = false;
};

void reset();
void setReply(QByteArray json); // A null QByteArray simulates a null C pointer.
void block(QByteArray operation);
Stats stats();
extern QSemaphore entered;
extern QSemaphore released;

} // namespace FakeFfi
