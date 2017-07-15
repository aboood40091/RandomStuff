/*
 * Wii U 'GTX' Texture Extractor
 * Created by Ninji Vahran / Treeki; 2014-10-31
 *   ( https://github.com/Treeki )
 * Updated by AboodXD; 2017-07-14
 *   ( https://github.com/aboood40091 )
 * This software is released into the public domain.
 *
 * Special thanks to: libtxc_dxtn developers
 *
 * Tested with TDM-GCC-64 on Windows 10 Pro x64.
 *
 * How to build:
 * g++ -o gtx_extract gtx_extract.c
 *
 * Why so complex?
 * Wii U textures appear to be packed using a complex 'texture swizzling'
 * algorithm, presumably for faster access.
 *
 * TODO:
 * Implement creating GTX files.
 * Add BFLIM support.
 *
 * Feel free to throw a pull request at me if you improve it!
 */

/* General stuff and imports */
#include "txc_dxtn.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <setjmp.h>
#include <time.h>

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))


static int formats[8] = {0x1a, 0x41a, 0x31, 0x431, 0x32, 0x432, 0x33, 0x433}; // Supported formats
static int DXTn_formats[6] = {0x31, 0x431, 0x32, 0x432, 0x33, 0x433};

// isvalueinarray(): find if a certain value is in a certain array
bool isvalueinarray(int val, int *arr, int size){
	for (int i = 0; i < size; i++) {
		if (arr[i] == val)
			return true;
	}
	return false;
}


uint8_t formatHwInfo[0x40*4] =
{
	// todo: Convert to struct
	// each entry is 4 bytes
	0x00,0x00,0x00,0x01,0x08,0x03,0x00,0x01,0x08,0x01,0x00,0x01,0x00,0x00,0x00,0x01,
	0x00,0x00,0x00,0x01,0x10,0x07,0x00,0x00,0x10,0x03,0x00,0x01,0x10,0x03,0x00,0x01,
	0x10,0x0B,0x00,0x01,0x10,0x01,0x00,0x01,0x10,0x03,0x00,0x01,0x10,0x03,0x00,0x01,
	0x10,0x03,0x00,0x01,0x20,0x03,0x00,0x00,0x20,0x07,0x00,0x00,0x20,0x03,0x00,0x00,
	0x20,0x03,0x00,0x01,0x20,0x05,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x03,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x20,0x03,0x00,0x01,0x00,0x00,0x00,0x01,
	0x00,0x00,0x00,0x01,0x20,0x0B,0x00,0x01,0x20,0x0B,0x00,0x01,0x20,0x0B,0x00,0x01,
	0x40,0x05,0x00,0x00,0x40,0x03,0x00,0x00,0x40,0x03,0x00,0x00,0x40,0x03,0x00,0x00,
	0x40,0x03,0x00,0x01,0x00,0x00,0x00,0x00,0x80,0x03,0x00,0x00,0x80,0x03,0x00,0x00,
	0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x10,0x01,0x00,0x00,
	0x10,0x01,0x00,0x00,0x20,0x01,0x00,0x00,0x20,0x01,0x00,0x00,0x20,0x01,0x00,0x00,
	0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x60,0x01,0x00,0x00,
	0x60,0x01,0x00,0x00,0x40,0x01,0x00,0x01,0x80,0x01,0x00,0x01,0x80,0x01,0x00,0x01,
	0x40,0x01,0x00,0x01,0x80,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


uint32_t surfaceGetBitsPerPixel(uint32_t surfaceFormat)
{
	uint32_t hwFormat = (surfaceFormat & 0x3F) * 4;
	uint32_t bpp = formatHwInfo[hwFormat];
	return bpp;
}


/* Start of libtxc_dxtn section */
#define EXP5TO8R(packedcol)					\
   ((((packedcol) >> 8) & 0xf8) | (((packedcol) >> 13) & 0x7))

#define EXP6TO8G(packedcol)					\
   ((((packedcol) >> 3) & 0xfc) | (((packedcol) >>  9) & 0x3))

#define EXP5TO8B(packedcol)					\
   ((((packedcol) << 3) & 0xf8) | (((packedcol) >>  2) & 0x7))

#define EXP4TO8(col)						\
   ((col) | ((col) << 4))


/* inefficient. To be efficient, it would be necessary to decode 16 pixels at once */

static void dxt135_decode_imageblock ( const GLubyte *img_block_src,
                         GLint i, GLint j, GLuint dxt_type, GLvoid *texel ) {
   GLchan *rgba = (GLchan *) texel;
   const GLushort color0 = img_block_src[0] | (img_block_src[1] << 8);
   const GLushort color1 = img_block_src[2] | (img_block_src[3] << 8);
   const GLuint bits = img_block_src[4] | (img_block_src[5] << 8) |
      (img_block_src[6] << 16) | (img_block_src[7] << 24);
   /* What about big/little endian? */
   GLubyte bit_pos = 2 * (j * 4 + i) ;
   GLubyte code = (GLubyte) ((bits >> bit_pos) & 3);

   rgba[ACOMP] = CHAN_MAX;
   switch (code) {
   case 0:
      rgba[RCOMP] = UBYTE_TO_CHAN( EXP5TO8R(color0) );
      rgba[GCOMP] = UBYTE_TO_CHAN( EXP6TO8G(color0) );
      rgba[BCOMP] = UBYTE_TO_CHAN( EXP5TO8B(color0) );
      break;
   case 1:
      rgba[RCOMP] = UBYTE_TO_CHAN( EXP5TO8R(color1) );
      rgba[GCOMP] = UBYTE_TO_CHAN( EXP6TO8G(color1) );
      rgba[BCOMP] = UBYTE_TO_CHAN( EXP5TO8B(color1) );
      break;
   case 2:
      if (color0 > color1) {
         rgba[RCOMP] = UBYTE_TO_CHAN( ((EXP5TO8R(color0) * 2 + EXP5TO8R(color1)) / 3) );
         rgba[GCOMP] = UBYTE_TO_CHAN( ((EXP6TO8G(color0) * 2 + EXP6TO8G(color1)) / 3) );
         rgba[BCOMP] = UBYTE_TO_CHAN( ((EXP5TO8B(color0) * 2 + EXP5TO8B(color1)) / 3) );
      }
      else {
         rgba[RCOMP] = UBYTE_TO_CHAN( ((EXP5TO8R(color0) + EXP5TO8R(color1)) / 2) );
         rgba[GCOMP] = UBYTE_TO_CHAN( ((EXP6TO8G(color0) + EXP6TO8G(color1)) / 2) );
         rgba[BCOMP] = UBYTE_TO_CHAN( ((EXP5TO8B(color0) + EXP5TO8B(color1)) / 2) );
      }
      break;
   case 3:
      if ((dxt_type > 1) || (color0 > color1)) {
         rgba[RCOMP] = UBYTE_TO_CHAN( ((EXP5TO8R(color0) + EXP5TO8R(color1) * 2) / 3) );
         rgba[GCOMP] = UBYTE_TO_CHAN( ((EXP6TO8G(color0) + EXP6TO8G(color1) * 2) / 3) );
         rgba[BCOMP] = UBYTE_TO_CHAN( ((EXP5TO8B(color0) + EXP5TO8B(color1) * 2) / 3) );
      }
      else {
         rgba[RCOMP] = 0;
         rgba[GCOMP] = 0;
         rgba[BCOMP] = 0;
         if (dxt_type == 1) rgba[ACOMP] = UBYTE_TO_CHAN(0);
      }
      break;
   default:
   /* CANNOT happen (I hope) */
      break;
   }
}


void fetch_2d_texel_rgba_dxt1(GLint srcRowStride, const GLubyte *pixdata,
                         GLint i, GLint j, GLvoid *texel)
{
   /* Extract the (i,j) pixel from pixdata and return it
    * in texel[RCOMP], texel[GCOMP], texel[BCOMP], texel[ACOMP].
    */

   const GLubyte *blksrc = (pixdata + ((srcRowStride + 3) / 4 * (j / 4) + (i / 4)) * 8);
   dxt135_decode_imageblock(blksrc, (i&3), (j&3), 1, texel);
}


void fetch_2d_texel_rgba_dxt3(GLint srcRowStride, const GLubyte *pixdata,
                         GLint i, GLint j, GLvoid *texel) {

   /* Extract the (i,j) pixel from pixdata and return it
    * in texel[RCOMP], texel[GCOMP], texel[BCOMP], texel[ACOMP].
    */

   GLchan *rgba = (GLchan *) texel;
   const GLubyte *blksrc = (pixdata + ((srcRowStride + 3) / 4 * (j / 4) + (i / 4)) * 16);
#if 0
   /* Simple 32bit version. */
/* that's pretty brain-dead for a single pixel, isn't it? */
   const GLubyte bit_pos = 4 * ((j&3) * 4 + (i&3));
   const GLuint alpha_low = blksrc[0] | (blksrc[1] << 8) | (blksrc[2] << 16) | (blksrc[3] << 24);
   const GLuint alpha_high = blksrc[4] | (blksrc[5] << 8) | (blksrc[6] << 16) | (blksrc[7] << 24);
   dxt135_decode_imageblock(blksrc + 8, (i&3), (j&3), 2, texel);
   if (bit_pos < 32)
      rgba[ACOMP] = UBYTE_TO_CHAN( (GLubyte)(EXP4TO8((alpha_low >> bit_pos) & 15)) );
   else
      rgba[ACOMP] = UBYTE_TO_CHAN( (GLubyte)(EXP4TO8((alpha_high >> (bit_pos - 32)) & 15)) );
#endif
#if 1
/* TODO test this! */
   const GLubyte anibble = (blksrc[((j&3) * 4 + (i&3)) / 2] >> (4 * (i&1))) & 0xf;
   dxt135_decode_imageblock(blksrc + 8, (i&3), (j&3), 2, texel);
   rgba[ACOMP] = UBYTE_TO_CHAN( (GLubyte)(EXP4TO8(anibble)) );
#endif

}


void fetch_2d_texel_rgba_dxt5(GLint srcRowStride, const GLubyte *pixdata,
                         GLint i, GLint j, GLvoid *texel) {

   /* Extract the (i,j) pixel from pixdata and return it
    * in texel[RCOMP], texel[GCOMP], texel[BCOMP], texel[ACOMP].
    */

   GLchan *rgba = (GLchan *) texel;
   const GLubyte *blksrc = (pixdata + ((srcRowStride + 3) / 4 * (j / 4) + (i / 4)) * 16);
   const GLubyte alpha0 = blksrc[0];
   const GLubyte alpha1 = blksrc[1];
#if 0
   const GLubyte bit_pos = 3 * ((j&3) * 4 + (i&3));
   /* simple 32bit version */
   const GLuint bits_low = blksrc[2] | (blksrc[3] << 8) | (blksrc[4] << 16) | (blksrc[5] << 24);
   const GLuint bits_high = blksrc[6] | (blksrc[7] << 8);
   GLubyte code;
   if (bit_pos < 30)
      code = (GLubyte) ((bits_low >> bit_pos) & 7);
   else if (bit_pos == 30)
      code = (GLubyte) ((bits_low >> 30) & 3) | ((bits_high << 2) & 4);
   else
      code = (GLubyte) ((bits_high >> (bit_pos - 32)) & 7);
#endif
#if 1
/* TODO test this! */
   const GLubyte bit_pos = ((j&3) * 4 + (i&3)) * 3;
   const GLubyte acodelow = blksrc[2 + bit_pos / 8];
   const GLubyte acodehigh = blksrc[3 + bit_pos / 8];
   const GLubyte code = (acodelow >> (bit_pos & 0x7) |
      (acodehigh  << (8 - (bit_pos & 0x7)))) & 0x7;
#endif
   dxt135_decode_imageblock(blksrc + 8, (i&3), (j&3), 2, texel);
#if 0
   if (alpha0 > alpha1) {
      switch (code) {
      case 0:
         rgba[ACOMP] = UBYTE_TO_CHAN( alpha0 );
         break;
      case 1:
         rgba[ACOMP] = UBYTE_TO_CHAN( alpha1 );
         break;
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
         rgba[ACOMP] = UBYTE_TO_CHAN( ((alpha0 * (8 - code) + (alpha1 * (code - 1))) / 7) );
         break;
      }
   }
   else {
      switch (code) {
      case 0:
         rgba[ACOMP] = UBYTE_TO_CHAN( alpha0 );
         break;
      case 1:
         rgba[ACOMP] = UBYTE_TO_CHAN( alpha1 );
         break;
      case 2:
      case 3:
      case 4:
      case 5:
         rgba[ACOMP] = UBYTE_TO_CHAN( ((alpha0 * (6 - code) + (alpha1 * (code - 1))) / 5) );
         break;
      case 6:
         rgba[ACOMP] = 0;
         break;
      case 7:
         rgba[ACOMP] = CHAN_MAX;
         break;
      }
   }
#endif
/* not sure. Which version is faster? */
#if 1
/* TODO test this */
   if (code == 0)
      rgba[ACOMP] = UBYTE_TO_CHAN( alpha0 );
   else if (code == 1)
      rgba[ACOMP] = UBYTE_TO_CHAN( alpha1 );
   else if (alpha0 > alpha1)
      rgba[ACOMP] = UBYTE_TO_CHAN( ((alpha0 * (8 - code) + (alpha1 * (code - 1))) / 7) );
   else if (code < 6)
      rgba[ACOMP] = UBYTE_TO_CHAN( ((alpha0 * (6 - code) + (alpha1 * (code - 1))) / 5) );
   else if (code == 6)
      rgba[ACOMP] = 0;
   else
      rgba[ACOMP] = CHAN_MAX;
#endif
}


/* Start of GTX Extractor section */
typedef struct _GFDData {
	uint32_t numImages;
	uint32_t dim;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t numMips;
	uint32_t format;
	uint32_t aa;
	uint32_t use;
	uint32_t imageSize;
	uint32_t imagePtr;
	uint32_t mipSize;
	uint32_t mipPtr;
	uint32_t tileMode;
	uint32_t swizzle;
	uint32_t alignment;
	uint32_t pitch;
	uint32_t bpp;
	uint32_t realSize;
	uint32_t dataSize;
	uint8_t *data;
} GFDData;


typedef struct _GFDHeader {
	char magic[4];
	uint32_t size_, majorVersion, minorVersion;
	uint32_t gpuVersion, alignMode, reserved1, reserved2;
} GFDHeader;


typedef struct _GFDBlockHeader {
	char magic[4];
	uint32_t size_, majorVersion, minorVersion, type_;
	uint32_t dataSize;
	uint32_t id, typeIdx;
} GFDBlockHeader;


typedef struct _GFDSurface {
	uint32_t dim, width, height, depth;
	uint32_t numMips, format_, aa, use;
	uint32_t imageSize, imagePtr, mipSize, mipPtr;
	uint32_t tileMode, swizzle, alignment, pitch;
	uint32_t mipOffset[13];
	uint32_t mip1, numMips_, slice1, numSlices;
	uint8_t compSel[4];
	uint32_t texReg[5];
} GFDSurface;

// swap32(): swaps the endianness
uint32_t swap32(uint32_t v) {
	uint32_t a = (v & 0xFF000000) >> 24;
	uint32_t b = (v & 0x00FF0000) >> 8;
	uint32_t c = (v & 0x0000FF00) << 8;
	uint32_t d = (v & 0x000000FF) << 24;
	return a|b|c|d;
}

// swapRB(): swaps the R and B channels
uint32_t swapRB(uint32_t argb) {
	uint32_t r = (argb & 0x00FF0000) >> 16;
	uint32_t b = (argb & 0x000000FF) << 16;
	uint32_t ag = (argb & 0xFF00FF00);
	return ag|r|b;
}

// writeBMPHeader(): writes a BMP header according to the given values
void writeBMPHeader(FILE *f, int width, int height) {
	uint16_t u16;
	uint32_t u32;

	fwrite("BM", 1, 2, f);
	u32 = 122 + (width*height*4); fwrite(&u32, 1, 4, f);
	u16 = 0; fwrite(&u16, 1, 2, f);
	u16 = 0; fwrite(&u16, 1, 2, f);
	u32 = 122; fwrite(&u32, 1, 4, f);

	u32 = 108; fwrite(&u32, 1, 4, f);
	u32 = width; fwrite(&u32, 1, 4, f);
	u32 = height; fwrite(&u32, 1, 4, f);
	u16 = 1; fwrite(&u16, 1, 2, f);
	u16 = 32; fwrite(&u16, 1, 2, f);
	u32 = 3; fwrite(&u32, 1, 4, f);
	u32 = width*height*4; fwrite(&u32, 1, 4, f);
	u32 = 2835; fwrite(&u32, 1, 4, f);
	u32 = 2835; fwrite(&u32, 1, 4, f);
	u32 = 0; fwrite(&u32, 1, 4, f);
	u32 = 0; fwrite(&u32, 1, 4, f);
	u32 = 0xFF0000; fwrite(&u32, 1, 4, f);
	u32 = 0xFF00; fwrite(&u32, 1, 4, f);
	u32 = 0xFF; fwrite(&u32, 1, 4, f);
	u32 = 0xFF000000; fwrite(&u32, 1, 4, f);
	u32 = 0x57696E20; fwrite(&u32, 1, 4, f);

	uint8_t thing[0x24];
	memset(thing, 0, 0x24);
	fwrite(thing, 1, 0x24, f);

	u32 = 0; fwrite(&u32, 1, 4, f);
	u32 = 0; fwrite(&u32, 1, 4, f);
	u32 = 0; fwrite(&u32, 1, 4, f);
}


// readGTX(): reads the GTX file
int readGTX(GFDData *gfd, FILE *f) {
	GFDHeader header;

	if (fread(&header, 1, sizeof(header), f) != sizeof(header))
		return -1;

	if (memcmp(header.magic, "Gfx2", 4) != 0)
		return -2;

	gfd->numImages = 0;

	while (!feof(f)) {
		GFDBlockHeader section;
		if (fread(&section, 1, sizeof(section), f) != sizeof(section))
			break;

		if (memcmp(section.magic, "BLK{", 4) != 0)
			return -100;

		if (swap32(section.type_) == 0xB) {
			GFDSurface info;

			if (swap32(section.dataSize) != 0x9C)
				return -200;

			if (fread(&info, 1, sizeof(info), f) != sizeof(info))
				return -201;


			gfd->dim = swap32(info.dim);
			gfd->width = swap32(info.width);
			gfd->height = swap32(info.height);
			gfd->depth = swap32(info.depth);
			gfd->numMips = swap32(info.numMips);
			gfd->format = swap32(info.format_);
			gfd->aa = swap32(info.aa);
			gfd->use = swap32(info.use);
			gfd->imageSize = swap32(info.imageSize);
			gfd->imagePtr = swap32(info.imagePtr);
			gfd->mipSize = swap32(info.mipSize);
			gfd->mipPtr = swap32(info.mipPtr);
			gfd->tileMode = swap32(info.tileMode);
			gfd->swizzle = swap32(info.swizzle);
			gfd->alignment = swap32(info.alignment);
			gfd->pitch = swap32(info.pitch);
			gfd->bpp = surfaceGetBitsPerPixel(gfd->format);

		}

		else if (swap32(section.type_) == 0xC) {
			uint32_t bpp = gfd->bpp;
			bpp /= 8;

			if (isvalueinarray(gfd->format, DXTn_formats, 6))
				gfd->realSize = ((gfd->width + 3) >> 2) * ((gfd->height + 3) >> 2) * bpp;

			else
				gfd->realSize = gfd->width * gfd->height * bpp;

			gfd->dataSize = swap32(section.dataSize);
			gfd->data = (uint8_t*)malloc(gfd->dataSize);
			if (!gfd->data)
				return -300;

			if (fread(gfd->data, 1, gfd->dataSize, f) != gfd->dataSize)
				return -301;

			gfd->numImages += 1;

		}

		else {
			fseek(f, swap32(section.dataSize), SEEK_CUR);
		}
	}

	return 1;
}


/* Start of swizzling section */

/* Credits:
  -AddrLib: actual code
  -Exzap: modifying code to apply to Wii U textures
  -AboodXD: porting, code improvements and cleaning up
*/

static uint32_t m_banks = 4;
static uint32_t m_banksBitcount = 2;
static uint32_t m_pipes = 2;
static uint32_t m_pipesBitcount = 1;
static uint32_t m_pipeInterleaveBytes = 256;
static uint32_t m_pipeInterleaveBytesBitcount = 8;
static uint32_t m_rowSize = 2048;
static uint32_t m_swapSize = 256;
static uint32_t m_splitSize = 2048;

static uint32_t m_chipFamily = 2;

static uint32_t MicroTilePixels = 64;

uint32_t computeSurfaceThickness(uint32_t tileMode)
{
	uint32_t thickness = 1;

	if (tileMode == 3 || tileMode == 7 || tileMode == 11 || tileMode == 13 || tileMode == 15)
		thickness = 4;

	else if (tileMode == 16 || tileMode == 17)
		thickness = 8;

    return thickness;
}


uint32_t computePixelIndexWithinMicroTile(uint32_t x, uint32_t y, uint32_t bpp, uint32_t tileMode)
{
	uint32_t z = 0;
	uint32_t thickness;
	uint32_t pixelBit8;
	uint32_t pixelBit7;
	uint32_t pixelBit6;
	uint32_t pixelBit5;
	uint32_t pixelBit4;
	uint32_t pixelBit3;
	uint32_t pixelBit2;
	uint32_t pixelBit1;
	uint32_t pixelBit0;
	pixelBit6 = 0;
	pixelBit7 = 0;
	pixelBit8 = 0;
	thickness = computeSurfaceThickness(tileMode);

	if (bpp == 0x08) {
		pixelBit0 = x & 1;
		pixelBit1 = (x & 2) >> 1;
		pixelBit2 = (x & 4) >> 2;
		pixelBit3 = (y & 2) >> 1;
		pixelBit4 = y & 1;
		pixelBit5 = (y & 4) >> 2;
	}

	else if (bpp == 0x10) {
		pixelBit0 = x & 1;
		pixelBit1 = (x & 2) >> 1;
		pixelBit2 = (x & 4) >> 2;
		pixelBit3 = y & 1;
		pixelBit4 = (y & 2) >> 1;
		pixelBit5 = (y & 4) >> 2;
	}

	else if (bpp == 0x20 || bpp == 0x60) {
		pixelBit0 = x & 1;
		pixelBit1 = (x & 2) >> 1;
		pixelBit2 = y & 1;
		pixelBit3 = (x & 4) >> 2;
		pixelBit4 = (y & 2) >> 1;
		pixelBit5 = (y & 4) >> 2;
	}

	else if (bpp == 0x40) {
		pixelBit0 = x & 1;
		pixelBit1 = y & 1;
		pixelBit2 = (x & 2) >> 1;
		pixelBit3 = (x & 4) >> 2;
		pixelBit4 = (y & 2) >> 1;
		pixelBit5 = (y & 4) >> 2;
	}

	else if (bpp == 0x80) {
		pixelBit0 = y & 1;
		pixelBit1 = x & 1;
		pixelBit2 = (x & 2) >> 1;
		pixelBit3 = (x & 4) >> 2;
		pixelBit4 = (y & 2) >> 1;
		pixelBit5 = (y & 4) >> 2;
	}

	else {
		pixelBit0 = x & 1;
		pixelBit1 = (x & 2) >> 1;
		pixelBit2 = y & 1;
		pixelBit3 = (x & 4) >> 2;
		pixelBit4 = (y & 2) >> 1;
		pixelBit5 = (y & 4) >> 2;
	}

	if (thickness > 1) {
		pixelBit6 = z & 1;
		pixelBit7 = (z & 2) >> 1;
	}

	if (thickness == 8)
		pixelBit8 = (z & 4) >> 2;

	return ((pixelBit8 << 8) | (pixelBit7 << 7) | (pixelBit6 << 6) |
		32 * pixelBit5 | 16 * pixelBit4 | 8 * pixelBit3 |
		4 * pixelBit2 | pixelBit0 | 2 * pixelBit1);
}


uint32_t computePipeFromCoordWoRotation(uint32_t x, uint32_t y) {
	// hardcoded to assume 2 pipes
	uint32_t pipe = ((y >> 3) ^ (x >> 3)) & 1;
	return pipe;
}


uint32_t computeBankFromCoordWoRotation(uint32_t x, uint32_t y) {
	uint32_t numPipes = m_pipes;
	uint32_t numBanks = m_banks;
	uint32_t bankBit0;
	uint32_t bankBit0a;
	uint32_t bank = 0;

	if (numBanks == 4) {
		bankBit0 = ((y / (16 * numPipes)) ^ (x >> 3)) & 1;
		bank = bankBit0 | 2 * (((y / (8 * numPipes)) ^ (x >> 4)) & 1);
	}

	else if (numBanks == 8) {
		bankBit0a = ((y / (32 * numPipes)) ^ (x >> 3)) & 1;
		bank = (bankBit0a | 2 * (((y / (32 * numPipes)) ^ (y / (16 * numPipes) ^ (x >> 4))) & 1) |
        	4 * (((y / (8 * numPipes)) ^ (x >> 5)) & 1));
	}

	return bank;
}


uint32_t isThickMacroTiled(uint32_t tileMode) {
	uint32_t thickMacroTiled = 0;

	if (tileMode == 7 || tileMode == 11 || tileMode == 13 || tileMode == 15)
		thickMacroTiled = 1;

	return thickMacroTiled;
}


uint32_t isBankSwappedTileMode(uint32_t tileMode) {
	uint32_t bankSwapped = 0;

	if (tileMode == 8 || tileMode == 9 || tileMode == 10 || tileMode == 11 || tileMode == 14 || tileMode == 15)
		bankSwapped = 1;

	return bankSwapped;
}


uint32_t computeMacroTileAspectRatio(uint32_t tileMode) {
	uint32_t ratio = 1;

	if (tileMode == 5 || tileMode == 9)
		ratio = 2;

	else if (tileMode == 6 || tileMode == 10)
		ratio = 4;

	return ratio;
}


uint32_t computeSurfaceBankSwappedWidth(uint32_t tileMode, uint32_t bpp, uint32_t pitch) {
	if (isBankSwappedTileMode(tileMode) == 0)
		return 0;

	uint32_t numSamples = 1;
	uint32_t numBanks = m_banks;
	uint32_t numPipes = m_pipes;
	uint32_t swapSize = m_swapSize;
	uint32_t rowSize = m_rowSize;
	uint32_t splitSize = m_splitSize;
	uint32_t groupSize = m_pipeInterleaveBytes;
	uint32_t bytesPerSample = 8 * bpp;

	uint32_t samplesPerTile = splitSize / bytesPerSample;
	uint32_t slicesPerTile = max(1, numSamples / samplesPerTile);

	if (isThickMacroTiled(tileMode) != 0)
		numSamples = 4;

	uint32_t bytesPerTileSlice = numSamples * bytesPerSample / slicesPerTile;

	uint32_t factor = computeMacroTileAspectRatio(tileMode);
	uint32_t swapTiles = max(1, (swapSize >> 1) / bpp);

	uint32_t swapWidth = swapTiles * 8 * numBanks;
	uint32_t heightBytes = numSamples * factor * numPipes * bpp / slicesPerTile;
	uint32_t swapMax = numPipes * numBanks * rowSize / heightBytes;
	uint32_t swapMin = groupSize * 8 * numBanks / bytesPerTileSlice;

	uint32_t bankSwapWidth = min(swapMax, max(swapMin, swapWidth));

	while (bankSwapWidth >= (2 * pitch))
		bankSwapWidth >>= 1;

	return bankSwapWidth;
}


uint64_t AddrLib_computeSurfaceAddrFromCoordLinear(uint32_t x, uint32_t y, uint32_t bpp, uint32_t pitch) {
	uint32_t rowOffset = y * pitch;
	uint32_t pixOffset = x;

	uint32_t addr = (rowOffset + pixOffset) * bpp;
	addr /= 8;

	return addr;
}


uint64_t AddrLib_computeSurfaceAddrFromCoordMicroTiled(uint32_t x, uint32_t y, uint32_t bpp, uint32_t pitch, uint32_t tileMode) {
	uint64_t microTileThickness = 1;

	if (tileMode == 3)
		microTileThickness = 4;

	uint64_t microTileBytes = (MicroTilePixels * microTileThickness * bpp + 7) / 8;
	uint64_t microTilesPerRow = pitch >> 3;
	uint64_t microTileIndexX = x >> 3;
	uint64_t microTileIndexY = y >> 3;

	uint64_t microTileOffset = microTileBytes * (microTileIndexX + microTileIndexY * microTilesPerRow);

	uint64_t pixelIndex = computePixelIndexWithinMicroTile(x, y, bpp, tileMode);

	uint64_t pixelOffset = bpp * pixelIndex;

	pixelOffset >>= 3;

	return pixelOffset + microTileOffset;
}


uint64_t AddrLib_computeSurfaceAddrFromCoordMacroTiled(uint32_t x, uint32_t y, uint32_t bpp, uint32_t pitch, uint32_t height, uint32_t tileMode, uint32_t pipeSwizzle, uint32_t bankSwizzle) {
	uint64_t sampleSlice, numSamples;

	uint32_t numPipes = m_pipes;
	uint32_t numBanks = m_banks;
	uint32_t numGroupBits = m_pipeInterleaveBytesBitcount;
	uint32_t numPipeBits = m_pipesBitcount;
	uint32_t numBankBits = m_banksBitcount;

	uint32_t microTileThickness = computeSurfaceThickness(tileMode);

	uint64_t microTileBits = bpp * (microTileThickness * MicroTilePixels);
	uint64_t microTileBytes = (microTileBits + 7) / 8;

	uint64_t pixelIndex = computePixelIndexWithinMicroTile(x, y, bpp, tileMode);

	uint64_t pixelOffset = bpp * pixelIndex;

	uint64_t elemOffset = pixelOffset;

	uint64_t bytesPerSample = microTileBytes;
	if (microTileBytes <= m_splitSize) {
		numSamples = 1;
		sampleSlice = 0;
	}

	else {
		uint64_t samplesPerSlice = m_splitSize / bytesPerSample;
		uint64_t numSampleSplits = max(1, 1 / samplesPerSlice);
		numSamples = samplesPerSlice;
		sampleSlice = elemOffset / (microTileBits / numSampleSplits);
		elemOffset %= microTileBits / numSampleSplits;
	}
	elemOffset += 7;
	elemOffset /= 8;

	uint64_t pipe = computePipeFromCoordWoRotation(x, y);
	uint64_t bank = computeBankFromCoordWoRotation(x, y);

	uint64_t bankPipe = pipe + numPipes * bank;

	uint64_t swizzle_ = pipeSwizzle + numPipes * bankSwizzle;

	bankPipe ^= numPipes * sampleSlice * ((numBanks >> 1) + 1) ^ swizzle_;
	bankPipe %= numPipes * numBanks;
	pipe = bankPipe % numPipes;
	bank = bankPipe / numPipes;

	uint64_t sliceBytes = (height * pitch * microTileThickness * bpp * numSamples + 7) / 8;
	uint64_t sliceOffset = sliceBytes * (sampleSlice / microTileThickness);

	uint64_t macroTilePitch = 8 * m_banks;
	uint64_t macroTileHeight = 8 * m_pipes;

	if (tileMode == 5 || tileMode == 9) { // GX2_TILE_MODE_2D_TILED_THIN4 and GX2_TILE_MODE_2B_TILED_THIN2
		macroTilePitch >>= 1;
		macroTileHeight *= 2;
	}

	else if (tileMode == 6 || tileMode == 10) { // GX2_TILE_MODE_2D_TILED_THIN4 and GX2_TILE_MODE_2B_TILED_THIN4
		macroTilePitch >>= 2;
		macroTileHeight *= 4;
	}

	uint64_t macroTilesPerRow = pitch / macroTilePitch;
	uint64_t macroTileBytes = (numSamples * microTileThickness * bpp * macroTileHeight * macroTilePitch + 7) / 8;
	uint64_t macroTileIndexX = x / macroTilePitch;
	uint64_t macroTileIndexY = y / macroTileHeight;
	uint64_t macroTileOffset = (macroTileIndexX + macroTilesPerRow * macroTileIndexY) * macroTileBytes;

	if (tileMode == 8 || tileMode == 9 || tileMode == 10 || tileMode == 11 || tileMode == 14 || tileMode == 15) {
		static const uint32_t bankSwapOrder[] = { 0, 1, 3, 2, 6, 7, 5, 4, 0, 0 };
		uint64_t bankSwapWidth = computeSurfaceBankSwappedWidth(tileMode, bpp, pitch);
		uint64_t swapIndex = macroTilePitch * macroTileIndexX / bankSwapWidth;
		bank ^= bankSwapOrder[swapIndex & (m_banks - 1)];
	}

	uint64_t groupMask = ((1 << numGroupBits) - 1);

	uint64_t numSwizzleBits = (numBankBits + numPipeBits);

	uint64_t totalOffset = (elemOffset + ((macroTileOffset + sliceOffset) >> numSwizzleBits));

	uint64_t offsetHigh  = (totalOffset & ~groupMask) << numSwizzleBits;
	uint64_t offsetLow = groupMask & totalOffset;

	uint64_t pipeBits = pipe << numGroupBits;
	uint64_t bankBits = bank << (numPipeBits + numGroupBits);

	return bankBits | pipeBits | offsetLow | offsetHigh;
}

// writeFile(): writes the BMP file
void writeFile(FILE *f, int width, int height, uint8_t *output) {
    int row;

	writeBMPHeader(f, width, height);

    for (row = height - 1; row >= 0; row--) {
        fwrite(&output[row * width * 4], 1, width * 4, f);
    }
}

// deswizzle(): deswizzles the image
void deswizzle(GFDData *gfd, FILE *f) {
	uint64_t pos, pos_;
	uint32_t x, y, width, height;
	uint8_t *data, *result;
	uint32_t *output, outValue;
	double frac, whole;

	data = (uint8_t *)gfd->data;
	result = (uint8_t*)malloc(gfd->dataSize);

	if (isvalueinarray(gfd->format, DXTn_formats, 6)) {
		double width2 = (double)gfd->width / 4.0;
		frac = modf(width2, &whole);
		width = (uint32_t)whole;
		if (frac == 0.5)
			width += 1;

		double height2 = (double)gfd->height / 4.0;
		frac = modf(height2, &whole);
		height = (uint32_t)whole;
		if (frac == 0.5)
			height += 1;
	}

	else {
		width = gfd->width;
		height = gfd->height;
	}

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			uint32_t bpp = gfd->bpp;
			uint32_t pipeSwizzle = (gfd->swizzle >> 8) & 1;
			uint32_t bankSwizzle = (gfd->swizzle >> 9) & 3;

			if (gfd->tileMode == 0 || gfd->tileMode == 1)
				pos = AddrLib_computeSurfaceAddrFromCoordLinear(x, y, bpp, gfd->pitch);

			else if (gfd->tileMode == 2 || gfd->tileMode == 3)
				pos = AddrLib_computeSurfaceAddrFromCoordMicroTiled(x, y, bpp, gfd->pitch, gfd->tileMode);

			else
				pos = AddrLib_computeSurfaceAddrFromCoordMacroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode, pipeSwizzle, bankSwizzle);

			bpp /= 8;

			pos_ = (y * width + x) * bpp;

			for (int i = 0; i < bpp; i++) {
				if (pos + i < gfd->dataSize && pos_ + i < gfd->dataSize)
					result[pos_ + i] = data[pos + i];
			}
		}
	}

	output = (uint32_t*)malloc(gfd->width * gfd->height * 4);

    for (y = 0; y < gfd->height; y++) {
		for (x = 0; x < gfd->width; x++) {
            if (isvalueinarray(gfd->format, DXTn_formats, 6)) {
                uint8_t bits[4];

                if (gfd->format == 0x31 || gfd->format == 0x431)
                    fetch_2d_texel_rgba_dxt1(gfd->width, result, x, y, bits);
                else if (gfd->format == 0x32 || gfd->format == 0x432)
                    fetch_2d_texel_rgba_dxt3(gfd->width, result, x, y, bits);
                else if (gfd->format == 0x33 || gfd->format == 0x433)
                    fetch_2d_texel_rgba_dxt5(gfd->width, result, x, y, bits);

                outValue = (bits[ACOMP] << 24);
                outValue |= (bits[RCOMP] << 16);
                outValue |= (bits[GCOMP] << 8);
                outValue |= bits[BCOMP];
            }

            else {
                pos_ = (y * width + x) * 4;

                outValue = (result[pos_ + 3] << 24);
                outValue |= (result[pos_] << 16);
                outValue |= (result[pos_ + 1] << 8);
                outValue |= result[pos_ + 2];
            }

            output[(y * gfd->width) + x] = outValue;
		}
	}

	writeFile(f, gfd->width, gfd->height, (uint8_t *)output);

	free(result);
	free(output);
}

// remove_three(): removes the file extension from a string
char *remove_three(const char *filename) {
	size_t len = strlen(filename);
	char *newfilename = (char*)malloc(len-2);
	if (!newfilename) /* handle error */;
	memcpy(newfilename, filename, len-3);
	newfilename[len - 3] = 0;
	return newfilename;
}

// main(): the main function
int main(int argc, char **argv) {
	GFDData data;
	FILE *f;
	int result;

	printf("GTX Extractor - C++ ver.\n");
	printf("BMP ver.\n");
	printf("(C) 2014 Treeki, 2017 AboodXD\n");

	if (argc != 2) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Usage: %s [input.gtx]\n", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Supported formats:\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_TCS_R8_G8_B8_A8_UNORM\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_TCS_R8_G8_B8_A8_SRGB\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC1_UNORM\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC1_SRGB\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC2_UNORM\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC2_SRGB\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC3_UNORM\n");
		fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC3_SRGB\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Exiting in 5 seconds...\n");
		unsigned int retTime = time(0) + 5;
		while (time(0) < retTime);
		return EXIT_FAILURE;
	}

	if (!(f = fopen(argv[1], "rb"))) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Cannot open %s for reading\n", argv[1]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Exiting in 5 seconds...\n");
		unsigned int retTime = time(0) + 5;
		while (time(0) < retTime);
		return EXIT_FAILURE;
	}

	else
		printf("\nConverting: %s\n", argv[1]);

	if ((result = readGTX(&data, f)) != 1) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Error %d while parsing GTX file %s\n", result, argv[1]);
		fclose(f);
		fprintf(stderr, "\n");
		fprintf(stderr, "Exiting in 5 seconds...\n");
		unsigned int retTime = time(0) + 5;
		while (time(0) < retTime);
		return EXIT_FAILURE;
	}

	fclose(f);

	if (data.numImages > 1) {
		fprintf(stderr, "\n");
		fprintf(stderr, "This program doesn't support converting GTX files with multiple images\n"); // TODO
		fprintf(stderr, "\n");
		fprintf(stderr, "Exiting in 5 seconds...\n");
		unsigned int retTime = time(0) + 5;
		while (time(0) < retTime);
		return EXIT_FAILURE;
	}

	else if (data.numImages == 0) {
		fprintf(stderr, "\n");
		fprintf(stderr, "No images were found in this GTX file\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "Exiting in 5 seconds...\n");
		unsigned int retTime = time(0) + 5;
		while (time(0) < retTime);
		return EXIT_FAILURE;
	}

	char *str = remove_three(argv[1]);
	char c = 'b';
	char c1 = 'm';
	char c2 = 'p';

	size_t len = strlen(str);
	char *str2 = (char*)malloc(len + 3 + 1 );
	strcpy(str2, str);
	str2[len] = c;
	str2[len + 1] = c1;
	str2[len + 2] = c2;
	str2[len + 3] = '\0';

	if (!(f = fopen(str2, "wb"))) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Cannot open %s for writing\n", str2);
		fprintf(stderr, "\n");
		fprintf(stderr, "Exiting in 5 seconds...\n");
		unsigned int retTime = time(0) + 5;
		while (time(0) < retTime);
		return EXIT_FAILURE;
	}

	free(str2);

	printf("\n");
	printf("// ----- GX2Surface Info ----- \n");
	printf("  dim             = %d\n", data.dim);
	printf("  width           = %d\n", data.width);
	printf("  height          = %d\n", data.height);
	printf("  depth           = %d\n", data.depth);
	printf("  numMips         = %d\n", data.numMips);
	printf("  format          = 0x%x\n", data.format);
	printf("  aa              = %d\n", data.aa);
	printf("  use             = %d\n", data.use);
	printf("  imageSize       = %d\n", data.imageSize);
	printf("  mipSize         = %d\n", data.mipSize);
	printf("  tileMode        = %d\n", data.tileMode);
	printf("  swizzle         = %d, 0x%x\n", data.swizzle, data.swizzle);
	printf("  alignment       = %d\n", data.alignment);
	printf("  pitch           = %d\n", data.pitch);
	printf("\n");
	printf("  bits per pixel  = %d\n", data.bpp);
	printf("  bytes per pixel = %d\n", data.bpp / 8);
	printf("  realSize        = %d\n", data.realSize);

	uint32_t bpp = data.bpp;

	if (isvalueinarray(data.format, formats, 8)) {
		deswizzle(&data, f);
	}

	else {
		fprintf(stderr, "Unsupported format: 0x%x\n", data.format);
		fprintf(stderr, "\n");
		fprintf(stderr, "Exiting in 5 seconds...\n");
		unsigned int retTime = time(0) + 5;
		while (time(0) < retTime);
		return EXIT_FAILURE;
	}

	fclose(f);

	printf("\nFinished converting: %s\n", argv[1]);

	return EXIT_SUCCESS;
}
