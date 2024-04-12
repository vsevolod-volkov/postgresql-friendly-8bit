#include "utf8_util.h"
#include "trace.h"
#include <memory.h>

typedef struct {
	int encoding;
   SingleByteMap singleByteMap;
	Byte4Map byte4Map;
} EncodingPatch;

#define macCodePagePatchCount 10
static EncodingPatch* codePagePatches[macCodePagePatchCount] = { NULL };

const char* add_codepage_patch(int encoding, unsigned char byte, pg_wchar unicode) {
   int hole = -1;
   EncodingPatch *patch = NULL;
   for(size_t i = 0; i < macCodePagePatchCount ; i++) {
      patch = codePagePatches[i];
      if( !patch ) {
         if( hole < 0 )
            hole = i;
         continue;
      }

      if( patch->encoding == encoding ) {
         break;
      } else {
         patch = NULL;
      }
   }

   if( !patch ) {
      if( hole < 0 )
         return "Codepage patch count limit reached.";
      
      patch = malloc(sizeof(EncodingPatch));
      patch->encoding = encoding;
      memset(&patch->singleByteMap, 0, sizeof(SingleByteMap));
      memset(&patch->byte4Map, 0, sizeof(Byte4Map));
      codePagePatches[hole] = patch;
   }

   patch->singleByteMap[(size_t) byte] = unicode;

   const unsigned char byte1 = unicode & 0xff;
   const unsigned char byte2 = (unicode >> 8) & 0xff;
   const unsigned char byte3 = (unicode >> 16) & 0xff;
   const unsigned char byte4 = (unicode >> 24) & 0xff;
   
   Byte3Map *byte3Map = patch->byte4Map[byte4];

   if( !byte3Map ) {
      trace(4, "Add byte 3 map for byte 4: %02X", (int) byte4);
      byte3Map = malloc(sizeof(Byte3Map));
      memset(byte3Map, 0, sizeof(Byte3Map));
      patch->byte4Map[byte4] = byte3Map;
   }

   Byte2Map *byte2Map = (*byte3Map)[byte3];

   if( !byte2Map ) {
      trace(4, "Add byte 2 map for byte 3: %02X, byte 4: %02X", (int) byte3, (int) byte4);
      byte2Map = malloc(sizeof(Byte2Map));
      memset(byte2Map, 0, sizeof(Byte2Map));
      (*byte3Map)[byte3] = byte2Map;
   }

   Byte1Map *byte1Map = (*byte2Map)[byte2];

   if( !byte1Map ) {
      trace(4, "Add byte 1 map for byte 2: %02X, byte 3: %02X, byte 4: %02X", (int) byte2, (int) byte3, (int) byte4);
      byte1Map = malloc(sizeof(Byte1Map));
      memset(byte1Map, 0, sizeof(Byte1Map));
      (*byte2Map)[byte2] = byte1Map;
   }

   (*byte1Map)[byte1] = byte;
   return NULL;
}

EncodingPatch * const getEncodingPatch(int encoding) {
   for(size_t i = 0; i < macCodePagePatchCount ; i++) {
      EncodingPatch *patch = codePagePatches[i];

      if( patch && patch->encoding == encoding ) {
         return patch;
      }
   }
   return NULL;
}

Byte4Map * const get_utf8_to_byte_patch(int encoding) {
   EncodingPatch * const patch = getEncodingPatch(encoding);
   return patch ? &patch->byte4Map : NULL;
}

SingleByteMap * const get_byte_to_utf8_patch(int encoding) {
   EncodingPatch * const patch = getEncodingPatch(encoding);
   return patch ? &patch->singleByteMap : NULL;
}

pg_wchar conv_byte_to_utf8(SingleByteMap * const map, unsigned char byte) {
   return (*map)[byte];
}

unsigned char conv_utf8_to_byte(Byte4Map * const map, pg_wchar unicode) {
   const unsigned char byte4 = (unicode >> 24) & 0xff;

   Byte3Map *byte3Map = (*map)[byte4];

   if( !byte3Map ) {
      return 0;
   }

   const unsigned char byte3 = (unicode >> 16) & 0xff;
   
   Byte2Map *byte2Map = (*byte3Map)[byte3];

   if( !byte2Map ) {
      return 0;
   }

   const unsigned char byte2 = (unicode >> 8) & 0xff;
   
   Byte1Map *byte1Map = (*byte2Map)[byte2];

   if( !byte1Map ) {
      return 0;
   }

   const unsigned char byte1 = unicode & 0xff;

   return (*byte1Map)[byte1];
}

void destroy_codepages() {
   for(size_t i = 0; i < macCodePagePatchCount ; i++) {
      EncodingPatch *patch = codePagePatches[i];

      if( !patch ) continue;

      for(size_t b4=0; b4 < 256; b4++ ) {
         if( !patch->byte4Map[b4] ) continue;
         for(size_t b3=0; b3 < 256; b3++ ) {
            const Byte3Map *byte3Map = patch->byte4Map[b3];
            if( !byte3Map ) continue;
            for(size_t b2=0; b2 < 256; b2++ ) {
               const Byte2Map *byte2Map = (*byte3Map)[b2];
               if( !byte2Map ) continue;
               for(size_t b1=0; b1 < 256; b1++ ) {
                  const Byte1Map *byte1Map = (*byte2Map)[b1];
                  if( !byte1Map ) continue;
                  free((void*) byte1Map);
               }
            }
         }
      }
   }
}