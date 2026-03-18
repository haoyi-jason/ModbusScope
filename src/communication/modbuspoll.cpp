
#include "communication/modbuspoll.h"

#include <QDateTime>

#include "communication/internalparammaster.h"
#include "communication/modbusmaster.h"
#include "communication/registervaluehandler.h"
#include "models/settingsmodel.h"
#include "util/formatdatetime.h"
#include "util/scopelogging.h"

using connectionId_t = ConnectionTypes::connectionId_t;

ModbusPoll::ModbusPoll(SettingsModel * pSettingsModel, QObject *parent) :
    QObject(parent), _bPollActive(false)
{

    _pPollTimer = new QTimer();
    _pSettingsModel = pSettingsModel;

    _pRegisterValueHandler = new RegisterValueHandler(_pSettingsModel);
    connect(_pRegisterValueHandler, &RegisterValueHandler::registerDataReady, this, &ModbusPoll::registerDataReady);

    /* Setup modbus master */
    for (quint8 i = 0u; i < ConnectionTypes::ID_CNT; i++)
    {
        auto conn = new ModbusConnection(); // ModbusMaster takes ownership
        auto master = new ModbusMaster(conn, pSettingsModel, i);
        auto modbusData = new ModbusMasterData(master);
        _modbusMasters.append(modbusData);

        connect(_modbusMasters.last()->pModbusMaster, &ModbusMaster::modbusPollDone, this, &ModbusPoll::handlePollDone);
        connect(_modbusMasters.last()->pModbusMaster, &ModbusMaster::modbusWriteDone, this, &ModbusPoll::handleWriteDone);

        // Per-connection internal-parameter master (owns its own connection)
        auto ipConn = new ModbusConnection();
        auto ipMaster = new InternalParamMaster(ipConn, pSettingsModel, i, this);
        _internalParamMasters.append(ipMaster);

        connect(ipMaster, &InternalParamMaster::readDone,
                this, &ModbusPoll::internalParamReadDone);
        connect(ipMaster, &InternalParamMaster::writeDone,
                this, &ModbusPoll::internalParamWriteDone);
        connect(ipMaster, &InternalParamMaster::writeKeyDone,
                this, &ModbusPoll::internalParamWriteKeyDone);
    }

    _activeMastersCount = 0;
    _lastPollStart = QDateTime::currentMSecsSinceEpoch();
}

ModbusPoll::~ModbusPoll()
{
    for (quint8 i = 0u; i < _modbusMasters.size(); i++)
    {
        _modbusMasters[i]->pModbusMaster->disconnect();

        delete _modbusMasters[i]->pModbusMaster;
        delete _modbusMasters[i];
    }

    delete _pPollTimer;
}

void ModbusPoll::startCommunication(QList<ModbusRegister>& registerList)
{
    _pRegisterValueHandler->setRegisters(registerList);

    // Trigger read immediately
    _pPollTimer->singleShot(1, this, &ModbusPoll::triggerRegisterRead);

    _bPollActive = true;

    qCInfo(scopeComm) << QString("Start logging: %1").arg(FormatDateTime::currentDateTime());

    for (quint8 i = 0u; i < ConnectionTypes::ID_CNT; i++)
    {
        QList<ModbusDataUnit> addrList;
        _pRegisterValueHandler->registerAddresListForConnection(addrList, i);

        if (!addrList.isEmpty())
        {
            auto connData = _pSettingsModel->connectionSettings(i);
            QString str;
            if (connData->connectionType() == ConnectionTypes::TYPE_TCP)
            {
                str = QString("[Conn %1] %2:%3").arg(i + 1).arg(connData->ipAddress()).arg(connData->port());
            }
            else
            {
                QString strParity;
                QString strDataBits;
                QString strStopBits;
                connData->serialConnectionStrings(strParity, strDataBits, strStopBits);

                str = QString("[Conn %1] %2, %3, %4, %5, %6")
                        .arg(i + 1)
                        .arg(connData->portName())
                        .arg(connData->baudrate())
                        .arg(strParity, strDataBits, strStopBits);
            }
            qCInfo(scopeCommConnection) << qPrintable(str);

            for (deviceId_t devId : _pSettingsModel->deviceListForConnection(i))
            {
                Device* dev = _pSettingsModel->deviceSettings(devId);
                QString devStr = QString("[Device] %1: slave ID %2, max consecutive %3, 32-bit little endian %4")
                                   .arg(dev->name())
                                   .arg(dev->slaveId())
                                   .arg(dev->consecutiveMax())
                                   .arg(dev->int32LittleEndian() ? "true" : "false");
                qCInfo(scopeCommConnection) << qPrintable(devStr);
            }
        }
    }

    resetCommunicationStats();
}

void ModbusPoll::resetCommunicationStats()
{
    _lastPollStart = QDateTime::currentMSecsSinceEpoch();
}

void ModbusPoll::handlePollDone(ModbusResultMap partialResultMap, connectionId_t connectionId)
{
    bool lastResult = false;

    quint8 activeCnt = 0;
    for (quint8 i = 0; i < ConnectionTypes::ID_CNT; i++)
    {
        if (_modbusMasters[i]->bActive)
        {
            activeCnt++;
        }
    }

    /* Last active modbus master has returned its result */
    if (activeCnt == 1 || activeCnt == 0)
    {
        /* Last result */
        lastResult = true;
    }

    // Always add data to result map
    _pRegisterValueHandler->processPartialResult(partialResultMap, connectionId);

    // Set master as inactive
    if (connectionId < ConnectionTypes::ID_CNT)
    {
        _modbusMasters[connectionId]->bActive = false;
    }

    if (lastResult)
    {
        _pRegisterValueHandler->finishRead();

        // Restart timer when previous request has been handled
        uint waitInterval;
        const quint32 passedInterval = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() - _lastPollStart);

        if (passedInterval > _pSettingsModel->pollTime())
        {
            // Poll again immediately
            waitInterval = 1;
        }
        else
        {
            // Set waitInterval to remaining time
            waitInterval = _pSettingsModel->pollTime() - passedInterval;
        }

        _pPollTimer->singleShot(static_cast<int>(waitInterval), this, &ModbusPoll::triggerRegisterRead);
    }
}

void ModbusPoll::stopCommunication()
{
    _bPollActive = false;
    _pPollTimer->stop();

    qCInfo(scopeComm) << QString("Stop logging: %1").arg(FormatDateTime::currentDateTime());

    for (quint8 i = 0; i < ConnectionTypes::ID_CNT; i++)
    {
        _modbusMasters[i]->pModbusMaster->cleanUp();
    }
}

bool ModbusPoll::isActive()
{
    return _bPollActive;
}

void ModbusPoll::triggerRegisterRead()
{
    if(_bPollActive)
    {
        _lastPollStart = QDateTime::currentMSecsSinceEpoch();

        _pRegisterValueHandler->startRead();

        /* Strange construction is required to avoid race condition:
         *
         * First set _activeMastersCount to correct value
         * And only then activate masters (readRegisterList)
         *
         * readRegisterList can return immediately and this will give race condition otherwise
         */

        _activeMastersCount = 0;

        QList<QList<ModbusDataUnit> > regAddrList;

        for (connectionId_t i = 0u; i < ConnectionTypes::ID_CNT; i++)
        {
            regAddrList.append(QList<ModbusDataUnit>());

            _pRegisterValueHandler->registerAddresListForConnection(regAddrList.last(), i);

            if (regAddrList.last().count() > 0)
            {
                _activeMastersCount++;
            }
        }

        for (connectionId_t i = 0u; i < ConnectionTypes::ID_CNT; i++)
        {
            if (regAddrList.at(i).count() > 0)
            {
                quint8 consecutiveMax = lowestConsecutiveMaxForConnection(i);
                _modbusMasters[i]->bActive = true;
                _modbusMasters[i]->pModbusMaster->readRegisterList(regAddrList.at(i), consecutiveMax);
            }
        }

        if (_activeMastersCount == 0)
        {
            ModbusResultMap emptyResultMap;
            handlePollDone(emptyResultMap, ConnectionTypes::ID_1);
        }
    }
}

quint8 ModbusPoll::lowestConsecutiveMaxForConnection(connectionId_t connId) const
{
    quint8 consecutiveMax = 128;
    QList<deviceId_t> devList = _pSettingsModel->deviceListForConnection(connId);
    for (deviceId_t devId : std::as_const(devList))
    {
        quint8 devConsecutiveMax = _pSettingsModel->deviceSettings(devId)->consecutiveMax();
        if (devConsecutiveMax < consecutiveMax)
        {
            consecutiveMax = devConsecutiveMax;
        }
    }
    return consecutiveMax;
}

void ModbusPoll::writeRegister(connectionId_t connId, quint16 address, quint8 slaveId, quint16 value)
{
    if (connId < static_cast<connectionId_t>(ConnectionTypes::ID_CNT))
    {
        ModbusDataUnit regAddr(address, ModbusAddress::ObjectType::HOLDING_REGISTER, slaveId);
        _modbusMasters[connId]->pModbusMaster->writeRegister(regAddr, value);
    }
}

void ModbusPoll::readInternalParam(connectionId_t connId, quint8 slaveId,
                                   quint16 paramAddr, bool is32Bit)
{
    if (connId >= static_cast<connectionId_t>(ConnectionTypes::ID_CNT))
    {
        emit internalParamReadDone(false, tr("Invalid connection ID"), 0, 0);
        return;
    }

    if (_pSettingsModel->connectionSettings(connId)->connectionType() != ConnectionTypes::TYPE_SERIAL)
    {
        emit internalParamReadDone(false, tr("Internal parameter access is only supported for RTU (serial) connections"), 0, 0);
        return;
    }

    if (_bPollActive)
    {
        emit internalParamReadDone(false, tr("Stop logging before using internal parameter access"), 0, 0);
        return;
    }

    if (_internalParamMasters[static_cast<int>(connId)]->isBusy())
    {
        emit internalParamReadDone(false, tr("Internal parameter master is busy"), 0, 0);
        return;
    }

    _internalParamMasters[static_cast<int>(connId)]->readParam(slaveId, paramAddr, is32Bit);
}

void ModbusPoll::writeInternalParam(connectionId_t connId, quint8 slaveId,
                                    quint16 paramAddr, quint16 word1, quint16 word2, bool is32Bit)
{
    if (connId >= static_cast<connectionId_t>(ConnectionTypes::ID_CNT))
    {
        emit internalParamWriteDone(false, tr("Invalid connection ID"), 0, 0);
        return;
    }

    if (_pSettingsModel->connectionSettings(connId)->connectionType() != ConnectionTypes::TYPE_SERIAL)
    {
        emit internalParamWriteDone(false, tr("Internal parameter access is only supported for RTU (serial) connections"), 0, 0);
        return;
    }

    if (_bPollActive)
    {
        emit internalParamWriteDone(false, tr("Stop logging before using internal parameter access"), 0, 0);
        return;
    }

    if (_internalParamMasters[static_cast<int>(connId)]->isBusy())
    {
        emit internalParamWriteDone(false, tr("Internal parameter master is busy"), 0, 0);
        return;
    }

    _internalParamMasters[static_cast<int>(connId)]->writeParam(slaveId, paramAddr, word1, word2, is32Bit);
}

void ModbusPoll::writeKey(connectionId_t connId, quint8 slaveId, quint16 key)
{
    if (connId >= static_cast<connectionId_t>(ConnectionTypes::ID_CNT))
    {
        emit internalParamWriteKeyDone(false, tr("Invalid connection ID"));
        return;
    }

    if (_pSettingsModel->connectionSettings(connId)->connectionType() != ConnectionTypes::TYPE_SERIAL)
    {
        emit internalParamWriteKeyDone(false, tr("Internal parameter access is only supported for RTU (serial) connections"));
        return;
    }

    if (_bPollActive)
    {
        emit internalParamWriteKeyDone(false, tr("Stop logging before using internal parameter access"));
        return;
    }

    if (_internalParamMasters[static_cast<int>(connId)]->isBusy())
    {
        emit internalParamWriteKeyDone(false, tr("Internal parameter master is busy"));
        return;
    }

    _internalParamMasters[static_cast<int>(connId)]->writeKey(slaveId, key);
}

void ModbusPoll::handleWriteDone(bool success, QString errorMessage, connectionId_t connectionId)
{
    Q_UNUSED(connectionId);
    emit writeRegisterDone(success, errorMessage);
}
