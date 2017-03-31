////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016-2017
//
// 


#ifndef ANDYZIP_DEFLATE_ENCODER_INCLUDED
#define ANDYZIP_DEFLATE_ENCODER_INCLUDED

#include <algorithm>
#include <andyzip/suffix_array.hpp>

namespace andyzip {
  class deflate_encoder {
  public:
    deflate_encoder() {
    }

    bool encode(uint8_t *dest, uint8_t *dest_max, const uint8_t *src, const uint8_t *src_max) const {
      size_t block_size = 0x20000;
      FILE *log = fopen("1.txt", "wb");

      auto todot = [](char x) { return x < ' ' || x` >= 0x7f ? '.' : x; };

      while (src != src_max) {
        size_t size = std::min((size_t)(src_max - src), block_size);
        suffix_array<uint8_t, uint32_t> sa(src, src + size);
        for (size_t i = 0; i != size; ++i) {
          //size_t i = sa.rank(j);
          size_t addr = sa.addr(i);
          char buf[12];
          size_t k = 0;
          for (size_t j = addr; k != 10 && j != size; ++j) {
            buf[k++] = todot(src[j]);
          }
          buf[k] = 0;
          char bwt = addr == 0 ? '$' : todot(src[addr-1]) ;
          fprintf(log, "%8d [%8d] %c<%s> %5d\n", i, sa.addr(i), bwt, buf, sa.lcp(i));
        }
        break;
        src += size;
      }
      return false;
    }
  private:
  };
}

#endif
