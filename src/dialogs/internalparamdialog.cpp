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
    connect(ui->btnWriteKey, &QPushButton::clicked, this, &InternalParamDialog::onWriteKeyClicked);
    connect(ui->btnClose, &QPushButton::clicked, this, &QDialog::accept);

    connect(ui->radioBtn16bit, &QRadioButton::toggled, this, &InternalParamDialog::onModeChanged);
    connect(ui->radioBtn32bit, &QRadioButton::toggled, this, &InternalParamDialog::onModeChanged);

    connect(_pModbusPoll, &ModbusPoll::internalParamReadDone,
            this, &InternalParamDialog::onReadDone);
    connect(_pModbusPoll, &ModbusPoll::internalParamWriteDone,
            this, &InternalParamDialog::onWriteDone);
    connect(_pModbusPoll, &ModbusPoll::internalParamWriteKeyDone,
            this, &InternalParamDialog::onWriteKeyDone);

    // Apply initial mode state
    onModeChanged();
}

InternalParamDialog::~InternalParamDialog()
{
    delete ui;
}

void InternalParamDialog::onReadClicked()
{
    bool ok = false;
    const quint16 paramAddr = parseHexOrDec(ui->leParamAddr->text(), &ok);
    if (!ok)
    {
        ui->lblStatus->setText(tr("Invalid parameter address"));
        return;
    }

    setButtonsEnabled(false);
    ui->lblStatus->setText(tr("Reading…"));

    const auto connId = selectedConnId();
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());

    _pModbusPoll->readInternalParam(connId, slaveId, paramAddr, is32Bit());
}

void InternalParamDialog::onWriteClicked()
{
    bool ok1 = false, ok2 = false, ok3 = false;
    const quint16 paramAddr = parseHexOrDec(ui->leParamAddr->text(), &ok1);
    const quint16 word1 = parseHexOrDec(ui->leWord1->text(), &ok2);
    const quint16 word2 = parseHexOrDec(ui->leWord2->text(), &ok3);

    if (!ok1)
    {
        ui->lblStatus->setText(tr("Invalid parameter address"));
        return;
    }
    if (!ok2)
    {
        ui->lblStatus->setText(tr("Invalid Word 1 value"));
        return;
    }
    if (is32Bit() && !ok3)
    {
        ui->lblStatus->setText(tr("Invalid Word 2 value"));
        return;
    }

    setButtonsEnabled(false);
    ui->lblStatus->setText(tr("Writing…"));

    const auto connId = selectedConnId();
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());

    _pModbusPoll->writeInternalParam(connId, slaveId, paramAddr, word1, word2, is32Bit());
}

void InternalParamDialog::onModeChanged()
{
    const bool b32 = is32Bit();
    ui->lblWord2->setVisible(b32);
    ui->leWord2->setVisible(b32);
}

void InternalParamDialog::onReadDone(bool success, const QString& errorMsg,
                                      quint16 word1, quint16 word2)
{
    setButtonsEnabled(true);

    if (success)
    {
        ui->leWord1->setText(QString("0x%1").arg(word1, 4, 16, QChar('0')).toUpper());
        ui->leWord2->setText(QString("0x%1").arg(word2, 4, 16, QChar('0')).toUpper());
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
        ui->leWord1->setText(QString("0x%1").arg(word1, 4, 16, QChar('0')).toUpper());
        ui->leWord2->setText(QString("0x%1").arg(word2, 4, 16, QChar('0')).toUpper());
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
    ui->btnWriteKey->setEnabled(enabled);
}

bool InternalParamDialog::is32Bit() const
{
    return ui->radioBtn32bit->isChecked();
}

ConnectionTypes::connectionId_t InternalParamDialog::selectedConnId() const
{
    return static_cast<ConnectionTypes::connectionId_t>(ui->cmbConnection->currentIndex());
}

void InternalParamDialog::onWriteKeyClicked()
{
    bool ok = false;
    const quint16 key = parseHexOrDec(ui->leKey->text(), &ok);
    if (!ok)
    {
        ui->lblStatus->setText(tr("Invalid key value"));
        return;
    }
    setButtonsEnabled(false);
    ui->lblStatus->setText(tr("Writing key…"));

    const auto connId = selectedConnId();
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());

    _pModbusPoll->writeKey(connId, slaveId, key);
}

void InternalParamDialog::onWriteKeyDone(bool success, const QString& errorMsg)
{
    setButtonsEnabled(true);
    if (success)
        ui->lblStatus->setText(tr("Key written successfully"));
    else
        ui->lblStatus->setText(tr("Key write error: %1").arg(errorMsg));
}

quint16 InternalParamDialog::parseHexOrDec(const QString& text, bool* ok)
{
    QString trimmed = text.trimmed();
    bool localOk = false;
    uint value = 0;
    if (trimmed.startsWith("0x", Qt::CaseInsensitive))
        value = trimmed.toUInt(&localOk, 16);
    else
        value = trimmed.toUInt(&localOk, 10);

    if (localOk && value > 65535u)
        localOk = false;

    if (ok)
        *ok = localOk;

    return static_cast<quint16>(value);
}
