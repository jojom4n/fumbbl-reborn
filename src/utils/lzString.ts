// =============================================================================
// LZString Compression Utility
// Reference: ffbclient network.ts, jervis-ffb LZString.kt
//
// NOTE: jervis-ffb (Kotlin client) uses direct JSON serialization without
// LZString compression. This utility is provided for compatibility with
// ffbclient protocol which uses LZString compression.
//
// For production use, consider installing the official lz-string package:
//   npm install lz-string
// =============================================================================

// -----------------------------------------------------------------------------
// Lazy-loaded lz-string functions
// These functions will use the lz-string package if available,
// otherwise they return the input unchanged
// -----------------------------------------------------------------------------

/**
 * Compress a string to UTF16 format
 * Uses the lz-string package if available, otherwise returns the input unchanged
 */
export function compressToUTF16(input: string): string {
  if (input === '') return '';
  try {
    // @ts-expect-error lz-string is an optional dependency
    if (typeof lzString !== 'undefined' && lzString.compressToUTF16) {
      // @ts-expect-error lz-string is an optional dependency
      return lzString.compressToUTF16(input);
    }
  } catch {
    // Package not available, fall through to identity
  }
  return input;
}

/**
 * Decompress a UTF16-encoded string
 * Uses the lz-string package if available, otherwise returns the input unchanged
 */
export function decompressFromUTF16(input: string): string {
  if (input === '') return '';
  try {
    // @ts-expect-error lz-string is an optional dependency
    if (typeof lzString !== 'undefined' && lzString.decompressFromUTF16) {
      // @ts-expect-error lz-string is an optional dependency
      return lzString.decompressFromUTF16(input);
    }
  } catch {
    // Package not available, fall through to identity
  }
  return input;
}

/**
 * Compress a string to base64 format
 * Uses the lz-string package if available, otherwise returns the input unchanged
 */
export function compressToBase64(input: string): string {
  if (input === '') return '';
  try {
    // @ts-expect-error lz-string is an optional dependency
    if (typeof lzString !== 'undefined' && lzString.compressToBase64) {
      // @ts-expect-error lz-string is an optional dependency
      return lzString.compressToBase64(input);
    }
  } catch {
    // Package not available, fall through to identity
  }
  return input;
}

/**
 * Decompress a base64-encoded string
 * Uses the lz-string package if available, otherwise returns the input unchanged
 */
export function decompressFromBase64(input: string): string {
  if (input === '') return '';
  try {
    // @ts-expect-error lz-string is an optional dependency
    if (typeof lzString !== 'undefined' && lzString.decompressFromBase64) {
      // @ts-expect-error lz-string is an optional dependency
      return lzString.decompressFromBase64(input);
    }
  } catch {
    // Package not available, fall through to identity
  }
  return input;
}

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

/**
 * Check if a string appears to be LZString compressed (UTF16 format)
 */
export function isCompressed(input: string): boolean {
  if (!input || input.length < 3) {
    return false;
  }
  // UTF16 compressed strings start with a space
  return input.charAt(0) === ' ';
}

/**
 * Smart decompress - auto-detects if compression is needed
 */
export function smartDecompress(input: string): string {
  if (!input || input.length === 0) {
    return '';
  }
  if (isCompressed(input)) {
    return decompressFromUTF16(input);
  }
  return input;
}

// -----------------------------------------------------------------------------
// Exported API object
// -----------------------------------------------------------------------------

export const LZString = {
  /** Compress to UTF16 */
  compressToUTF16,
  /** Decompress from UTF16 */
  decompressFromUTF16,
  /** Compress to base64 */
  compressToBase64,
  /** Decompress from base64 */
  decompressFromBase64,
  /** Check if string is compressed */
  isCompressed,
  /** Smart decompress with auto-detection */
  smartDecompress,
};