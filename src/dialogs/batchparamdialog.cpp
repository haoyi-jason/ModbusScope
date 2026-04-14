#include "dialogs/batchparamdialog.h"
#include "communication/modbuspoll.h"
#include "dialogs/ui_batchparamdialog.h"

#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QTextStream>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <climits>

// Column indices
static const int COL_NAME    = 0;
static const int COL_ADDRESS = 1;
static const int COL_TYPE    = 2;
static const int COL_VALUE   = 3;

BatchParamDialog::BatchParamDialog(ModbusPoll* pModbusPoll, QWidget* parent)
    : QDialog(parent), ui(new Ui::BatchParamDialog), _pModbusPoll(pModbusPoll)
{
    ui->setupUi(this);

    for (quint32 i = 0u; i < ConnectionTypes::ID_CNT; i++)
    {
        ui->cmbConnection->addItem(QString("Connection %1").arg(i + 1));
    }

    ui->tblParams->horizontalHeader()->setStretchLastSection(true);
    ui->tblParams->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->btnStop->setEnabled(false);

    connect(ui->btnLoadCsv,    &QPushButton::clicked, this, &BatchParamDialog::onLoadCsvClicked);
    connect(ui->btnSaveCsv,    &QPushButton::clicked, this, &BatchParamDialog::onSaveCsvClicked);
    connect(ui->btnAddRow,     &QPushButton::clicked, this, &BatchParamDialog::onAddRowClicked);
    connect(ui->btnRemoveRow,  &QPushButton::clicked, this, &BatchParamDialog::onRemoveRowClicked);
    connect(ui->btnBatchRead,  &QPushButton::clicked, this, &BatchParamDialog::onBatchReadClicked);
    connect(ui->btnBatchWrite, &QPushButton::clicked, this, &BatchParamDialog::onBatchWriteClicked);
    connect(ui->btnStop,       &QPushButton::clicked, this, &BatchParamDialog::onStopClicked);
    connect(ui->btnClose,      &QPushButton::clicked, this, &QDialog::accept);
    connect(ui->chkHexDisplay, &QCheckBox::toggled,   this, &BatchParamDialog::onHexDisplayToggled);

    connect(_pModbusPoll, &ModbusPoll::internalParamReadDone,
            this, &BatchParamDialog::onBatchReadStepDone);
    connect(_pModbusPoll, &ModbusPoll::internalParamWriteDone,
            this, &BatchParamDialog::onBatchWriteStepDone);
}

BatchParamDialog::~BatchParamDialog()
{
    delete ui;
}

void BatchParamDialog::setConnection(ConnectionTypes::connectionId_t connId, quint8 slaveId)
{
    ui->cmbConnection->setCurrentIndex(static_cast<int>(connId));
    ui->spnSlaveId->setValue(slaveId);
}

void BatchParamDialog::onLoadCsvClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Load CSV"), QString(), tr("CSV files (*.csv);;All files (*)"));

    if (!filePath.isEmpty())
    {
        loadCsvToTable(filePath);
    }
}

void BatchParamDialog::onSaveCsvClicked()
{
    QString filePath = QFileDialog::getSaveFileName(
        this, tr("Save CSV"), QString(), tr("CSV files (*.csv);;All files (*)"));

    if (!filePath.isEmpty())
    {
        if (!filePath.endsWith(".csv", Qt::CaseInsensitive))
        {
            filePath += ".csv";
        }
        saveTableToCsv(filePath);
    }
}

void BatchParamDialog::onAddRowClicked()
{
    const int row = ui->tblParams->rowCount();
    ui->tblParams->insertRow(row);
    ui->tblParams->setItem(row, COL_NAME,    new QTableWidgetItem(QString()));
    ui->tblParams->setItem(row, COL_ADDRESS, new QTableWidgetItem(QString()));
    ui->tblParams->setItem(row, COL_TYPE,    new QTableWidgetItem("16"));
    ui->tblParams->setItem(row, COL_VALUE,   new QTableWidgetItem(QString()));
}

void BatchParamDialog::onRemoveRowClicked()
{
    const int row = ui->tblParams->currentRow();
    if (row >= 0)
    {
        ui->tblParams->removeRow(row);
    }
}

void BatchParamDialog::onBatchReadClicked()
{
    const int total = ui->tblParams->rowCount();
    if (total == 0)
    {
        ui->lblProgress->setText(tr("No rows to read."));
        return;
    }

    _batchActive      = true;
    _batchIsWrite     = false;
    _stopRequested    = false;
    _currentBatchIndex = 0;
    _totalBatchRows   = total;

    setBatchButtonsEnabled(false);
    startNextRead();
}

void BatchParamDialog::onBatchWriteClicked()
{
    const int total = ui->tblParams->rowCount();
    if (total == 0)
    {
        ui->lblProgress->setText(tr("No rows to write."));
        return;
    }

    _batchActive      = true;
    _batchIsWrite     = true;
    _stopRequested    = false;
    _currentBatchIndex = 0;
    _totalBatchRows   = total;

    setBatchButtonsEnabled(false);
    startNextWrite();
}

void BatchParamDialog::onStopClicked()
{
    _stopRequested = true;
}

void BatchParamDialog::onBatchReadStepDone(bool success, const QString& errorMsg,
                                            quint16 word1, quint16 word2)
{
    if (!_batchActive || _batchIsWrite)
    {
        return;
    }

    const int row = _currentBatchIndex;

    if (success)
    {
        const bool is32 = (ui->tblParams->item(row, COL_TYPE) != nullptr)
                          && (ui->tblParams->item(row, COL_TYPE)->text() == "32");
        const quint32 value = is32
                              ? (static_cast<quint32>(word1) | (static_cast<quint32>(word2) << 16))
                              : static_cast<quint32>(word1);

        ui->tblParams->setItem(row, COL_VALUE,
            new QTableWidgetItem(formatValue(value, is32, ui->chkHexDisplay->isChecked())));
    }
    else
    {
        ui->tblParams->setItem(row, COL_VALUE, new QTableWidgetItem(tr("Error: %1").arg(errorMsg)));
    }

    _currentBatchIndex++;

    if (_stopRequested || _currentBatchIndex >= _totalBatchRows)
    {
        finishBatch();
    }
    else
    {
        startNextRead();
    }
}

void BatchParamDialog::onBatchWriteStepDone(bool success, const QString& errorMsg,
                                             quint16 /*word1*/, quint16 /*word2*/)
{
    if (!_batchActive || !_batchIsWrite)
    {
        return;
    }

    if (!success)
    {
        ui->lblProgress->setText(tr("Error at row %1: %2").arg(_currentBatchIndex + 1).arg(errorMsg));
    }

    _currentBatchIndex++;

    if (_stopRequested || _currentBatchIndex >= _totalBatchRows)
    {
        finishBatch();
    }
    else
    {
        startNextWrite();
    }
}

void BatchParamDialog::startNextRead()
{
    const int row = _currentBatchIndex;

    ui->lblProgress->setText(tr("Reading %1/%2…").arg(row + 1).arg(_totalBatchRows));

    const QTableWidgetItem* addrItem = ui->tblParams->item(row, COL_ADDRESS);
    const QTableWidgetItem* typeItem = ui->tblParams->item(row, COL_TYPE);

    bool ok = false;
    const quint16 addr = static_cast<quint16>(parseHexOrDec32(
        addrItem ? addrItem->text() : QString(), &ok));

    if (!ok)
    {
        // Skip invalid row
        ui->tblParams->setItem(row, COL_VALUE, new QTableWidgetItem(tr("Invalid address")));
        _currentBatchIndex++;
        if (_currentBatchIndex >= _totalBatchRows)
        {
            finishBatch();
        }
        else
        {
            startNextRead();
        }
        return;
    }

    const bool is32 = typeItem && (typeItem->text() == "32");
    const auto connId = static_cast<ConnectionTypes::connectionId_t>(ui->cmbConnection->currentIndex());
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());

    _pModbusPoll->readInternalParam(connId, slaveId, addr, is32);
}

void BatchParamDialog::startNextWrite()
{
    const int row = _currentBatchIndex;

    ui->lblProgress->setText(tr("Writing %1/%2…").arg(row + 1).arg(_totalBatchRows));

    const QTableWidgetItem* addrItem  = ui->tblParams->item(row, COL_ADDRESS);
    const QTableWidgetItem* typeItem  = ui->tblParams->item(row, COL_TYPE);
    const QTableWidgetItem* valueItem = ui->tblParams->item(row, COL_VALUE);

    bool addrOk = false;
    const quint16 addr = static_cast<quint16>(parseHexOrDec32(
        addrItem ? addrItem->text() : QString(), &addrOk));

    bool valOk = false;
    const quint32 rawValue = parseHexOrDec32(
        valueItem ? valueItem->text() : QString(), &valOk);

    if (!addrOk || !valOk)
    {
        // Skip invalid row
        _currentBatchIndex++;
        if (_currentBatchIndex >= _totalBatchRows)
        {
            finishBatch();
        }
        else
        {
            startNextWrite();
        }
        return;
    }

    const bool is32 = typeItem && (typeItem->text() == "32");
    const quint16 word1 = static_cast<quint16>(rawValue & 0xFFFF);
    const quint16 word2 = static_cast<quint16>((rawValue >> 16) & 0xFFFF);

    const auto connId = static_cast<ConnectionTypes::connectionId_t>(ui->cmbConnection->currentIndex());
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());

    _pModbusPoll->writeInternalParam(connId, slaveId, addr, word1, word2, is32);
}

void BatchParamDialog::finishBatch()
{
    _batchActive = false;
    setBatchButtonsEnabled(true);

    if (_stopRequested)
    {
        ui->lblProgress->setText(tr("Stopped at row %1/%2.").arg(_currentBatchIndex).arg(_totalBatchRows));
    }
    else
    {
        ui->lblProgress->setText(_batchIsWrite ? tr("Write done.") : tr("Read done."));
    }
}

void BatchParamDialog::setBatchButtonsEnabled(bool enabled)
{
    ui->btnBatchRead->setEnabled(enabled);
    ui->btnBatchWrite->setEnabled(enabled);
    ui->btnLoadCsv->setEnabled(enabled);
    ui->btnSaveCsv->setEnabled(enabled);
    ui->btnAddRow->setEnabled(enabled);
    ui->btnRemoveRow->setEnabled(enabled);
    ui->btnStop->setEnabled(!enabled);
}

void BatchParamDialog::loadCsvToTable(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file: %1").arg(filePath));
        return;
    }

    QTextStream in(&file);
    ui->tblParams->setRowCount(0);

    // Skip header
    if (!in.atEnd())
    {
        in.readLine();
    }

    while (!in.atEnd())
    {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        const QStringList parts = line.split(',');
        if (parts.size() < 3)
        {
            continue;
        }

        const QString name    = parts[0].trimmed();
        const QString addrStr = parts[1].trimmed();
        const QString typeStr = parts[2].trimmed();
        const QString valStr  = (parts.size() >= 4) ? parts[3].trimmed() : QString();

        bool addrOk = false;
        const quint32 addr = parseHexOrDec32(addrStr, &addrOk);
        if (!addrOk)
        {
            continue;
        }

        // Only "16" and "32" are valid type values
        if (typeStr != "16" && typeStr != "32")
        {
            continue;
        }
        const bool is32 = (typeStr == "32");

        bool valOk = false;
        quint32 value = 0;
        if (!valStr.isEmpty())
        {
            value = parseHexOrDec32(valStr, &valOk);
        }

        const bool hexMode = ui->chkHexDisplay->isChecked();

        const int row = ui->tblParams->rowCount();
        ui->tblParams->insertRow(row);
        ui->tblParams->setItem(row, COL_NAME,
            new QTableWidgetItem(name));
        ui->tblParams->setItem(row, COL_ADDRESS,
            new QTableWidgetItem(formatAddress(static_cast<quint16>(addr), hexMode)));
        ui->tblParams->setItem(row, COL_TYPE,
            new QTableWidgetItem(is32 ? "32" : "16"));
        ui->tblParams->setItem(row, COL_VALUE,
            new QTableWidgetItem(valOk ? formatValue(value, is32, hexMode) : QString()));
    }

    ui->lblProgress->setText(tr("Loaded %1 rows from CSV.").arg(ui->tblParams->rowCount()));
}

void BatchParamDialog::saveTableToCsv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Error"), tr("Cannot write file: %1").arg(filePath));
        return;
    }

    QTextStream out(&file);
    out << "name,address,type,value\n";

    for (int row = 0; row < ui->tblParams->rowCount(); row++)
    {
        const QTableWidgetItem* nameItem  = ui->tblParams->item(row, COL_NAME);
        const QTableWidgetItem* addrItem  = ui->tblParams->item(row, COL_ADDRESS);
        const QTableWidgetItem* typeItem  = ui->tblParams->item(row, COL_TYPE);
        const QTableWidgetItem* valueItem = ui->tblParams->item(row, COL_VALUE);

        const QString name    = nameItem  ? nameItem->text()  : QString();
        const QString addr    = addrItem  ? addrItem->text()  : QString();
        const QString type    = typeItem  ? typeItem->text()  : QString();
        const QString value   = valueItem ? valueItem->text() : QString();

        out << name << "," << addr << "," << type << "," << value << "\n";
    }

    ui->lblProgress->setText(tr("Saved %1 rows to CSV.").arg(ui->tblParams->rowCount()));
}

quint32 BatchParamDialog::parseHexOrDec32(const QString& text, bool* ok)
{
    const QString trimmed = text.trimmed();
    bool localOk = false;
    quint32 value = 0;

    if (trimmed.startsWith("0x", Qt::CaseInsensitive))
    {
        value = trimmed.mid(2).toUInt(&localOk, 16);
    }
    else if (!trimmed.isEmpty())
    {
        // Use toLongLong so that negative decimal values (e.g. -1) are accepted.
        // We accept any value whose bit pattern fits in 32 bits, i.e. the range
        // [INT32_MIN, UINT32_MAX].
        const qint64 signed64 = trimmed.toLongLong(&localOk, 10);
        if (localOk)
        {
            if (signed64 < static_cast<qint64>(INT32_MIN) || signed64 > static_cast<qint64>(UINT32_MAX))
            {
                localOk = false;
            }
            else
            {
                value = static_cast<quint32>(signed64);
            }
        }
    }

    if (ok)
    {
        *ok = localOk;
    }

    return value;
}

QString BatchParamDialog::formatValue(quint32 value, bool is32Bit, bool hexMode)
{
    if (hexMode)
    {
        if (is32Bit)
        {
            return QString("0x%1").arg(value, 8, 16, QChar('0')).toUpper();
        }
        else
        {
            return QString("0x%1").arg(value & 0xFFFF, 4, 16, QChar('0')).toUpper();
        }
    }
    else
    {
        return is32Bit ? QString::number(value) : QString::number(value & 0xFFFF);
    }
}

QString BatchParamDialog::formatAddress(quint16 addr, bool hexMode)
{
    if (hexMode)
    {
        return QString("0x%1").arg(addr, 4, 16, QChar('0')).toUpper();
    }
    else
    {
        return QString::number(addr);
    }
}

void BatchParamDialog::onHexDisplayToggled(bool checked)
{
    for (int row = 0; row < ui->tblParams->rowCount(); row++)
    {
        // Reformat Address column
        const QTableWidgetItem* addrItem = ui->tblParams->item(row, COL_ADDRESS);
        if (addrItem)
        {
            bool ok = false;
            const quint32 addr = parseHexOrDec32(addrItem->text(), &ok);
            if (ok)
            {
                ui->tblParams->setItem(row, COL_ADDRESS,
                    new QTableWidgetItem(formatAddress(static_cast<quint16>(addr), checked)));
            }
        }

        // Reformat Value column (skip cells that don't hold a plain number)
        const QTableWidgetItem* typeItem  = ui->tblParams->item(row, COL_TYPE);
        const QTableWidgetItem* valueItem = ui->tblParams->item(row, COL_VALUE);
        if (valueItem)
        {
            bool ok = false;
            const quint32 val = parseHexOrDec32(valueItem->text(), &ok);
            if (ok)
            {
                const bool is32 = typeItem && (typeItem->text() == "32");
                ui->tblParams->setItem(row, COL_VALUE,
                    new QTableWidgetItem(formatValue(val, is32, checked)));
            }
        }
    }
}
