#ifndef MODBUSMASTER_H
#define MODBUSMASTER_H

#include <QMap>
#include <QModbusDevice>
#include <QModbusReply>

#include "communication/modbusconnection.h"
#include "communication/modbusresultmap.h"
#include "communication/readregisters.h"
#include "models/connectiontypes.h"

/* Forward declaration */
class SettingsModel;

class ModbusMaster : public QObject
{
    Q_OBJECT
public:
    explicit ModbusMaster(ModbusConnection* pModbusConnection,
                          SettingsModel* pSettingsModel,
                          ConnectionTypes::connectionId_t connectionId);
    virtual ~ModbusMaster();

    void readRegisterList(QList<ModbusDataUnit> registerList, quint8 consecutiveMax);
    void writeRegister(ModbusDataUnit regAddr, quint16 value);

    void cleanUp();

signals:
    void modbusPollDone(ModbusResultMap modbusResults, ConnectionTypes::connectionId_t connectionId);
    void modbusWriteDone(bool success, QString errorMessage, ConnectionTypes::connectionId_t connectionId);
    void triggerNextRequest();

private slots:
    void handleConnectionOpened();
    void handlerConnectionError(QModbusDevice::Error error, QString msg);

    void handleRequestSuccess(ModbusDataUnit const& startRegister, QList<quint16> registerDataList);
    void handleRequestProtocolError(QModbusPdu::ExceptionCode exceptionCode);
    void handleRequestError(QString errorString, QModbusDevice::Error error);

    void handleWriteRequestSuccess();
    void handleWriteRequestProtocolError(QModbusPdu::ExceptionCode exceptionCode);
    void handleWriteRequestError(QString errorString, QModbusDevice::Error error);

    void handleTriggerNextRequest(void);

private:
    void finishRead(bool bError);
    QString dumpToString(ModbusResultMap map) const;
    QString dumpToString(QList<ModbusDataUnit> list) const;

    void logResults(const ModbusResultMap &results);

    void logDebug(const QString& msg);
    void logError(const QString& msg);

    ConnectionTypes::connectionId_t _connectionId{};

    std::unique_ptr<ModbusConnection> _pModbusConnection;
    SettingsModel* _pSettingsModel{};
    ReadRegisters _readRegisters{};

    bool _bWritePending{false};
    quint16 _writeValue{};
    ModbusDataUnit _writeRegister{};
};

#endif // MODBUSMASTER_H
