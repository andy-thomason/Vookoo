////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: png image format decoder
//
// This tiny decoder supports the most common subsets of the PNG standard.
//
// It does not do:
//   Interleaved PNG files (now obsolete).
//   Paletted PNG files
//   Bits per pixel != 8
// 
// The decoder is thread safe and should be able to run on multiple
// threads sharing a common decoder core.

#ifndef VKU_PNGDECODER_INCLUDED
#define VKU_PNGDECODER_INCLUDED

#include <vector>
#include <cstdint>
#include <exception>
#include <vku/zipDecoder.hpp>

namespace vku {

  class pngDecoder {
    enum { debug = 0 };
  public:
    struct result_t {
      std::vector<std::uint8_t> bytes;
      std::uint32_t width = 0;
      std::uint32_t height = 0;
      char colourType;
      char bitDepth;
    };

    static inline std::uint8_t u1(const char *p) {
      const unsigned char *q = (const unsigned char *)p;
      std::uint32_t res = q[0];
      return res;
    }

    static inline std::uint16_t u2(const char *p) {
      const unsigned char *q = (const unsigned char *)p;
      std::uint32_t res = q[1] + q[0] * 0x100;
      return res;
    }

    static inline std::uint32_t u4(const char *p) {
      const unsigned char *q = (const unsigned char *)p;
      std::uint32_t res = q[3] + q[2] * 0x100 + q[1] * 0x10000 + q[0] * 0x1000000;
      return res;
    }

    static inline std::uint64_t u8(const char *p) {
      return ((std::uint64_t)u4(p) << 32) | u4(p+4);
    }

    pngDecoder() : zip() {
    }

    // Decode a PNG image from memory to memory.
    // Note: This is thread safe.
    void decode(result_t &result, const char *begin, const char *end) const {
      const char *p = begin;
      if (p + 7 >= end) error("pngDecode: file too short");
      std::uint64_t hdr = u8(begin);
      if (hdr != 0x89504e470d0a1a0aull) error("pngDecoder: bad header");
      p += 8;
      std::vector<uint8_t> idats;
      char compression = 0;
      char filter = 0;
      char interlace = 0;
      while (p + 3 < end) {
        std::uint32_t len = u4(p);
        std::uint32_t tag = u4(p+4);
        p += 8;
        //printf("%08x %08x\n", (uint32_t)len, (uint32_t)tag);
        switch (tag) {
          case 0x49484452: { // IHDR
            result.width = u4(p);
            result.height = u4(p+4);
            result.bitDepth = p[8];
            result.colourType = p[9];
            compression = p[10];
            filter = p[11];
            interlace = p[12];
          } break;
          case 0x70485973: { // PHYS
          } break;
          case 0x74494d45: { // TIME
          } break;
          case 0x69545874: { // iTXt
          } break;
          case 0x49444154: { // IDAT
            idats.insert(idats.end(), p, p + len);
          } break;
          case 0x49454e44: { // IEND
          } break;
        }
        p += len;
        std::uint32_t crc = u4(p);
        p += 4;
      }

      if (result.bitDepth != 8) error("unsupported png format (bitDepth != 8)");
      if (result.colourType == 3) error("unsupported png format (indexed palette)");
      if (filter != 0) error("bad png format (filter)");
      if (interlace != 0) error("unsupported png format (interlace)");
      size_t chans = 0;
      switch (result.colourType) {
        case 0: chans = 1; break; // Greyscale
        case 2: chans = 3; break; // True colour
        case 3: chans = 3; error("unsupported png format (indexed palette)");
        case 4: chans = 2; break; // Greyscale with Alpha
        case 6: chans = 4; break; // True colour with Alpha
      }
      result.bytes.resize((1 + result.width * chans) * result.height);
      std::uint8_t *dest = result.bytes.data();
      size_t dest_size = result.bytes.size();
      const std::uint8_t *src = idats.data();
      size_t src_size = idats.size();

      // zlib header
      std::uint8_t cmf = src[0];
      std::uint8_t flags = src[1];
      if (cmf % 16 != 8) error("bad png format (compression != 8)");
      if (flags & 0x20) error("bad png format (zlib preset)");
      if ((cmf * 256 + flags) % 31) error("bad png format (flags check fail)");
      src += 2;
      zip.decode(dest, dest + dest_size, src, src + src_size);

      // https://www.w3.org/TR/PNG-Filters.html
      std::uint8_t *dp = dest;
      const std::uint8_t *sp = dest;
      size_t num_bytes = result.width * chans;
      size_t bpp = chans * result.bitDepth / 8;
      for (uint32_t y = 0; y != result.height; ++y) {
        switch (*sp++) {
          case 0: {
            for (size_t i = 0; i != num_bytes; ++i) {
              *dp++ = *sp++;
            }
          } break;
          case 1: {
            for (size_t i = 0; i != bpp; ++i) {
              *dp++ = *sp++;
            }
            for (size_t i = bpp; i != num_bytes; ++i) {
              int a = dp[0-bpp];
              *dp++ = *sp++ + a;
            }
          } break;
          case 2: {
            if (y == 0) {
              for (size_t i = 0; i != num_bytes; ++i) {
                *dp++ = *sp++;
              }
            } else {
              for (size_t i = 0; i != num_bytes; ++i) {
                int b = dp[0-num_bytes];
                *dp++ = *sp++ + b;
              }
            }
          } break;
          case 3: {
            // a = left, b = above, c = upper left
            auto paeth = [](int a, int b, int c) {
              int p = a + b - c;
              int pa = std::abs(p - a);
              int pb = std::abs(p - b);
              int pc = std::abs(p - c);
              return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            };
            if (y == 0) {
              for (size_t i = 0; i != num_bytes; ++i) {
                int a = i < bpp ? 0 : dp[0-bpp];
                *dp++ = *sp++ + paeth(a, 0, 0);
              }
            } else {
              for (size_t x = 0; x != bpp; ++x) {
                int b = dp[0-num_bytes];
                *dp++ = *sp++ + paeth(0, b, 0);
              }
              for (size_t i = bpp; i != num_bytes; ++i) {
                int a = dp[0-bpp];
                int b = dp[0-num_bytes];
                int c = dp[0-num_bytes-bpp];
                *dp++ = *sp++ + paeth(a, b, c);
              }
            }
          } break;
        }
      }
      result.bytes.resize(dp - dest);
    }
  private:
    void error(const char *what) const {
      throw (std::runtime_error(what));
    }
    zipDecoder zip;
    std::vector<std::uint8_t> zipBytes;
  };

} // vku

#endif
