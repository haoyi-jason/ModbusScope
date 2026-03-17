#include "internalparammaster.h"

#include "models/settingsmodel.h"
#include "util/scopelogging.h"

InternalParamMaster::InternalParamMaster(ModbusConnection* pConn,
                                         SettingsModel* pSettings,
                                         ConnectionTypes::connectionId_t connId,
                                         QObject* parent)
    : QObject(parent), _pConn(pConn), _pSettings(pSettings), _connId(connId)
{
    _waitTimer.setSingleShot(true);
    connect(&_waitTimer, &QTimer::timeout, this, &InternalParamMaster::handleWaitTimeout);

    connect(_pConn.get(), &ModbusConnection::connectionSuccess,
            this, &InternalParamMaster::handleConnectionOpened);
    connect(_pConn.get(), &ModbusConnection::connectionError,
            this, &InternalParamMaster::handleConnectionError);

    connect(_pConn.get(), &ModbusConnection::writeRequestSuccess,
            this, &InternalParamMaster::handleWriteSuccess);
    connect(_pConn.get(), &ModbusConnection::writeRequestProtocolError,
            this, &InternalParamMaster::handleWriteProtocolError);
    connect(_pConn.get(), &ModbusConnection::writeRequestError,
            this, &InternalParamMaster::handleWriteError);

    connect(_pConn.get(), &ModbusConnection::readRequestSuccess,
            this, &InternalParamMaster::handleReadSuccess);
    connect(_pConn.get(), &ModbusConnection::readRequestProtocolError,
            this, &InternalParamMaster::handleReadProtocolError);
    connect(_pConn.get(), &ModbusConnection::readRequestError,
            this, &InternalParamMaster::handleReadError);
}

bool InternalParamMaster::isBusy() const
{
    return _state != State::IDLE;
}

void InternalParamMaster::readParam(quint8 slaveId, quint16 paramAddr, bool is32Bit)
{
    if (isBusy())
    {
        qCWarning(scopeCommConnection) << "[InternalParam] Busy - ignoring readParam request";
        return;
    }

    _opType = OpType::READ_PARAM;
    _slaveId = slaveId;
    _paramAddr = paramAddr;
    _is32Bit = is32Bit;
    _word1 = 0;
    _word2 = 0;
    _state = State::WRITE_PARAM_ADDR;

    openConnection();
}

void InternalParamMaster::writeParam(quint8 slaveId, quint16 paramAddr,
                                     quint16 word1, quint16 word2, bool is32Bit)
{
    if (isBusy())
    {
        qCWarning(scopeCommConnection) << "[InternalParam] Busy - ignoring writeParam request";
        return;
    }

    _opType = OpType::WRITE_PARAM;
    _slaveId = slaveId;
    _paramAddr = paramAddr;
    _word1 = word1;
    _word2 = word2;
    _is32Bit = is32Bit;
    _state = State::WRITE_PARAM_ADDR;

    openConnection();
}

void InternalParamMaster::openConnection()
{
    /* Configure the underlying connection based on the settings.
     * Note: ModbusPoll enforces a TYPE_SERIAL-only constraint before calling
     * readParam/writeParam, but InternalParamMaster itself does not restrict
     * to serial – the protocol can work over TCP as well. */
    auto connData = _pSettings->connectionSettings(_connId);

    if (connData->connectionType() == ConnectionTypes::TYPE_SERIAL)
    {
        ModbusConnection::serialSettings_t serialSettings = {
            .portName = connData->portName(),
            .parity = connData->parity(),
            .baudrate = connData->baudrate(),
            .databits = connData->databits(),
            .stopbits = connData->stopbits(),
        };
        _pConn->configureSerialConnection(serialSettings);
    }
    else
    {
        ModbusConnection::tcpSettings_t tcpSettings = {
            .ip = connData->ipAddress(),
            .port = connData->port(),
        };
        _pConn->configureTcpConnection(tcpSettings);
    }

    _pConn->open(connData->timeout());
}

void InternalParamMaster::handleConnectionOpened()
{
    if (_state == State::WRITE_PARAM_ADDR)
    {
        doNextWrite();
    }
}

void InternalParamMaster::handleConnectionError(QModbusDevice::Error /*error*/, QString msg)
{
    handleError(QString("Connection error: %1").arg(msg));
}

// Called after each successful FC06 write; advances state and issues next write or starts wait/read
void InternalParamMaster::handleWriteSuccess()
{
    doNextWrite();
}

void InternalParamMaster::handleWriteProtocolError(QModbusPdu::ExceptionCode exceptionCode)
{
    handleError(QString("Write protocol error: %1").arg(static_cast<int>(exceptionCode)));
}

void InternalParamMaster::handleWriteError(QString errorString, QModbusDevice::Error /*error*/)
{
    handleError(QString("Write error: %1").arg(errorString));
}

void InternalParamMaster::handleReadSuccess(ModbusDataUnit const& /*startRegister*/,
                                             QList<quint16> registerDataList)
{
    if (_state != State::READ_DATA)
    {
        return;
    }

    _pConn->close();
    _state = State::IDLE;

    finishWithData(registerDataList);
}

void InternalParamMaster::handleReadProtocolError(QModbusPdu::ExceptionCode exceptionCode)
{
    handleError(QString("Read protocol error: %1").arg(static_cast<int>(exceptionCode)));
}

void InternalParamMaster::handleReadError(QString errorString, QModbusDevice::Error /*error*/)
{
    handleError(QString("Read error: %1").arg(errorString));
}

void InternalParamMaster::handleWaitTimeout()
{
    // After 200ms delay, issue the FC03 read
    _state = State::READ_DATA;
    const quint16 qty = _is32Bit ? 2u : 1u;
    _pConn->sendReadRequest(makeReg(REG_DATA1), qty);
}

void InternalParamMaster::doNextWrite()
{
    switch (_state)
    {
    case State::WRITE_PARAM_ADDR:
        // First step for both read and write: write paramAddr to reg100
        _pConn->sendWriteRequest(makeReg(REG_PARAM_ADDR), _paramAddr);
        if (_opType == OpType::READ_PARAM)
        {
            _state = State::WRITE_ACTION;
        }
        else
        {
            _state = State::WRITE_DATA1;
        }
        break;

    case State::WRITE_DATA1:
        // Write-param only: write word1 to reg101
        _pConn->sendWriteRequest(makeReg(REG_DATA1), _word1);
        _state = _is32Bit ? State::WRITE_DATA2 : State::WRITE_ACTION;
        break;

    case State::WRITE_DATA2:
        // Write-param 32-bit only: write word2 to reg102
        _pConn->sendWriteRequest(makeReg(REG_DATA2), _word2);
        _state = State::WRITE_ACTION;
        break;

    case State::WRITE_ACTION:
    {
        // Final write: trigger action (1=read, 3=write)
        const quint16 actionVal = (_opType == OpType::READ_PARAM) ? ACTION_READ : ACTION_WRITE;
        _pConn->sendWriteRequest(makeReg(REG_ACTION), actionVal);
        _state = State::WAITING;
        break;
    }

    case State::WAITING:
        // Action write completed – start 200ms delay then read back data
        _waitTimer.start(WAIT_MS);
        break;

    default:
        break;
    }
}

void InternalParamMaster::handleError(const QString& msg)
{
    qCWarning(scopeCommConnection) << "[InternalParam]" << msg;

    _waitTimer.stop();
    _pConn->close();

    const OpType op = _opType;
    _state = State::IDLE;

    if (op == OpType::READ_PARAM)
    {
        emit readDone(false, msg, 0, 0);
    }
    else
    {
        emit writeDone(false, msg, 0, 0);
    }
}

void InternalParamMaster::finishWithData(const QList<quint16>& data)
{
    const quint16 w1 = data.value(0, 0);
    const quint16 w2 = data.value(1, 0);

    if (_opType == OpType::READ_PARAM)
    {
        emit readDone(true, QString(), w1, w2);
    }
    else
    {
        emit writeDone(true, QString(), w1, w2);
    }
}

ModbusDataUnit InternalParamMaster::makeReg(quint16 regAddr) const
{
    return ModbusDataUnit(regAddr, ModbusAddress::ObjectType::HOLDING_REGISTER, _slaveId);
}
