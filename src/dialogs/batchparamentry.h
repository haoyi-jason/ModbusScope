#ifndef BATCHPARAMENTRY_H
#define BATCHPARAMENTRY_H

#include <QString>
#include <QtGlobal>

struct BatchParamEntry
{
    QString name;
    quint16 address{0};
    bool is32Bit{false};
    quint32 value{0};      // raw value (word1 | (word2 << 16) for 32-bit)
    bool hasValue{false};  // true if a value was parsed/read
};

#endif // BATCHPARAMENTRY_H
