#include "dialogs/batchparamdialog.h"
#include "communication/modbuspoll.h"
#include "dialogs/ui_batchparamdialog.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextStream>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <climits>
#include <cstring>

// Column indices
static const int COL_NAME    = 0;
static const int COL_ADDRESS = 1;
static const int COL_TYPE    = 2;
static const int COL_VALUE   = 3;

// Ordered list of type labels shown in the combobox and stored in CSV
static const QStringList cTypeLabels = {"U8", "I8", "U16", "I16", "U32", "I32", "F32"};

// ---------------------------------------------------------------------------
// Delegate: provides a combobox editor for the Type column
// ---------------------------------------------------------------------------
class BatchTypeDelegate : public QStyledItemDelegate
{
public:
    explicit BatchTypeDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        QComboBox* combo = new QComboBox(parent);
        for (const QString& label : cTypeLabels)
        {
            combo->addItem(label);
        }
        return combo;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);
        if (!combo) return;
        const int idx = combo->findText(index.data(Qt::EditRole).toString());
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        QComboBox* combo = qobject_cast<QComboBox*>(editor);
        if (combo)
        {
            model->setData(index, combo->currentText(), Qt::EditRole);
        }
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex&) const override
    {
        editor->setGeometry(option.rect);
    }
};

// ---------------------------------------------------------------------------
// BatchParamDialog
// ---------------------------------------------------------------------------

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
    ui->tblParams->setItemDelegateForColumn(COL_TYPE, new BatchTypeDelegate(ui->tblParams));

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
    ui->tblParams->setItem(row, COL_TYPE,    new QTableWidgetItem("U16"));
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
        const ModbusDataType::Type type = rowType(row);
        const quint32 rawValue = ModbusDataType::is32Bit(type)
                                 ? (static_cast<quint32>(word1) | (static_cast<quint32>(word2) << 16))
                                 : static_cast<quint32>(word1);

        ui->tblParams->setItem(row, COL_VALUE,
            new QTableWidgetItem(formatValue(rawValue, type, ui->chkHexDisplay->isChecked())));
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

    const ModbusDataType::Type type = rowType(row);
    const bool is32 = ModbusDataType::is32Bit(type);
    const auto connId = static_cast<ConnectionTypes::connectionId_t>(ui->cmbConnection->currentIndex());
    const quint8 slaveId = static_cast<quint8>(ui->spnSlaveId->value());

    _pModbusPoll->readInternalParam(connId, slaveId, addr, is32);
}

void BatchParamDialog::startNextWrite()
{
    const int row = _currentBatchIndex;

    ui->lblProgress->setText(tr("Writing %1/%2…").arg(row + 1).arg(_totalBatchRows));

    const QTableWidgetItem* addrItem  = ui->tblParams->item(row, COL_ADDRESS);
    const QTableWidgetItem* valueItem = ui->tblParams->item(row, COL_VALUE);

    bool addrOk = false;
    const quint16 addr = static_cast<quint16>(parseHexOrDec32(
        addrItem ? addrItem->text() : QString(), &addrOk));

    const ModbusDataType::Type type = rowType(row);
    quint16 word1 = 0;
    quint16 word2 = 0;
    const bool valOk = encodeToWords(valueItem ? valueItem->text() : QString(), type, word1, word2);

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

    const bool is32 = ModbusDataType::is32Bit(type);
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

ModbusDataType::Type BatchParamDialog::rowType(int row) const
{
    const QTableWidgetItem* typeItem = ui->tblParams->item(row, COL_TYPE);
    ModbusDataType::Type type = ModbusDataType::Type::UNSIGNED_16;
    if (typeItem)
    {
        labelToType(typeItem->text(), type);
    }
    return type;
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

        // Accept current labels (U8, I8, U16, I16, U32, I32, F32) plus
        // legacy values from older CSV files ("16" → U16, "32" → U32).
        ModbusDataType::Type type = ModbusDataType::Type::UNSIGNED_16;
        if (typeStr == "16")
        {
            type = ModbusDataType::Type::UNSIGNED_16;
        }
        else if (typeStr == "32")
        {
            type = ModbusDataType::Type::UNSIGNED_32;
        }
        else if (!labelToType(typeStr, type))
        {
            continue;
        }

        const bool hexMode = ui->chkHexDisplay->isChecked();

        const int row = ui->tblParams->rowCount();
        ui->tblParams->insertRow(row);
        ui->tblParams->setItem(row, COL_NAME,
            new QTableWidgetItem(name));
        ui->tblParams->setItem(row, COL_ADDRESS,
            new QTableWidgetItem(formatAddress(static_cast<quint16>(addr), hexMode)));
        ui->tblParams->setItem(row, COL_TYPE,
            new QTableWidgetItem(typeToLabel(type)));

        // Parse and display the value field if present
        quint16 word1 = 0;
        quint16 word2 = 0;
        const bool valOk = !valStr.isEmpty() && encodeToWords(valStr, type, word1, word2);
        const quint32 rawValue = ModbusDataType::is32Bit(type)
                                 ? (static_cast<quint32>(word1) | (static_cast<quint32>(word2) << 16))
                                 : static_cast<quint32>(word1);
        ui->tblParams->setItem(row, COL_VALUE,
            new QTableWidgetItem(valOk ? formatValue(rawValue, type, hexMode) : QString()));
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
            if (signed64 < INT32_MIN || signed64 > static_cast<qint64>(UINT32_MAX))
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

quint32 BatchParamDialog::parseDisplayedValue(const QString& text, ModbusDataType::Type type,
                                               bool currentHexMode, bool* ok)
{
    // For F32 in decimal mode the displayed text is a float string (e.g. "3.14").
    // Re-encode it as raw IEEE-754 bits so the caller can reformat.
    if (type == ModbusDataType::Type::FLOAT_32 && !currentHexMode)
    {
        bool localOk = false;
        const float f = text.trimmed().toFloat(&localOk);
        if (ok) *ok = localOk;
        if (localOk)
        {
            quint32 bits = 0;
            std::memcpy(&bits, &f, sizeof(bits));
            return bits;
        }
        return 0;
    }

    // For all other types (and F32 in hex mode) the displayed text is an integer
    // (possibly hex-prefixed or negative decimal).
    return parseHexOrDec32(text, ok);
}

bool BatchParamDialog::encodeToWords(const QString& text, ModbusDataType::Type type,
                                     quint16& word1, quint16& word2)
{
    const QString trimmed = text.trimmed();
    word1 = 0;
    word2 = 0;

    if (type == ModbusDataType::Type::FLOAT_32)
    {
        bool ok = false;
        const float f = trimmed.toFloat(&ok);
        if (!ok) return false;
        quint32 bits = 0;
        std::memcpy(&bits, &f, sizeof(bits));
        word1 = static_cast<quint16>(bits & 0xFFFF);
        word2 = static_cast<quint16>((bits >> 16) & 0xFFFF);
        return true;
    }

    // Integer types — parse allowing hex prefix and negative decimals
    bool ok = false;
    quint32 bits = 0;

    if (trimmed.startsWith("0x", Qt::CaseInsensitive))
    {
        bits = trimmed.mid(2).toUInt(&ok, 16);
    }
    else if (!trimmed.isEmpty())
    {
        const qint64 val = trimmed.toLongLong(&ok, 10);
        if (ok)
        {
            // Range-check based on type
            bool inRange = true;
            switch (type)
            {
            case ModbusDataType::Type::UNSIGNED_8:
                inRange = (val >= 0 && val <= 255);
                bits = inRange ? static_cast<quint32>(val & 0xFF) : 0;
                break;
            case ModbusDataType::Type::SIGNED_8:
                inRange = (val >= -128 && val <= 127);
                bits = inRange ? static_cast<quint32>(static_cast<quint8>(static_cast<qint8>(val))) : 0;
                break;
            case ModbusDataType::Type::UNSIGNED_16:
                inRange = (val >= 0 && val <= 65535);
                bits = inRange ? static_cast<quint32>(val & 0xFFFF) : 0;
                break;
            case ModbusDataType::Type::SIGNED_16:
                inRange = (val >= -32768 && val <= 32767);
                bits = inRange ? static_cast<quint32>(static_cast<quint16>(static_cast<qint16>(val))) : 0;
                break;
            case ModbusDataType::Type::UNSIGNED_32:
                inRange = (val >= 0 && val <= static_cast<qint64>(UINT32_MAX));
                bits = inRange ? static_cast<quint32>(val) : 0;
                break;
            case ModbusDataType::Type::SIGNED_32:
                inRange = (val >= INT32_MIN && val <= INT32_MAX);
                bits = inRange ? static_cast<quint32>(static_cast<qint32>(val)) : 0;
                break;
            default:
                bits = static_cast<quint32>(val);
                break;
            }
            if (!inRange) ok = false;
        }
    }

    if (!ok) return false;

    word1 = static_cast<quint16>(bits & 0xFFFF);
    word2 = static_cast<quint16>((bits >> 16) & 0xFFFF);
    return true;
}

QString BatchParamDialog::formatValue(quint32 rawValue, ModbusDataType::Type type, bool hexMode)
{
    if (hexMode)
    {
        // Display raw bit pattern with width appropriate to the type
        switch (type)
        {
        case ModbusDataType::Type::UNSIGNED_8:
        case ModbusDataType::Type::SIGNED_8:
            return QString("0x%1").arg(rawValue & 0xFF, 2, 16, QChar('0')).toUpper();
        case ModbusDataType::Type::UNSIGNED_32:
        case ModbusDataType::Type::SIGNED_32:
        case ModbusDataType::Type::FLOAT_32:
            return QString("0x%1").arg(rawValue, 8, 16, QChar('0')).toUpper();
        case ModbusDataType::Type::UNSIGNED_16:
        case ModbusDataType::Type::SIGNED_16:
        default:
            return QString("0x%1").arg(rawValue & 0xFFFF, 4, 16, QChar('0')).toUpper();
        }
    }
    else
    {
        // Display as typed value
        switch (type)
        {
        case ModbusDataType::Type::UNSIGNED_8:
            return QString::number(static_cast<quint8>(rawValue & 0xFF));
        case ModbusDataType::Type::SIGNED_8:
            return QString::number(static_cast<qint8>(rawValue & 0xFF));
        case ModbusDataType::Type::UNSIGNED_16:
        default:
            return QString::number(static_cast<quint16>(rawValue & 0xFFFF));
        case ModbusDataType::Type::SIGNED_16:
            return QString::number(static_cast<qint16>(rawValue & 0xFFFF));
        case ModbusDataType::Type::UNSIGNED_32:
            return QString::number(rawValue);
        case ModbusDataType::Type::SIGNED_32:
            return QString::number(static_cast<qint32>(rawValue));
        case ModbusDataType::Type::FLOAT_32:
        {
            float f = 0.0f;
            std::memcpy(&f, &rawValue, sizeof(f));
            return QString::number(static_cast<double>(f));
        }
        }
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

        // Reformat Value column
        const QTableWidgetItem* valueItem = ui->tblParams->item(row, COL_VALUE);
        if (valueItem && !valueItem->text().isEmpty())
        {
            const ModbusDataType::Type type = rowType(row);
            bool ok = false;
            // !checked is the mode *before* the toggle happened
            const quint32 val = parseDisplayedValue(valueItem->text(), type, !checked, &ok);
            if (ok)
            {
                ui->tblParams->setItem(row, COL_VALUE,
                    new QTableWidgetItem(formatValue(val, type, checked)));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Type label helpers
// ---------------------------------------------------------------------------

QString BatchParamDialog::typeToLabel(ModbusDataType::Type type)
{
    switch (type)
    {
    case ModbusDataType::Type::UNSIGNED_8:  return "U8";
    case ModbusDataType::Type::SIGNED_8:    return "I8";
    case ModbusDataType::Type::UNSIGNED_16: return "U16";
    case ModbusDataType::Type::SIGNED_16:   return "I16";
    case ModbusDataType::Type::UNSIGNED_32: return "U32";
    case ModbusDataType::Type::SIGNED_32:   return "I32";
    case ModbusDataType::Type::FLOAT_32:    return "F32";
    default:                                return "U16";
    }
}

bool BatchParamDialog::labelToType(const QString& label, ModbusDataType::Type& type)
{
    if (label == "U8")        { type = ModbusDataType::Type::UNSIGNED_8;  return true; }
    else if (label == "I8")   { type = ModbusDataType::Type::SIGNED_8;    return true; }
    else if (label == "U16")  { type = ModbusDataType::Type::UNSIGNED_16; return true; }
    else if (label == "I16")  { type = ModbusDataType::Type::SIGNED_16;   return true; }
    else if (label == "U32")  { type = ModbusDataType::Type::UNSIGNED_32; return true; }
    else if (label == "I32")  { type = ModbusDataType::Type::SIGNED_32;   return true; }
    else if (label == "F32")  { type = ModbusDataType::Type::FLOAT_32;    return true; }
    return false;
}

