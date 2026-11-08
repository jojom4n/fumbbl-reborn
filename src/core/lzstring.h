// =============================================================================
// lzstring — LZString UTF16 codec (port of pieroxy/lz-string, WTFPL)
//
// The FUMBBL WebSocket protocol compresses ALL messages with LZString
// "UTF16" mode (15 bits/char, char + 32 offset, trailing space). This is a
// faithful C++/Qt port of pieroxy/lz-string (v1.5.0) `_compress`/
// `_decompress`, so streams are byte-identical to the FFB Java server's.
// =============================================================================
#pragma once

#include <QString>

namespace fumbbl::lzstring {

// Compress `input` to the UTF16 LZString format (matches JS compressToUTF16,
// including the trailing space).
QString compressToUTF16(const QString &input);

// Decompress a UTF16 LZString stream (matches JS decompressFromUTF16).
// Returns an empty QString for empty input or invalid streams.
QString decompressFromUTF16(const QString &compressed);

} // namespace fumbbl::lzstring
