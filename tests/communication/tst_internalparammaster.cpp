
#include "tst_internalparammaster.h"

#include "communication/internalparammaster.h"
#include "modbusconnectionfake.h"
#include "models/device.h"

#include <QSignalSpy>
#include <QTest>

void TestInternalParamMaster::init()
{
    _pSettingsModel = new SettingsModel;
    _pTestDevice = new TestDevice();

    // Replace default 50-register slave data with a larger one to cover reg 101/102
    _pTestDevice->setSlaveData(QModbusDataUnit::HoldingRegisters, new TestSlaveData(0, 200));

    // Use a serial connection type so the internal param is allowed
    auto connData = _pSettingsModel->connectionSettings(ConnectionTypes::ID_1);
    connData->setConnectionType(ConnectionTypes::TYPE_SERIAL);
    connData->setPortName("COM1");
    connData->setTimeout(500);

    deviceId_t devId = Device::cFirstDeviceId;
    _pSettingsModel->addDevice(devId);
    _pSettingsModel->deviceSettings(devId)->setConnectionId(ConnectionTypes::ID_1);
    _pSettingsModel->deviceSettings(devId)->setSlaveId(1);
}

void TestInternalParamMaster::cleanup()
{
    delete _pTestDevice;
    delete _pSettingsModel;
}

void TestInternalParamMaster::readParam16bitSuccess()
{
    // Configure holding registers: reg 101 = 0xABCD
    _pTestDevice->configureHoldingRegister(101, true, 0xABCD);

    ModbusConnectionFake* conn = new ModbusConnectionFake();
    conn->addSlaveDevice(1, _pTestDevice);

    InternalParamMaster master(conn, _pSettingsModel, ConnectionTypes::ID_1);

    QSignalSpy spyReadDone(&master, &InternalParamMaster::readDone);

    master.readParam(1, 42, false);

    QVERIFY(spyReadDone.wait(600));
    QCOMPARE(spyReadDone.count(), 1);

    auto args = spyReadDone.takeFirst();
    QCOMPARE(args[0].toBool(), true);
    QCOMPARE(args[1].toString(), QString());
    QCOMPARE(args[2].toUInt(), static_cast<quint16>(0xABCD));
    QCOMPARE(args[3].toUInt(), static_cast<quint16>(0));
}

void TestInternalParamMaster::readParam32bitSuccess()
{
    // Configure holding registers: reg 101 = 0x1234, reg 102 = 0x5678
    _pTestDevice->configureHoldingRegister(101, true, 0x1234);
    _pTestDevice->configureHoldingRegister(102, true, 0x5678);

    ModbusConnectionFake* conn = new ModbusConnectionFake();
    conn->addSlaveDevice(1, _pTestDevice);

    InternalParamMaster master(conn, _pSettingsModel, ConnectionTypes::ID_1);

    QSignalSpy spyReadDone(&master, &InternalParamMaster::readDone);

    master.readParam(1, 42, true);

    QVERIFY(spyReadDone.wait(600));
    QCOMPARE(spyReadDone.count(), 1);

    auto args = spyReadDone.takeFirst();
    QCOMPARE(args[0].toBool(), true);
    QCOMPARE(args[1].toString(), QString());
    QCOMPARE(args[2].toUInt(), static_cast<quint16>(0x1234));
    QCOMPARE(args[3].toUInt(), static_cast<quint16>(0x5678));
}

void TestInternalParamMaster::writeParam16bitSuccess()
{
    // After write, master reads back from reg 101.
    // Configure the device so reg 101 returns 0xBEEF after the write.
    _pTestDevice->configureHoldingRegister(101, true, 0xBEEF);

    ModbusConnectionFake* conn = new ModbusConnectionFake();
    conn->addSlaveDevice(1, _pTestDevice);

    InternalParamMaster master(conn, _pSettingsModel, ConnectionTypes::ID_1);

    QSignalSpy spyWriteDone(&master, &InternalParamMaster::writeDone);

    master.writeParam(1, 42, 0xBEEF, 0, false);

    QVERIFY(spyWriteDone.wait(600));
    QCOMPARE(spyWriteDone.count(), 1);

    auto args = spyWriteDone.takeFirst();
    QCOMPARE(args[0].toBool(), true);
    QCOMPARE(args[1].toString(), QString());
    QCOMPARE(args[2].toUInt(), static_cast<quint16>(0xBEEF));
}

void TestInternalParamMaster::writeParam32bitSuccess()
{
    // After write, master reads back from reg 101 and 102.
    _pTestDevice->configureHoldingRegister(101, true, 0x0011);
    _pTestDevice->configureHoldingRegister(102, true, 0x0022);

    ModbusConnectionFake* conn = new ModbusConnectionFake();
    conn->addSlaveDevice(1, _pTestDevice);

    InternalParamMaster master(conn, _pSettingsModel, ConnectionTypes::ID_1);

    QSignalSpy spyWriteDone(&master, &InternalParamMaster::writeDone);

    master.writeParam(1, 42, 0x0011, 0x0022, true);

    QVERIFY(spyWriteDone.wait(600));
    QCOMPARE(spyWriteDone.count(), 1);

    auto args = spyWriteDone.takeFirst();
    QCOMPARE(args[0].toBool(), true);
    QCOMPARE(args[1].toString(), QString());
    QCOMPARE(args[2].toUInt(), static_cast<quint16>(0x0011));
    QCOMPARE(args[3].toUInt(), static_cast<quint16>(0x0022));
}

QTEST_GUILESS_MAIN(TestInternalParamMaster)
