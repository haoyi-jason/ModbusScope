#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include "communication/modbusregister.h"
#include "communication/modbusresultmap.h"
#include "models/connectiontypes.h"
#include <QStringListModel>
#include <QTimer>

//Forward declaration
class SettingsModel;
class RegisterValueHandler;
class ModbusMaster;
class InternalParamMaster;

class ModbusMasterData : public QObject
{
    Q_OBJECT
public:

    explicit ModbusMasterData(ModbusMaster * pArgModbusMaster, QObject *parent = nullptr):
        QObject(parent)
    {
        pModbusMaster = pArgModbusMaster;
        bActive = false;
    }

    ModbusMaster * pModbusMaster;
    bool bActive;
};

class ModbusPoll : public QObject
{
    Q_OBJECT
public:
    explicit ModbusPoll(SettingsModel * pSettingsModel, QObject *parent = nullptr);
    ~ModbusPoll();

    void startCommunication(QList<ModbusRegister>& registerList);
    void stopCommunication();

    bool isActive();
    void resetCommunicationStats();

    void writeRegister(ConnectionTypes::connectionId_t connId, quint16 address, quint8 slaveId, quint16 value);

    void readInternalParam(ConnectionTypes::connectionId_t connId, quint8 slaveId,
                           quint16 paramAddr, bool is32Bit);
    void writeInternalParam(ConnectionTypes::connectionId_t connId, quint8 slaveId,
                            quint16 paramAddr, quint16 word1, quint16 word2, bool is32Bit);

signals:
    void registerDataReady(ResultDoubleList registers);
    void writeRegisterDone(bool success, QString errorMessage);

    void internalParamReadDone(bool success, QString errorMsg, quint16 word1, quint16 word2);
    void internalParamWriteDone(bool success, QString errorMsg, quint16 word1, quint16 word2);

private slots:
    void handlePollDone(ModbusResultMap partialResultMap, ConnectionTypes::connectionId_t connectionId);
    void triggerRegisterRead();
    void handleWriteDone(bool success, QString errorMessage, ConnectionTypes::connectionId_t connectionId);

private:
    quint8 lowestConsecutiveMaxForConnection(ConnectionTypes::connectionId_t connId) const;

    QList<ModbusMasterData*> _modbusMasters;
    QList<InternalParamMaster*> _internalParamMasters;
    quint32 _activeMastersCount;

    bool _bPollActive;
    QTimer * _pPollTimer;
    qint64 _lastPollStart;

    RegisterValueHandler* _pRegisterValueHandler;

    SettingsModel * _pSettingsModel;
};

#endif // COMMUNICATION_MANAGER_H
