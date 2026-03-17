#ifndef INTERNALPARAMMASTER_H
#define INTERNALPARAMMASTER_H

#include "communication/modbusconnection.h"
#include "models/connectiontypes.h"

#include <QModbusDevice>
#include <QTimer>

/* Forward declaration */
class SettingsModel;

/*!
 * \brief InternalParamMaster handles the multi-step internal-parameter
 *        read/write transaction via Modbus RTU (holding registers 98-102).
 *
 * Register map (0-based):
 *   98  – action register  (write 1 = read-param, 3 = write-param)
 *   100 – parameter-address register
 *   101 – data register #1 (first / only 16-bit word)
 *   102 – data register #2 (second word for 32-bit values)
 *
 * Read sequence  : write 100 → write 98=1 → wait 200ms → FC03 read 101 (qty 1/2)
 * Write sequence : write 100 → write 101 → [write 102] → write 98=3 → wait 200ms → FC03 read 101 (qty 1/2)
 */
class InternalParamMaster : public QObject
{
    Q_OBJECT

public:
    explicit InternalParamMaster(ModbusConnection* pConn,
                                 SettingsModel* pSettings,
                                 ConnectionTypes::connectionId_t connId,
                                 QObject* parent = nullptr);

    void readParam(quint8 slaveId, quint16 paramAddr, bool is32Bit);
    void writeParam(quint8 slaveId, quint16 paramAddr, quint16 word1, quint16 word2, bool is32Bit);

    bool isBusy() const;

signals:
    void readDone(bool success, QString errorMsg, quint16 word1, quint16 word2);
    void writeDone(bool success, QString errorMsg, quint16 word1, quint16 word2);

private slots:
    void handleConnectionOpened();
    void handleConnectionError(QModbusDevice::Error error, QString msg);

    void handleWriteSuccess();
    void handleWriteProtocolError(QModbusPdu::ExceptionCode exceptionCode);
    void handleWriteError(QString errorString, QModbusDevice::Error error);

    void handleReadSuccess(ModbusDataUnit const& startRegister, QList<quint16> registerDataList);
    void handleReadProtocolError(QModbusPdu::ExceptionCode exceptionCode);
    void handleReadError(QString errorString, QModbusDevice::Error error);

    void handleWaitTimeout();

private:
    enum class State
    {
        IDLE,
        WRITE_PARAM_ADDR,
        WRITE_DATA1,
        WRITE_DATA2,
        WRITE_ACTION,
        WAITING,
        READ_DATA
    };

    enum class OpType
    {
        READ_PARAM,
        WRITE_PARAM
    };

    void openConnection();
    void doNextWrite();
    void handleError(const QString& msg);
    void finishWithData(const QList<quint16>& data);
    ModbusDataUnit makeReg(quint16 regAddr) const;

    std::unique_ptr<ModbusConnection> _pConn;
    SettingsModel* _pSettings{};
    ConnectionTypes::connectionId_t _connId{};

    State _state{State::IDLE};
    OpType _opType{OpType::READ_PARAM};

    quint8 _slaveId{1};
    quint16 _paramAddr{0};
    quint16 _word1{0};
    quint16 _word2{0};
    bool _is32Bit{false};

    QTimer _waitTimer;

    /* Register map (0-based Modbus protocol addresses).
     * Note: gaps between addresses are intentional – the device uses other
     * registers in the 98–102 range for unrelated purposes. */
    static constexpr quint16 REG_ACTION = 98;     /*!< Write 1=read, 3=write  */
    static constexpr quint16 REG_PARAM_ADDR = 100; /*!< 16-bit parameter address */
    static constexpr quint16 REG_DATA1 = 101;      /*!< First / only 16-bit data word */
    static constexpr quint16 REG_DATA2 = 102;      /*!< Second data word (32-bit only) */
    static constexpr int WAIT_MS = 200;
    static constexpr quint8 ACTION_READ = 1;
    static constexpr quint8 ACTION_WRITE = 3;
};

#endif // INTERNALPARAMMASTER_H
