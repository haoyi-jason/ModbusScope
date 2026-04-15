#ifndef MODBUSDATATYPE_H
#define MODBUSDATATYPE_H

#include <QObject>

class ModbusDataType
{

public:

    enum class Type
    {
        UNSIGNED_16 = 0,
        SIGNED_16 = 1,
        UNSIGNED_32 = 2,
        SIGNED_32 = 3,
        FLOAT_32 = 4,
        UNSIGNED_8 = 5,
        SIGNED_8 = 6,
    };

    static bool is32Bit(ModbusDataType::Type type)
    {
        return cDataTypes[static_cast<int>(type)].b32Bit;
    }

    static bool is8Bit(ModbusDataType::Type type)
    {
        return cDataTypes[static_cast<int>(type)].b8Bit;
    }

    static bool isUnsigned(ModbusDataType::Type type)
    {
        return cDataTypes[static_cast<int>(type)].bUnsigned;
    }

    static bool isFloat(ModbusDataType::Type type)
    {
        return cDataTypes[static_cast<int>(type)].bFloat;
    }

    static QString typeString(ModbusDataType::Type type)
    {
        switch (type)
        {
        case Type::SIGNED_16:
            return "s16b";
        case Type::UNSIGNED_32:
            return "32b";
        case Type::SIGNED_32:
            return "s32b";
        case Type::FLOAT_32:
            return "f32b";
        case Type::UNSIGNED_8:
            return "u8b";
        case Type::SIGNED_8:
            return "s8b";
        case Type::UNSIGNED_16:
        default:
            return "16b";
        }
    }

    static QString description(ModbusDataType::Type type)
    {
        switch (type)
        {
        case Type::SIGNED_16:
            return "signed 16-bit";
        case Type::UNSIGNED_32:
            return "unsigned 32-bit";
        case Type::SIGNED_32:
            return "signed 32-bit";
        case Type::FLOAT_32:
            return "32-bit float";
        case Type::UNSIGNED_8:
            return "unsigned 8-bit";
        case Type::SIGNED_8:
            return "signed 8-bit";
        case Type::UNSIGNED_16:
        default:
            return "unsigned 16-bit";
        }
    }

    static Type convertSettings(bool is32bit, bool bUnsigned, bool bFloat)
    {
        if (bFloat)
        {
            return Type::FLOAT_32;
        }
        else if (bUnsigned)
        {
            return is32bit ? Type::UNSIGNED_32 : Type::UNSIGNED_16;
        }
        else
        {
            return is32bit ? Type::SIGNED_32 : Type::SIGNED_16;
        }
    }

    static Type convertString(QString strType, bool &bOk)
    {
        bOk = true;

        if (strType == "16b" || strType.isEmpty())
        {
            return Type::UNSIGNED_16;
        }
        else if (strType == "s16b")
        {
            return Type::SIGNED_16;
        }
        else if (strType == "32b")
        {
            return Type::UNSIGNED_32;
        }
        else if (strType == "s32b")
        {
            return Type::SIGNED_32;
        }
        else if (strType == "f32b")
        {
            return Type::FLOAT_32;
        }
        else if (strType == "u8b")
        {
            return Type::UNSIGNED_8;
        }
        else if (strType == "s8b")
        {
            return Type::SIGNED_8;
        }
        else
        {
            bOk = false;
            return Type::UNSIGNED_16;
        }
    }

    static Type convertMbcString(QString strType, bool &bOk)
    {
        bOk = true;

        if (
            (strType == "uint16")
            || (strType == "hex16")
            || (strType == "bin16")
            || (strType == "ascii16")
            || strType.isEmpty()
        )
        {
            return Type::UNSIGNED_16;
        }
        else if (strType == "int16")
        {
            return Type::SIGNED_16;
        }
        else if (
            (strType == "uint32")
            || (strType == "hex32")
            || (strType == "bin32")
            || (strType == "ascii32")
        )
        {
            return Type::UNSIGNED_32;
        }
        else if (strType == "int32")
        {
            return Type::SIGNED_32;
        }
        else if (strType == "float32")
        {
            return Type::FLOAT_32;
        }
        else
        {
            bOk = false;
            return Type::UNSIGNED_16;
        }
    }

private:

    struct TypeSettings
    {
        bool b32Bit;
        bool b8Bit;
        bool bUnsigned;
        bool bFloat;
    };

    static const TypeSettings cDataTypes[];
};



#endif // MODBUSDATATYPE_H
