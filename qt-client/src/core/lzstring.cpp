// =============================================================================
// lzstring — implementation
//
// Port of lz-string.js (Pieroxy, WTFPL) — only the UTF16 variants are needed
// for the FUMBBL protocol, but the core _compress/_decompress are shared.
//
// JS -> C++ mapping:
//   JS string (UTF-16 code units)      -> QString (UTF-16 code units)
//   charCodeAt(i)                      -> str[i].unicode()
//   charCode & 1 / value >> 1          -> uint32 bit ops (values are <= 16 bit)
//   dictionary (JS object/array)       -> QHash / QVector<std::optional<QString>>
//
// NOTE: JS `null` results are mapped to empty QString (the caller in the WS
// layer falls back to treating the payload as raw JSON, exactly like ffbclient).
// =============================================================================
#include "core/lzstring.h"

#include <QHash>
#include <QSet>
#include <QVector>

#include <optional>
#include <vector>

namespace fumbbl::lzstring {

namespace {

// -----------------------------------------------------------------------------
// Bit writer (LSB-first) producing UTF16 output characters (value + 32).
// -----------------------------------------------------------------------------
class Utf16BitWriter
{
public:
    explicit Utf16BitWriter(int bitsPerChar)
        : m_bitsPerChar(bitsPerChar)
    {
    }

    // Write `n` zero bits (JS: plain `dataVal <<= 1` loop).
    void writeZeros(int n)
    {
        for (int i = 0; i < n; ++i)
            writeBit(false);
    }

    // Write the lowest `n` bits of `value`, LSB first (JS: (value & 1) loop).
    void writeBitsLsbFirst(uint32_t value, int n)
    {
        for (int i = 0; i < n; ++i) {
            writeBit((value & 1u) != 0);
            value >>= 1;
        }
    }

    // Flush the last partial character with zero bits (JS: flush while loop).
    void flush()
    {
        while (!writeBit(false)) {
        }
    }

    const QString &out() const { return m_out; }

private:
    // JS: dataVal <<= 1 | bit; if (dataPos == bitsPerChar-1) push & reset.
    bool writeBit(bool bit)
    {
        m_dataVal = (m_dataVal << 1) | (bit ? 1u : 0u);
        if (m_dataPos == m_bitsPerChar - 1) {
            m_out.append(static_cast<QChar>(m_dataVal + 32));
            m_dataPos = 0;
            m_dataVal = 0;
            return true;
        }
        ++m_dataPos;
        return false;
    }

    int m_bitsPerChar;
    uint32_t m_dataVal = 0;
    int m_dataPos = 0;
    QString m_out;
};

} // namespace

QString compressToUTF16(const QString &input)
{
    // JS: compressToUTF16 = _compress(input, 15, a => f(a+32)) + " "
    Utf16BitWriter writer(/*bitsPerChar=*/15);

    QHash<QString, int> dictionary;
    QSet<QString> dictionaryToCreate;
    QString w;
    int enlargeIn = 2; // Compensate for the first entry which should not count
    int dictSize = 3;
    int numBits = 2;

    const auto emitCode = [&](const QString &word, bool isNewChar) {
        if (isNewChar) {
            const ushort ch = word[0].unicode();
            if (ch < 256) {
                writer.writeZeros(numBits);
                writer.writeBitsLsbFirst(ch, 8);
            } else {
                writer.writeBitsLsbFirst(1, numBits); // marker bit: 1 then zeros
                writer.writeBitsLsbFirst(ch, 16);
            }
            --enlargeIn;
            if (enlargeIn == 0) {
                enlargeIn = 1 << numBits;
                ++numBits;
            }
            dictionaryToCreate.remove(word);
        } else {
            writer.writeBitsLsbFirst(static_cast<uint32_t>(dictionary[word]), numBits);
        }
        --enlargeIn;
        if (enlargeIn == 0) {
            enlargeIn = 1 << numBits;
            ++numBits;
        }
    };

    for (int ii = 0; ii < input.size(); ++ii) {
        const QChar c = input.at(ii);
        if (!dictionary.contains(c)) {
            dictionary[c] = dictSize++;
            dictionaryToCreate.insert(c);
        }

        const QString wc = w + c;
        if (dictionary.contains(wc)) {
            w = wc;
        } else {
            emitCode(w, dictionaryToCreate.contains(w));
            dictionary[wc] = dictSize++;
            w = QString(c);
        }
    }

    // Output the code for w.
    if (!w.isEmpty())
        emitCode(w, dictionaryToCreate.contains(w));

    // Mark the end of the stream.
    writer.writeBitsLsbFirst(2, numBits);
    writer.flush();

    return writer.out() + QLatin1Char(' ');
}

QString decompressFromUTF16(const QString &compressed)
{
    // JS: _decompress(length, 16384, i => charCodeAt(i) - 32)
    if (compressed.isEmpty())
        return QString(); // JS: "" -> null -> ""

    const int length = compressed.size();
    constexpr int resetValue = 16384; // 2^14 — top bit of the 15-bit chars

    int val = compressed.at(0).unicode() - 32;
    int position = resetValue;
    int index = 1;

    const auto refill = [&]() {
        val = (index < length) ? (compressed.at(index).unicode() - 32) : 0;
        ++index;
    };

    // Read `n` bits, LSB first (JS: while (power != maxpower) loop).
    const auto readBits = [&](int n) {
        int bits = 0;
        for (int i = 0; i < n; ++i) {
            const bool resb = (val & position) != 0;
            position >>= 1;
            if (position == 0) {
                position = resetValue;
                refill();
            }
            if (resb)
                bits |= (1 << i);
        }
        return bits;
    };

    // JS: dictionary = [0, 1, 2] (numeric; only used as truthy markers).
    // In the reference path (c >= 3) every slot holds a real string, so
    // std::optional is a faithful stand-in. JS arrays auto-extend on
    // out-of-bounds assignment, so route writes through setEntry().
    std::vector<std::optional<QString>> dictionary(3);
    const auto setEntry = [&](std::size_t idx, const QString &s) {
        if (idx >= dictionary.size())
            dictionary.resize(idx + 1);
        dictionary[idx] = s;
    };

    int enlargeIn = 4;
    int dictSize = 4;
    int numBits = 3;
    QString w;
    QVector<QString> result;

    // First symbol: 2 bits -> 0 (8-bit char) / 1 (16-bit char) / 2 (empty).
    const int firstCode = readBits(2);
    QChar firstChar;
    switch (firstCode) {
    case 0:
        firstChar = static_cast<QChar>(readBits(8));
        break;
    case 1:
        firstChar = static_cast<QChar>(readBits(16));
        break;
    case 2:
        return QString();
    default:
        Q_UNREACHABLE();
    }

    setEntry(3, QString(firstChar));
    w = QString(firstChar);
    result.append(w);

    for (;;) {
        if (index > length)
            return QString(); // JS: return ""

        int code = readBits(numBits);
        switch (code) {
        case 0: {
            const int v = readBits(8);
            setEntry(dictSize, QString(static_cast<QChar>(v)));
            ++dictSize;
            code = dictSize - 1; // JS: c = dictSize - 1 (refer the new entry)
            --enlargeIn;
            break;
        }
        case 1: {
            const int v = readBits(16);
            setEntry(dictSize, QString(static_cast<QChar>(v)));
            ++dictSize;
            code = dictSize - 1; // JS: c = dictSize - 1 (refer the new entry)
            --enlargeIn;
            break;
        }
        case 2: {
            // End of stream: join the result.
            QString out;
            out.reserve(result.size() * 4);
            for (const QString &s : result)
                out += s;
            return out;
        }
        default:
            break;
        }

        if (enlargeIn == 0) {
            enlargeIn = 1 << numBits;
            ++numBits;
        }

        QString entry;
        if (code < static_cast<int>(dictionary.size()) && dictionary[code]) {
            entry = *dictionary[code];
        } else if (code == dictSize) {
            entry = w + w.at(0);
        } else {
            return QString(); // JS: return null (invalid stream)
        }

        result.append(entry);

        // Add w + entry[0] to the dictionary.
        setEntry(dictSize, w + entry.at(0));
        ++dictSize;
        --enlargeIn;
        w = entry;

        if (enlargeIn == 0) {
            enlargeIn = 1 << numBits;
            ++numBits;
        }
    }
}

} // namespace fumbbl::lzstring
