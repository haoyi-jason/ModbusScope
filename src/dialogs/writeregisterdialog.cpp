#include "dialogs/writeregisterdialog.h"
#include "communication/modbuspoll.h"
#include "dialogs/ui_writeregisterdialog.h"

WriteRegisterDialog::WriteRegisterDialog(ModbusPoll* pModbusPoll, QWidget* parent)
    : QDialog(parent), ui(new Ui::WriteRegisterDialog), _pModbusPoll(pModbusPoll)
{
    ui->setupUi(this);

    for (quint32 i = 0u; i < ConnectionTypes::ID_CNT; i++)
    {
        ui->cmbConnection->addItem(QString("Connection %1").arg(i + 1));
    }

    connect(ui->btnWrite, &QPushButton::clicked, this, &WriteRegisterDialog::onWriteClicked);
    connect(ui->btnClose, &QPushButton::clicked, this, &QDialog::accept);
    connect(_pModbusPoll, &ModbusPoll::writeRegisterDone, this, &WriteRegisterDialog::onWriteDone);
}

WriteRegisterDialog::~WriteRegisterDialog()
{
    delete ui;
}

void WriteRegisterDialog::onWriteClicked()
{
    ui->btnWrite->setEnabled(false);
    ui->lblStatus->clear();

    const auto connId = static_cast<ConnectionTypes::connectionId_t>(ui->cmbConnection->currentIndex());
    const quint16 address = static_cast<quint16>(ui->spnAddress->value());
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());
    const quint16 value = static_cast<quint16>(ui->spnValue->value());

    _pModbusPoll->writeRegister(connId, address, slaveId, value);
}

void WriteRegisterDialog::onWriteDone(bool success, const QString& errorMessage)
{
    ui->btnWrite->setEnabled(true);

    if (success)
    {
        ui->lblStatus->setText(tr("Write successful"));
    }
    else
    {
        ui->lblStatus->setText(tr("Error: %1").arg(errorMessage));
    }
}
