/* Bra86.c -- Converter for x86 code (BCJ)
2017-04-03 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "Bra.h"

#define Test86MSByte(b)  ((((b) + 1) & 0xFE) == 0)

/**
  [TEMPLATE] - Provide a function description!

  Function overview/purpose.

  Anything a caller should be aware of must be noted in the description.

  All parameters must be described. Parameter names must be Pascal case.

  @retval must be used and each unique return code should be clearly
  described. Providing "Others" is only acceptable if a return code
  is bubbled up from a function called internal to this function. However,
  that's usually not helpful. Try to provide explicit values that mean
  something to the caller.

  Examples:
  @param[in]      ParameterName         Brief parameter description.
  @param[out]     ParameterName         Brief parameter description.
  @param[in,out]  ParameterName         Brief parameter description.

  @retval   EFI_SUCCESS                 Brief return code description.

**/
SizeT
x86_Convert (
  Byte *data, SizeT size, UInt32 ip, UInt32 *state, int encoding
  )
{
  SizeT   pos  = 0;
  UInt32  mask = *state & 7;

  if (size < 5) {
    return 0;
  }

  size -= 4;
  ip   += 5;

  for ( ; ;) {
    Byte        *p     = data + pos;
    const Byte  *limit = data + size;
    for ( ; p < limit; p++) {
      if ((*p & 0xFE) == 0xE8) {
        break;
      }
    }

    {
      SizeT  d = (SizeT)(p - data - pos);
      pos = (SizeT)(p - data);
      if (p >= limit) {
        *state = (d > 2 ? 0 : mask >> (unsigned)d);
        return pos;
      }

      if (d > 2) {
        mask = 0;
      } else {
        mask >>= (unsigned)d;
        if (mask != 0 && (mask > 4 || mask == 3 || Test86MSByte (p[(size_t)(mask >> 1) + 1]))) {
          mask = (mask >> 1) | 4;
          pos++;
          continue;
        }
      }
    }

    if (Test86MSByte (p[4])) {
      UInt32  v   = ((UInt32)p[4] << 24) | ((UInt32)p[3] << 16) | ((UInt32)p[2] << 8) | ((UInt32)p[1]);
      UInt32  cur = ip + (UInt32)pos;
      pos += 5;
      if (encoding) {
        v += cur;
      } else {
        v -= cur;
      }

      if (mask != 0) {
        unsigned  sh = (mask & 6) << 2;
        if (Test86MSByte ((Byte)(v >> sh))) {
          v ^= (((UInt32)0x100 << sh) - 1);
          if (encoding) {
            v += cur;
          } else {
            v -= cur;
          }
        }

        mask = 0;
      }

      p[1] = (Byte)v;
      p[2] = (Byte)(v >> 8);
      p[3] = (Byte)(v >> 16);
      p[4] = (Byte)(0 - ((v >> 24) & 1));
    } else {
      mask = (mask >> 1) | 4;
      pos++;
    }
  }
}
