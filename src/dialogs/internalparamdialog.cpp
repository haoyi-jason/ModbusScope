#include "dialogs/internalparamdialog.h"
#include "communication/modbuspoll.h"
#include "dialogs/ui_internalparamdialog.h"

InternalParamDialog::InternalParamDialog(ModbusPoll* pModbusPoll, QWidget* parent)
    : QDialog(parent), ui(new Ui::InternalParamDialog), _pModbusPoll(pModbusPoll)
{
    ui->setupUi(this);

    for (quint32 i = 0u; i < ConnectionTypes::ID_CNT; i++)
    {
        ui->cmbConnection->addItem(QString("Connection %1").arg(i + 1));
    }

    connect(ui->btnRead, &QPushButton::clicked, this, &InternalParamDialog::onReadClicked);
    connect(ui->btnWrite, &QPushButton::clicked, this, &InternalParamDialog::onWriteClicked);
    connect(ui->btnClose, &QPushButton::clicked, this, &QDialog::accept);

    connect(ui->radioBtn16bit, &QRadioButton::toggled, this, &InternalParamDialog::onModeChanged);
    connect(ui->radioBtn32bit, &QRadioButton::toggled, this, &InternalParamDialog::onModeChanged);

    connect(_pModbusPoll, &ModbusPoll::internalParamReadDone,
            this, &InternalParamDialog::onReadDone);
    connect(_pModbusPoll, &ModbusPoll::internalParamWriteDone,
            this, &InternalParamDialog::onWriteDone);

    // Apply initial mode state
    onModeChanged();
}

InternalParamDialog::~InternalParamDialog()
{
    delete ui;
}

void InternalParamDialog::onReadClicked()
{
    setButtonsEnabled(false);
    ui->lblStatus->setText(tr("Reading…"));

    const auto connId = selectedConnId();
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());
    const quint16 paramAddr = static_cast<quint16>(ui->spnParamAddr->value());

    _pModbusPoll->readInternalParam(connId, slaveId, paramAddr, is32Bit());
}

void InternalParamDialog::onWriteClicked()
{
    setButtonsEnabled(false);
    ui->lblStatus->setText(tr("Writing…"));

    const auto connId = selectedConnId();
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());
    const quint16 paramAddr = static_cast<quint16>(ui->spnParamAddr->value());
    const quint16 word1 = static_cast<quint16>(ui->spnWord1->value());
    const quint16 word2 = static_cast<quint16>(ui->spnWord2->value());

    _pModbusPoll->writeInternalParam(connId, slaveId, paramAddr, word1, word2, is32Bit());
}

void InternalParamDialog::onModeChanged()
{
    const bool b32 = is32Bit();
    ui->lblWord2->setVisible(b32);
    ui->spnWord2->setVisible(b32);
}

void InternalParamDialog::onReadDone(bool success, const QString& errorMsg,
                                      quint16 word1, quint16 word2)
{
    setButtonsEnabled(true);

    if (success)
    {
        ui->spnWord1->setValue(word1);
        ui->spnWord2->setValue(word2);
        ui->lblStatus->setText(tr("Read successful: word1=%1, word2=%2").arg(word1).arg(word2));
    }
    else
    {
        ui->lblStatus->setText(tr("Read error: %1").arg(errorMsg));
    }
}

void InternalParamDialog::onWriteDone(bool success, const QString& errorMsg,
                                       quint16 word1, quint16 word2)
{
    setButtonsEnabled(true);

    if (success)
    {
        ui->spnWord1->setValue(word1);
        ui->spnWord2->setValue(word2);
        ui->lblStatus->setText(tr("Write successful (readback: word1=%1, word2=%2)").arg(word1).arg(word2));
    }
    else
    {
        ui->lblStatus->setText(tr("Write error: %1").arg(errorMsg));
    }
}

void InternalParamDialog::setButtonsEnabled(bool enabled)
{
    ui->btnRead->setEnabled(enabled);
    ui->btnWrite->setEnabled(enabled);
}

bool InternalParamDialog::is32Bit() const
{
    return ui->radioBtn32bit->isChecked();
}

ConnectionTypes::connectionId_t InternalParamDialog::selectedConnId() const
{
    return static_cast<ConnectionTypes::connectionId_t>(ui->cmbConnection->currentIndex());
}
