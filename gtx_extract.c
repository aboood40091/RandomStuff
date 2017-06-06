/*
 * Wii U 'GTX' Texture Extractor
 * Created by Ninji Vahran / Treeki; 2014-10-31
 *   ( https://github.com/Treeki )
 * Updated by AboodXD; 2017-05-21
 *   ( https://github.com/aboood40091 )
 * This software is released into the public domain.
 *
 * Tested with TDM-GCC-64 on Windows 10 Pro x64.
 *
 * How to build:
 * gcc -m64 -o gtx_extract gtx_extract.c
 * (You need an x64 version of gcc!)
 *
 * Why so complex?
 * Wii U textures appear to be packed using a complex 'texture swizzling'
 * algorithm, presumably for faster access.
 *
 * TODO:
 * Make a x86 version (Would probably force me to rewrite the program?)
 *
 * Feel free to throw a pull request at me if you improve it!
 */

/* General stuff and imports */
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


static int formats[] = {0x1a, 0x41a, 0x19, 0x8, 0xa, 0xb, 0x1, 0x7, 0x2, 0x31, 0x431, 0x32, 0x432, 0x33, 0x433, 0x34, 0x234, 0x35, 0x235}; // Supported formats
static int BCn_formats[10] = {0x31, 0x431, 0x32, 0x432, 0x33, 0x433, 0x34, 0x234, 0x35, 0x235};


bool isvalueinarray(int val, int *arr, int size){
    int i;
    for (i=0; i < size; i++) {
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
	uint32_t hwFormat = surfaceFormat&0x3F;
	uint32_t bpp = formatHwInfo[hwFormat*4+0];
	return bpp;
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
	uint32_t _40, _44, _48, _4C;
	uint32_t _50, _54, _58, _5C;
	uint32_t _60, _64, _68, _6C;
	uint32_t _70, _74, _78, _7C;
	uint32_t _80, _84, _88, _8C;
	uint32_t _90, _94, _98;
} GFDSurface;

uint32_t swap32(uint32_t v) {
	uint32_t a = (v & 0xFF000000) >> 24;
	uint32_t b = (v & 0x00FF0000) >> 8;
	uint32_t c = (v & 0x0000FF00) << 8;
	uint32_t d = (v & 0x000000FF) << 24;
	return a|b|c|d;
}

/* Start of DDS writer section */

/*
 * Copyright © 2016-2017 AboodXD
 *
 * Supported formats:
    -RGBA8
    -RGB10A2
    -RGB565
    -RGB5A1
    -RGBA4
    -L8
    -L8A8
    -L4A4
    -BC1_UNORM
    -BC2_UNORM
    -BC3_UNORM
    -BC4_UNORM
    -BC4_SNORM
    -BC5_UNORM
    -BC5_SNORM
 *
 * Feel free to include this in your own program if you want, just give credits. :)
 */

void writeHeader(FILE *f, uint32_t num_mipmaps, uint32_t w, uint32_t h, uint32_t format_, bool compressed) {
    uint32_t fmtbpp = 0;
    uint32_t has_alpha;
    uint32_t rmask = 0;
    uint32_t gmask = 0;
    uint32_t bmask = 0;
    uint32_t amask = 0;
    uint32_t flags;
    uint32_t pflags;
    uint32_t caps;
    uint32_t size;
    uint32_t u32;

    if (format_ == 28) { // RGBA8
        fmtbpp = 4;
        has_alpha = 1;
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
    }

    else if (format_ == 24) { // RGB10A2
        fmtbpp = 4;
        has_alpha = 1;
        rmask = 0x000003ff;
        gmask = 0x000ffc00;
        bmask = 0x3ff00000;
        amask = 0xc0000000;
    }

    else if (format_ == 85) { // RGB565
        fmtbpp = 2;
        has_alpha = 0;
        rmask = 0x0000001f;
        gmask = 0x000007e0;
        bmask = 0x0000f800;
        amask = 0x00000000;
    }

    else if (format_ == 86) { // RGB5A1
        fmtbpp = 2;
        has_alpha = 1;
        rmask = 0x0000001f;
        gmask = 0x000003e0;
        bmask = 0x00007c00;
        amask = 0x00008000;
    }

    else if (format_ == 115) { // RGBA4
        fmtbpp = 2;
        has_alpha = 1;
        rmask = 0x0000000f;
        gmask = 0x000000f0;
        bmask = 0x00000f00;
        amask = 0x0000f000;
    }

    else if (format_ == 61) { // L8
        fmtbpp = 1;
        has_alpha = 0;
        rmask = 0x000000ff;
        gmask = 0x000000ff;
        bmask = 0x000000ff;
        amask = 0x00000000;
    }

    else if (format_ == 49) { // L8A8
        fmtbpp = 2;
        has_alpha = 1;
        rmask = 0x000000ff;
        gmask = 0x000000ff;
        bmask = 0x000000ff;
        amask = 0x0000ff00;
    }

    else if (format_ == 112) { // L4A4
        fmtbpp = 1;
        has_alpha = 1;
        rmask = 0x0000000f;
        gmask = 0x0000000f;
        bmask = 0x0000000f;
        amask = 0x000000f0;
    }

    fmtbpp <<= 3;

    flags = (0x00000001) | (0x00001000) | (0x00000004) | (0x00000002);

    caps = (0x00001000);

    if (num_mipmaps == 0)
        num_mipmaps = 1;
    if (num_mipmaps != 1) {
        flags |= (0x00020000);
        caps |= ((0x00000008) | (0x00400000));
    }

    if (!(compressed)) {
        flags |= (0x00000008);

        if (fmtbpp == 1 || format_ == 49) // LUMINANCE
            pflags = (0x00020000);

        else // RGB
            pflags = (0x00000040);

        if (has_alpha != 0)
            pflags |= (0x00000001);

        size = (w * fmtbpp);
    }

    else {
        flags |= (0x00080000);
        pflags = (0x00000004);

        size = ((w + 3) >> 2) * ((h + 3) >> 2);
        if (format_ == 71 || format_ == 80 || format_ == 81)
            size *= 8;
        else
            size *= 16;
    }

    fwrite("DDS ", 1, 4, f);
    u32 = 124; fwrite(&u32, 1, 4, f);
    fwrite(&flags, 1, 4, f);
    fwrite(&h, 1, 4, f);
    fwrite(&w, 1, 4, f);
    fwrite(&size, 1, 4, f);
    u32 = 0; fwrite(&u32, 1, 4, f);
    fwrite(&num_mipmaps, 1, 4, f);

    uint8_t thing[0x2C];
	memset(thing, 0, 0x2C);
	fwrite(thing, 1, 0x2C, f);

	u32 = 32; fwrite(&u32, 1, 4, f);
    fwrite(&pflags, 1, 4, f);

    if (!(compressed)) {
        u32 = 0; fwrite(&u32, 1, 4, f);
    }
    else {
        if (format_ == 71)
            fwrite("DXT1", 1, 4, f);
        else if (format_ == 74)
            fwrite("DXT3", 1, 4, f);
        else if (format_ == 77)
            fwrite("DXT5", 1, 4, f);
        else if (format_ == 80)
            fwrite("BC4U", 1, 4, f);
        else if (format_ == 81)
            fwrite("BC4S", 1, 4, f);
        else if (format_ == 83)
            fwrite("BC5U", 1, 4, f);
        else if (format_ == 84)
            fwrite("BC5S", 1, 4, f);
    }

    fwrite(&fmtbpp, 1, 4, f);
    fwrite(&rmask, 1, 4, f);
    fwrite(&gmask, 1, 4, f);
    fwrite(&bmask, 1, 4, f);
    fwrite(&amask, 1, 4, f);
    fwrite(&caps, 1, 4, f);

    uint8_t thing2[0x10];
	memset(thing2, 0, 0x10);
	fwrite(thing2, 1, 0x10, f);
}

uint32_t cal_pitch(uint32_t width, uint32_t format_, uint32_t org_pitch) {
    uint32_t bpp = surfaceGetBitsPerPixel(format_) >> 3;
    double frac, whole;

    if (isvalueinarray(format_, BCn_formats, 10)) {
        double width2 = (double)width / 4.0;
        frac = modf(width2, &whole);
        width = (uint32_t)whole;
        if (frac == 0.5)
            width += 1;
    }

    uint32_t pitch = 1;
    uint32_t z = 1;
    while ((pitch < width) || (pitch < org_pitch)) {
        pitch = bpp*z;
        z += 1;
    }

    if (pitch < 1)
        pitch = 1;

    return pitch;
}

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
            gfd->pitch = cal_pitch(gfd->width, gfd->format, swap32(info.pitch));
            gfd->bpp = surfaceGetBitsPerPixel(gfd->format);

		} else if (swap32(section.type_) == 0xC) {
		    uint32_t bpp = gfd->bpp;
            bpp /= 8;
		    if (isvalueinarray(gfd->format, BCn_formats, 10))
                gfd->realSize = ((gfd->width + 3) >> 2) * ((gfd->height + 3) >> 2) * bpp;

		    else
                gfd->realSize = gfd->width * gfd->height * bpp;

            gfd->dataSize = swap32(section.dataSize);
            gfd->data = malloc(gfd->dataSize);
			if (!gfd->data)
				return -300;

			if (fread(gfd->data, 1, gfd->dataSize, f) != gfd->dataSize)
				return -301;

            gfd->numImages += 1;

		} else {
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

static uint32_t MicroTilePixels = 8 * 8;

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
        bank = bankBit0a | 2 * (((y / (32 * numPipes)) ^ (y / (16 * numPipes) ^ (x >> 4))) & 1) | 4 * (((y / (8 * numPipes)) ^ (x >> 5)) & 1);
    }

    return bank;
}

uint32_t computeSurfaceRotationFromTileMode(uint32_t tileMode) {
    uint32_t pipes = m_pipes;
    uint32_t result = 0;

    if (tileMode == 4 || tileMode == 5 || tileMode == 6 || tileMode == 7 || tileMode == 8 || tileMode == 9 || tileMode == 10 || tileMode == 11)
        result = pipes * ((m_banks >> 1) - 1);

    else if (tileMode == 12 || tileMode == 13 || tileMode == 14 || tileMode == 15) {
        if (pipes >= 4)
            result = (pipes >> 1) - 1;

        else
            result = 1;
    }

    return result;
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

uint64_t AddrLib_computeSurfaceAddrFromCoordLinear(uint32_t x, uint32_t y, uint32_t bpp, uint32_t pitch, uint32_t height) {
    uint32_t rowOffset = y * pitch;
    uint32_t pixOffset = x;

    uint32_t addr = (rowOffset + pixOffset) * bpp;
    addr /= 8;

    return addr;
}

uint64_t AddrLib_computeSurfaceAddrFromCoordMicroTiled(uint32_t x, uint32_t y, uint32_t bpp, uint32_t pitch, uint32_t height, uint32_t tileMode) {
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
    uint32_t numPipes = m_pipes;
    uint32_t numBanks = m_banks;
    uint32_t numGroupBits = m_pipeInterleaveBytesBitcount;
    uint32_t numPipeBits = m_pipesBitcount;
    uint32_t numBankBits = m_banksBitcount;

    uint32_t microTileThickness = computeSurfaceThickness(tileMode);

    uint64_t pixelIndex = computePixelIndexWithinMicroTile(x, y, bpp, tileMode);

    uint64_t elemOffset = (bpp * pixelIndex) >> 3;

    uint64_t pipe = computePipeFromCoordWoRotation(x, y);
    uint64_t bank = computeBankFromCoordWoRotation(x, y);

    uint64_t bankPipe = pipe + numPipes * bank;
    uint64_t rotation = computeSurfaceRotationFromTileMode(tileMode);

    bankPipe %= numPipes * numBanks;
    pipe = bankPipe % numPipes;
    bank = bankPipe / numPipes;

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
    uint64_t macroTileBytes = (microTileThickness * bpp * macroTileHeight * macroTilePitch + 7) / 8;
    uint64_t macroTileIndexX = x / macroTilePitch;
    uint64_t macroTileIndexY = y / macroTileHeight;
    uint64_t macroTileOffset = macroTileBytes * (macroTileIndexX + macroTilesPerRow * macroTileIndexY);

    if (tileMode == 8 || tileMode == 9 || tileMode == 10 || tileMode == 11 || tileMode == 14 || tileMode == 15) {
        static const uint32_t bankSwapOrder[] = { 0, 1, 3, 2, 6, 7, 5, 4, 0, 0 };
        uint64_t bankSwapWidth = computeSurfaceBankSwappedWidth(tileMode, bpp, pitch);
        uint64_t swapIndex = macroTilePitch * macroTileIndexX / bankSwapWidth;
        bank ^= bankSwapOrder[swapIndex & (m_banks - 1)];
    }

    uint64_t group_mask = (1 << numGroupBits) - 1;
    uint64_t total_offset = elemOffset + (macroTileOffset >> (numBankBits + numPipeBits));

    uint64_t offset_high = (total_offset & ~(group_mask)) << (numBankBits + numPipeBits);
    uint64_t offset_low = total_offset & group_mask;
    uint64_t bank_bits = bank << (numPipeBits + numGroupBits);
    uint64_t pipe_bits = pipe << numGroupBits;

    return bank_bits | pipe_bits | offset_low | offset_high;
}

void writeFile(FILE *f, GFDData *gfd, uint8_t *output) {
	int y;
	uint32_t format;

	if (gfd->format == 0x1a || gfd->format == 0x41a)
        format = 28;
    else if (gfd->format == 0x19)
        format = 24;
    else if (gfd->format == 0x8)
        format = 85;
    else if (gfd->format == 0xa)
        format = 86;
    else if (gfd->format == 0xb)
        format = 115;
    else if (gfd->format == 0x1)
        format = 61;
    else if (gfd->format == 0x7)
        format = 49;
    else if (gfd->format == 0x2)
        format = 112;
    else if (gfd->format == 0x31 || gfd->format == 0x431)
        format = 71;
    else if (gfd->format == 0x32 || gfd->format == 0x432)
        format = 74;
    else if (gfd->format == 0x33 || gfd->format == 0x433)
        format = 77;
    else if (gfd->format == 0x34)
        format = 80;
    else if (gfd->format == 0x234)
        format = 81;
    else if (gfd->format == 0x35)
        format = 83;
    else if (gfd->format == 0x235)
        format = 84;

	writeHeader(f, 1, gfd->width, gfd->height, format, isvalueinarray(gfd->format, BCn_formats, 10));

	uint32_t bpp = gfd->bpp;
	bpp /= 8;

	if (isvalueinarray(gfd->format, BCn_formats, 10)) {
        bpp /= 2;
	    bpp = max(1, bpp);
	}

    for (y = 0; y < gfd->height; y++) {
        if ((y * gfd->width * bpp) >= gfd->realSize)
            break;

        fwrite(&output[y * gfd->width * bpp], 1, gfd->width * bpp, f);
    }
}

void swizzle_8(GFDData *gfd, FILE *f) {
	uint64_t pos;
	uint32_t x, y, width, height;
	uint8_t *source, *output;
	double frac, whole;

	source = (uint8_t *)gfd->data;
	output = malloc(gfd->dataSize);

	if (isvalueinarray(gfd->format, BCn_formats, 10)) {
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
                pos = AddrLib_computeSurfaceAddrFromCoordLinear(x, y, bpp, gfd->pitch, gfd->height);
            else if (gfd->tileMode == 2 || gfd->tileMode == 3)
                pos = AddrLib_computeSurfaceAddrFromCoordMicroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode);
            else
                pos = AddrLib_computeSurfaceAddrFromCoordMacroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode, pipeSwizzle, bankSwizzle);

            bpp /= 8;

			output[y * width + x] = source[pos / bpp];
		}
	}

	writeFile(f, gfd, (uint8_t *)output);

	free(output);
}

void swizzle_16(GFDData *gfd, FILE *f) {
	uint64_t pos;
	uint32_t x, y, width, height;
	uint16_t *source, *output;
	double frac, whole;

	source = (uint16_t *)gfd->data;
	output = malloc(gfd->dataSize);

	if (isvalueinarray(gfd->format, BCn_formats, 10)) {
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
                pos = AddrLib_computeSurfaceAddrFromCoordLinear(x, y, bpp, gfd->pitch, gfd->height);
            else if (gfd->tileMode == 2 || gfd->tileMode == 3)
                pos = AddrLib_computeSurfaceAddrFromCoordMicroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode);
            else
                pos = AddrLib_computeSurfaceAddrFromCoordMacroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode, pipeSwizzle, bankSwizzle);

            bpp /= 8;

			output[y * width + x] = source[pos / bpp];
		}
	}

	writeFile(f, gfd, (uint8_t *)output);

	free(output);
}

void swizzle_32(GFDData *gfd, FILE *f) {
	uint64_t pos;
	uint32_t x, y, width, height;
	uint32_t *source, *output;
	double frac, whole;

	source = (uint32_t *)gfd->data;
	output = malloc(gfd->dataSize);

	if (isvalueinarray(gfd->format, BCn_formats, 10)) {
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
                pos = AddrLib_computeSurfaceAddrFromCoordLinear(x, y, bpp, gfd->pitch, gfd->height);
            else if (gfd->tileMode == 2 || gfd->tileMode == 3)
                pos = AddrLib_computeSurfaceAddrFromCoordMicroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode);
            else
                pos = AddrLib_computeSurfaceAddrFromCoordMacroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode, pipeSwizzle, bankSwizzle);

            bpp /= 8;

			output[y * width + x] = source[pos / bpp];
		}
	}

	writeFile(f, gfd, (uint8_t *)output);

	free(output);
}

void swizzle_64(GFDData *gfd, FILE *f) {
	uint64_t pos;
	uint32_t x, y, width, height;
	uint64_t *source, *output;
	double frac, whole;

	source = (uint64_t *)gfd->data;
	output = malloc(gfd->dataSize);

	if (isvalueinarray(gfd->format, BCn_formats, 10)) {
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
                pos = AddrLib_computeSurfaceAddrFromCoordLinear(x, y, bpp, gfd->pitch, gfd->height);
            else if (gfd->tileMode == 2 || gfd->tileMode == 3)
                pos = AddrLib_computeSurfaceAddrFromCoordMicroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode);
            else
                pos = AddrLib_computeSurfaceAddrFromCoordMacroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode, pipeSwizzle, bankSwizzle);

            bpp /= 8;

			output[y * width + x] = source[pos / bpp];
		}
	}

	writeFile(f, gfd, (uint8_t *)output);

	free(output);

}

void swizzle_128(GFDData *gfd, FILE *f) {
	uint64_t pos;
	uint32_t x, y, width, height;
	__uint128_t *source, *output;
	double frac, whole;

	source = (__uint128_t *)gfd->data;
	output = malloc(gfd->dataSize);

	if (isvalueinarray(gfd->format, BCn_formats, 10)) {
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
                pos = AddrLib_computeSurfaceAddrFromCoordLinear(x, y, bpp, gfd->pitch, gfd->height);
            else if (gfd->tileMode == 2 || gfd->tileMode == 3)
                pos = AddrLib_computeSurfaceAddrFromCoordMicroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode);
            else
                pos = AddrLib_computeSurfaceAddrFromCoordMacroTiled(x, y, bpp, gfd->pitch, gfd->height, gfd->tileMode, pipeSwizzle, bankSwizzle);

            bpp /= 8;

			output[y * width + x] = source[pos / bpp];
		}
	}

	writeFile(f, gfd, (uint8_t *)output);

	free(output);
}

char *remove_three(const char *filename) {
    size_t len = strlen(filename);
    char *newfilename = malloc(len-2);
    if (!newfilename) /* handle error */;
    memcpy(newfilename, filename, len-3);
    newfilename[len - 3] = 0;
    return newfilename;
}

int main(int argc, char **argv) {
	GFDData data;
	FILE *f;
	int result;

	printf("GTX Extractor - C ver.\n");
    printf("(C) 2014 Treeki, 2017 AboodXD\n");

	if (argc != 2) {
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage: %s [input.gtx]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Supported formats:\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TCS_R8_G8_B8_A8_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TCS_R8_G8_B8_A8_SRGB\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TCS_R10_G10_B10_A2_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TCS_R5_G6_B5_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TC_R5_G5_B5_A1_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TC_R4_G4_B4_A4_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TC_R8_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TC_R8_G8_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_TC_R4_G4_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC1_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC1_SRGB\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC2_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC2_SRGB\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC3_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC3_SRGB\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC4_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC4_SNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC5_UNORM\n");
        fprintf(stderr, " - GX2_SURFACE_FORMAT_T_BC5_SNORM\n");
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
    char c = 'd';
    char c1 = 'd';
    char c2 = 's';

    size_t len = strlen(str);
    char *str2 = malloc(len + 3 + 1 );
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

	if (isvalueinarray(data.format, formats, 19)) {
        if (bpp == 8)
                swizzle_8(&data, f);
        else if (bpp == 16)
                swizzle_16(&data, f);
        else if (bpp == 32)
                swizzle_32(&data, f);
        else if (bpp == 64)
                swizzle_64(&data, f);
        else if (bpp == 128)
                swizzle_128(&data, f);
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

