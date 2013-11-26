/**
 * Converter between standard Bitmaps and Flight Simulator Bitmaps
 *
 * Copyright (c) Brian Chau, 2013
 *
 * me@brianchau.ca
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author information at http://www.brianchau.ca/
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define BUILD_VERSION "20131126-1.0.00080 ALPHA"

// This section defines the different types of files we will use
#define UNKN 0
#define STD_24 1
#define STD_32 2
#define FS_32 3
#define FS_DXT1 4
#define FS_DXT1A 5
#define FS_DXT3 6
#define FS_DXT5 7
#define STD_16 8
#define MASK_16 9
char* filetype[10] = {	"Undefined / other",
			"Standard 24-bit",
			"Standard 32-bit",
			"Flight Simulator 32-bit",
			"Flight Simulator DXT1 without Alpha",
			"Flight Simulator DXT1 with Alpha",
			"Flight Simulator DXT3",
			"Flight Simulator DXT5",
			"Standard 16-bit",
			"16-bit with bit masks"};

// This section defines various buffers and files used
FILE* bmpFile;
unsigned char* inputFileBuffer;
unsigned char* convertFileBuffer;
unsigned char* outputFileBuffer;
unsigned char* outputHeaderBuffer;

// ints holding input and output file type
int inputFileType;
int outputFileType;

// global variables holding image properties
unsigned int width;
unsigned int height;
unsigned int inputFileSize;
unsigned int outputFileSize;
unsigned int inputHeaderSize;
unsigned int inputBufferSize;
unsigned int convertBufferSize;
unsigned int outputHeaderSize;
unsigned int outputBufferSize;

// For 16-bit with mask
unsigned int bitmask_red;
unsigned int bitmask_green;
unsigned int bitmask_blue;
unsigned int bitmask_alpha;

// Other global variables
int inputReadSuccess;
bool mips;

void bufferWriteLittleEndianLong(unsigned char* fileBuffer, unsigned int index, unsigned long long value) {
	fileBuffer[index] = (unsigned char)(value & 0x000000ff);
	fileBuffer[index + 1] = (unsigned char)((value >> 8) & 0x000000ff);
	fileBuffer[index + 2] = (unsigned char)((value >> 16) & 0x000000ff);
	fileBuffer[index + 3] = (unsigned char)((value >> 24) & 0x000000ff);
	fileBuffer[index + 4] = (unsigned char)((value >> 32) & 0x000000ff);
	fileBuffer[index + 5] = (unsigned char)((value >> 40) & 0x000000ff);
	fileBuffer[index + 6] = (unsigned char)((value >> 48) & 0x000000ff);
	fileBuffer[index + 7] = (unsigned char)((value >> 56) & 0x000000ff);
}

void bufferWriteLittleEndianInt(unsigned char* fileBuffer, unsigned int index, unsigned int value) {
	fileBuffer[index] = (unsigned char)(value & 0x000000ff);
	fileBuffer[index + 1] = (unsigned char)((value >> 8) & 0x000000ff);
	fileBuffer[index + 2] = (unsigned char)((value >> 16) & 0x000000ff);
	fileBuffer[index + 3] = (unsigned char)((value >> 24) & 0x000000ff);
}

void bufferWriteLittleEndianShort(unsigned char* fileBuffer, unsigned int index, unsigned short value) {
	fileBuffer[index] = (unsigned char)(value & 0x000000ff);
	fileBuffer[index + 1] = (unsigned char)((value >> 8) & 0x000000ff);
}

unsigned short bufferReadLittleEndianShort(unsigned char* fileBuffer, unsigned int index) {
	return (unsigned short)fileBuffer[index]
	    + ((unsigned short)fileBuffer[index + 1] << 8);
}

unsigned int bufferReadLittleEndianInt(unsigned char* fileBuffer, unsigned int index) {
	return (unsigned int)fileBuffer[index]
	    + ((unsigned int)fileBuffer[index + 1] << 8)
	    + ((unsigned int)fileBuffer[index + 2] << 16)
	    + ((unsigned int)fileBuffer[index + 3] << 24);
}

unsigned long long bufferReadLittleEndianLong(unsigned char* fileBuffer, unsigned int index) {
	return (unsigned long long)fileBuffer[index]
	    + ((unsigned long long)fileBuffer[index + 1] << 8)
	    + ((unsigned long long)fileBuffer[index + 2] << 16)
	    + ((unsigned long long)fileBuffer[index + 3] << 24)
	    + ((unsigned long long)fileBuffer[index + 4] << 32)
	    + ((unsigned long long)fileBuffer[index + 5] << 40)
	    + ((unsigned long long)fileBuffer[index + 6] << 48)
	    + ((unsigned long long)fileBuffer[index + 7] << 56);
}

unsigned short getLittleEndianShort() {
	unsigned char intbuffer[2];
	for (int i = 0; i < 2; i++) {
		intbuffer[i] = (unsigned char)getc(bmpFile);
	}
	return (unsigned short)intbuffer[0] + ((unsigned short)intbuffer[1] << 8);
}

unsigned int getLittleEndianInt() {
	unsigned char intbuffer[4];
	for (int i = 0; i < 4; i++) {
		intbuffer[i] = (unsigned char)getc(bmpFile);
	}
	return (unsigned int)intbuffer[0] + ((unsigned int)intbuffer[1] << 8) + ((unsigned int)intbuffer[2] << 16) + ((unsigned int)intbuffer[3] << 24);
}

unsigned short findMaxDistance(unsigned char* rgb, int size, int rAvg, int gAvg, int bAvg) {
	unsigned short distance = 0;
	unsigned short temp;
	for (int i = 0; i < size; i++) {
		temp = (unsigned short)sqrt(pow((double)(rgb[3 * i] - bAvg), 2) + pow((double)(rgb[3 * i + 1] - gAvg), 2) + pow((double)(rgb[3 * i + 2] - rAvg), 2));
		if (temp > distance) distance = temp;
	}
	return distance;
}

// TODO: This function needs work
unsigned char* compress_dxt3(unsigned char* rgb, unsigned char* alpha) {
	unsigned char* to = (unsigned char*)malloc(16 * sizeof(unsigned char));
	
	// First 8 bytes are the Alpha
	// Next 8 bytes are the RGB Compressed data
	
	// Compress Alpha
	
	unsigned long long value_a = 0;
	for (int i = 15; i >= 0; i--) {
		// this method maps to closest 4-bit value (a little slower)
		value_a += ((((unsigned short)alpha[i]) + 8) / 17);
		
		// this is a faster method (each output has same-sized domain)
		//value_a += (alpha[i] >> 4);
		
		if (i == 0) break;
		value_a <<= 4;
	}
	bufferWriteLittleEndianLong(to, 0, value_a);
	
	// Compress RGB
	//1. Calculate average, max distance, and difference relative to red;
	
	unsigned int avg_r = 0;
	unsigned int avg_g = 0;
	unsigned int avg_b = 0;
	
	for (int i = 0; i < 16; i++) {
		avg_b += rgb[3 * i];
		avg_g += rgb[3 * i + 1];
		avg_r += rgb[3 * i + 2];
	}
	
	avg_r >>= 4;
	avg_g >>= 4;
	avg_b >>= 4;
	
	unsigned short maxDistance = findMaxDistance(rgb, 16, avg_r, avg_g, avg_b);
	
	int dif_r_v_r = 0;
	int dif_b_v_r = 0;
	int dif_g_v_r = 0;
	
	short dif_r;
	unsigned char dif_temp_r, dif_temp_g, dif_temp_b;
	
	for (int i = 0; i < 16; i++) {
		dif_temp_b = rgb[3 * i];
		dif_temp_g = rgb[3 * i + 1];
		dif_temp_r = rgb[3 * i + 2];
		
		dif_r = dif_temp_r - avg_r;
		
		dif_r_v_r += (dif_temp_r - avg_r) * (dif_r > 0 ? 1 : dif_r == 0 ? 0 : -1);
		dif_g_v_r += (dif_temp_g - avg_g) * (dif_r > 0 ? 1 : dif_r == 0 ? 0 : -1);
		dif_b_v_r += (dif_temp_b - avg_b) * (dif_r > 0 ? 1 : dif_r == 0 ? 0 : -1);
	}
	
	dif_r_v_r >>= 3;
	dif_g_v_r >>= 3;
	dif_b_v_r >>= 3;
	
	dif_r_v_r /= 3;
	dif_g_v_r /= 3;
	dif_b_v_r /= 3;
	
	//2. Calculate SD
	unsigned int var_r = 0;
	unsigned int var_g = 0;
	unsigned int var_b = 0;
	
	for (int i = 0; i < 16; i++) {
		var_b += (unsigned int)pow((double)(rgb[3 * i] - avg_b), 2);
		var_g += (unsigned int)pow((double)(rgb[3 * i + 1] - avg_g), 2);
		var_r += (unsigned int)pow((double)(rgb[3 * i + 2] - avg_r), 2);
	}
	
	var_r >>= 4;
	var_g >>= 4;
	var_b >>= 4;
	
	unsigned short varDistance = (unsigned short)sqrt(pow((double)(var_b), 2) + pow((double)(var_g), 2) + pow((double)(var_r), 2));
	
	unsigned int sd_r = (unsigned int)(sqrt((double)var_r));
	unsigned int sd_g = (unsigned int)(sqrt((double)var_g));
	unsigned int sd_b = (unsigned int)(sqrt((double)var_b));
	
	unsigned short sdDistance = (unsigned short)sqrt(pow((double)(sd_b), 2) + pow((double)(sd_g), 2) + pow((double)(sd_r), 2));
	
	//3. Calculate c0, c1: AVG +- 1.5 (SD * (sign of DIFF))
	
	double adjust;
	if (sdDistance == 0)
		adjust = 0;
	else
		adjust = maxDistance / sdDistance;
		//adjust = maxDistance / varDistance;
	//adjust = 1.5;
	
	short rr[2], gg[2], bb[2];
	
	/*bb[0] = (short)(avg_b + adjust * sd_b * (dif_b_v_r > 0 ? 1 : dif_b_v_r == 0 ? 0 : -1));
	gg[0] = (short)(avg_g + adjust * sd_g * (dif_g_v_r > 0 ? 1 : dif_g_v_r == 0 ? 0 : -1));
	rr[0] = (short)(avg_r + adjust * sd_r);
	
	bb[1] = (short)(avg_b - adjust * sd_b * (dif_b_v_r > 0 ? 1 : dif_b_v_r == 0 ? 0 : -1));
	gg[1] = (short)(avg_g - adjust * sd_g * (dif_g_v_r > 0 ? 1 : dif_g_v_r == 0 ? 0 : -1));
	rr[1] = (short)(avg_r - adjust * sd_r);*/
	
	bb[0] = (short)(avg_b + dif_b_v_r);
	gg[0] = (short)(avg_g + dif_g_v_r);
	rr[0] = (short)(avg_r + dif_r_v_r);
	
	bb[1] = (short)(avg_b - dif_b_v_r);
	gg[1] = (short)(avg_g - dif_g_v_r);
	rr[1] = (short)(avg_r - dif_r_v_r);
	
	if (bb[0] > 255) bb[0] = 255;
	if (gg[0] > 255) gg[0] = 255;
	if (rr[0] > 255) rr[0] = 255;
	
	if (bb[0] < 0) bb[0] = 0;
	if (gg[0] < 0) gg[0] = 0;
	if (rr[0] < 0) rr[0] = 0;
	
	if (bb[1] > 255) bb[1] = 255;
	if (gg[1] > 255) gg[1] = 255;
	if (rr[1] > 255) rr[1] = 255;
	
	if (bb[1] < 0) bb[1] = 0;
	if (gg[1] < 0) gg[1] = 0;
	if (rr[1] < 0) rr[1] = 0;
	
	unsigned short c0 = (((unsigned short)(rr[0] >> 3) & 0x1f) << 11)
			  + (((unsigned short)(gg[0] >> 2) & 0x3f) << 5)
			  + (((unsigned short)(bb[0] >> 3) & 0x1f));
	unsigned short c1 = (((unsigned short)(rr[1] >> 3) & 0x1f) << 11)
			  + (((unsigned short)(gg[1] >> 2) & 0x3f) << 5)
			  + (((unsigned short)(bb[1] >> 3) & 0x1f));
	
	int first = 0;
	int second = 1;
	
	if (c0 < c1) {
		first = 1;
		second = 0;
		// swap c0 and c1
		c0 ^= c1;
		c1 ^= c0;
		c0 ^= c1;
	}
	
	unsigned char r[4], g[4], b[4];
	
	unsigned char map[16];
	
	unsigned long long mapTo[4];
	int tempA, tempB;
	unsigned int mapping = 0;
	
	// if c0 == c1 then all 4 colors same; assign all to 0 with mapping = 0;
	if (c0 != c1) {
		b[0] = (unsigned char)bb[first];
		g[0] = (unsigned char)gg[first];
		r[0] = (unsigned char)rr[first];
		
		b[1] = (unsigned char)bb[second];
		g[1] = (unsigned char)gg[second];
		r[1] = (unsigned char)rr[second];
		
		b[2] = (2 * b[0] + b[1]) / 3;
		g[2] = (2 * g[0] + g[1]) / 3;
		r[2] = (2 * r[0] + r[1]) / 3;
		
		b[3] = (b[0] + 2 * b[1]) / 3;
		g[3] = (g[0] + 2 * g[1]) / 3;
		r[3] = (r[0] + 2 * r[1]) / 3;
		
		for (int i = 0; i < 16; i++) {
			mapTo[0] = (unsigned long long)(10000 * sqrt(pow((double)(rgb[3 * i] - bb[0]), 2)
								   + pow((double)(rgb[3 * i + 1] - gg[0]), 2)
								   + pow((double)(rgb[3 * i + 2] - rr[0]), 2)));
			mapTo[1] = (unsigned long long)(10000 * sqrt(pow((double)(rgb[3 * i] - bb[1]), 2)
								   + pow((double)(rgb[3 * i + 1] - gg[1]), 2)
								   + pow((double)(rgb[3 * i + 2] - rr[1]), 2)));
			mapTo[2] = (unsigned long long)(10000 * sqrt(pow((double)(rgb[3 * i] - bb[2]), 2)
								   + pow((double)(rgb[3 * i + 1] - gg[2]), 2)
								   + pow((double)(rgb[3 * i + 2] - rr[2]), 2)));
			mapTo[3] = (unsigned long long)(10000 * sqrt(pow((double)(rgb[3 * i] - bb[3]), 2)
								   + pow((double)(rgb[3 * i + 1] - gg[3]), 2)
								   + pow((double)(rgb[3 * i + 2] - rr[3]), 2)));
			if (mapTo[0] <= mapTo[1])
				tempA = 0;
			else
				tempA = 1;
			if (mapTo[2] <= mapTo[3])
				tempB = 2;
			else
				tempB = 3;
			if (mapTo[tempA] <= mapTo[tempB])
				map[i] = tempA;
			else
				map[i] = tempB;
		}
		
		for (int i = 15; i >= 0; i--) {
			mapping += map[i];
			if (i == 0) break;
			mapping <<= 2;
		}
	}
	
	bufferWriteLittleEndianShort(to, 8, c0);
	bufferWriteLittleEndianShort(to, 10, c1);
	bufferWriteLittleEndianInt(to, 12, mapping);
	
	return to;
}

int processFileInput() {
	inputFileType = UNKN;
	
// 1. Read Bitmap File Header
	
	if (getc(bmpFile) != 'B')
		return 1;
	if (getc(bmpFile) != 'M')
		return 1;
	
	unsigned int codedFileSize = getLittleEndianInt();
	//if (codedFileSize != inputFileSize)
	if (codedFileSize == 0)
		return 1;
	
	// skip over irrelevant stuff
	getc(bmpFile);
	getc(bmpFile);
	getc(bmpFile);
	getc(bmpFile);
	
	unsigned int startvalue = getLittleEndianInt();
	
	inputHeaderSize = startvalue;
	
// 2. Read DIB Header
	
	unsigned int DIBsize = getLittleEndianInt();
	
	unsigned short panes, bitDepth;
	unsigned int compression, palette;
	if (DIBsize == 40 || DIBsize == 56) {
		// BITMAPINFOHEADER
		// BITMAPV3INFOHEADER
		width = getLittleEndianInt();
		height = getLittleEndianInt();
		if (width != height)
			return 4;
		if (width < 4 || (width & (width - 1)) != 0)
			return 5;
		if (height < 4 || (height & (height - 1)) != 0)
			return 5;
		
		panes = getLittleEndianShort();// var declared above
		if (panes != 1)
			return 1;
		
		bitDepth = getLittleEndianShort();// var declared above
		if (bitDepth == 16)
			inputFileType = STD_16;
		else if (bitDepth == 24)
			inputFileType = STD_24;
		else if (bitDepth == 32)
			inputFileType = STD_32;
		else
			return 1;
		
		compression = getLittleEndianInt();// var declared above
		if (compression == 827611204) // DXT1
			inputFileType = FS_DXT1;
		else if (compression == 861165636) // DXT3
			inputFileType = FS_DXT3;
		else if (compression == 894720068) // DXT5
			inputFileType = FS_DXT5;
		else if (compression == 3) { // BIT FIELD
			if (DIBsize == 40)
				return 1;
		}
		else if (compression != 0)
			return 1;
		
		inputBufferSize = getLittleEndianInt();// var global
		
		// skip over irrelevant stuff
		getLittleEndianInt();
		getLittleEndianInt();
		
		palette = getLittleEndianInt();// var declared above
		if (palette != 0)
			return 1;
		
		// skip over irrelevant stuff
		getLittleEndianInt();
		
		//IF BITMAPV3INFOHEADER ONLY
		if (DIBsize == 56) {
			if (bitDepth == 16) {
				inputFileType = MASK_16;
			} else {
				inputFileType = UNKN;
				return 1;
			}
			bitmask_red = getLittleEndianInt();
			bitmask_green = getLittleEndianInt();
			bitmask_blue = getLittleEndianInt();
			bitmask_alpha = getLittleEndianInt();
			printf("A:%08x R:%08x G:%08x B:%08x\n", bitmask_alpha, bitmask_red, bitmask_green, bitmask_blue);
		}
	} else {
	// OTHER HEADERS: WILL CODE LATER
		return 1;
	}
	unsigned int currentIndex = (unsigned int)ftell(bmpFile);
	
	if (currentIndex != startvalue) {
// 3. Test if it is already a FS file format.
		if (getLittleEndianInt() == 808932166) { // "FS70" in little endian
			if (bitDepth == 32)
				inputFileType = FS_32;
			if (getLittleEndianInt() != 20)
				return 1;
			if (inputFileType < FS_32 || inputFileType > FS_DXT5)
				return 2;
			
			getc(bmpFile);
			
			char dxtType = getc(bmpFile);
			if (inputFileType == FS_DXT1) {
				if (dxtType == 2)
					inputFileType = FS_DXT1A;
				else if (dxtType != 1)
					return 2;
			} else if (dxtType != 4) {
					return 2;
			}
			
			getLittleEndianInt();
			
			if (getLittleEndianShort() != 0)
				mips = true;
			getLittleEndianInt();
			currentIndex = (unsigned int)ftell(bmpFile);
		} else {
			return 1;
		}
	}
	
// 4. Now, bmpFile is at the beginning of the data area. Test that data is not corrupt.
	if (currentIndex + inputBufferSize > inputFileSize)
		return 3;
	
// 5. OK so put file into Buffer.
	inputFileBuffer = (unsigned char*)malloc(inputBufferSize * sizeof(unsigned char));
	
	for (unsigned int i = 0; i < inputBufferSize; i++) {
		inputFileBuffer[i] = (unsigned char)getc(bmpFile);
	}
	
	return 0;
}

bool conv_24_to_32(unsigned char* from, unsigned char* to) {
#pragma omp parallel for
	for (int i = 0; i < (int)(width * height); i++) {
		to[i * 4] = from[i * 3];
		to[i * 4 + 1] = from[i * 3 + 1];
		to[i * 4 + 2] = from[i * 3 + 2];
		to[i * 4 + 3] = (char)0xff;
	}
	return true;
}

bool conv_32_to_24(unsigned char* from, unsigned char* to) {
#pragma omp parallel for
	for (int i = 0; i < (int)(width * height); i++) {
		to[i * 3] = from[i * 4];
		to[i * 3 + 1] = from[i * 4 + 1];
		to[i * 3 + 2] = from[i * 4 + 2];
	}
	return true;
}

bool conv_mask16_to_32(unsigned char* from, unsigned char* to) {
#pragma omp parallel for
	for (int i = 0; i < (int)(width * height); i++) {
		unsigned short pixelValue = from[i * 2] + (from[i * 2 + 1] << 8);
		
		if (bitmask_blue != 0)
			to[i * 4] = (char)(((pixelValue & bitmask_blue) / (1)) * 255 / (bitmask_blue / (1)));
		else
			to[i * 4] = (char)0x00;
		
		if (bitmask_green != 0)
			to[i * 4 + 1] = (char)(((pixelValue & bitmask_green) / (bitmask_blue + 1)) * 255 / (bitmask_green / (bitmask_blue + 1)));
		else
			to[i * 4 + 1] = (char)0x00;
		
		if (bitmask_red != 0)
			to[i * 4 + 2] = (char)(((pixelValue & bitmask_red) / (bitmask_green + bitmask_blue + 1)) * 255 / (bitmask_red / (bitmask_green + bitmask_blue + 1)));
		else
			to[i * 4 + 2] = (char)0x00;
		
		if (bitmask_alpha != 0)
			to[i * 4 + 3] = (char)(((pixelValue & bitmask_alpha) / (bitmask_red + bitmask_green + bitmask_blue + 1)) * 255 / (bitmask_alpha / (bitmask_red + bitmask_green + bitmask_blue + 1)));
		else
			to[i * 4 + 3] = (char)0xff;
	}
	return true;
}

bool conv_16_to_32(unsigned char* from, unsigned char* to) {
#pragma omp parallel for
	for (int i = 0; i < (int)(width * height); i++) {
		unsigned short pixelValue = from[i * 2] + (from[i * 2 + 1] << 8);
		
		to[i * 4] = (char)((pixelValue & 0x1f) * 255 / 31);
		pixelValue >>= 5;
		
		to[i * 4 + 1] = (char)((pixelValue & 0x1f) * 255 / 31);
		pixelValue >>= 5;
		
		to[i * 4 + 2] = (char)((pixelValue & 0x1f) * 255 / 31);
		to[i * 4 + 3] = (char)0xff;
	}
	return true;
}

bool conv_dxt1_to_32(unsigned char* from, unsigned char* to, bool alpha = false) {
#pragma omp parallel for
	for (int i = 0; i < (int)((width * height) >> 4); i++) {
		unsigned short c0 = bufferReadLittleEndianShort(from, i * 8);
		unsigned short c1 = bufferReadLittleEndianShort(from, i * 8 + 2);
		unsigned int codes_rgba = bufferReadLittleEndianInt(from, i * 8 + 4);
		
		unsigned int x_coord = (i % (width >> 2)) << 2;
		unsigned int y_coord = ((i << 2) / width) << 2;
		
		unsigned char a[4];
		unsigned char b[4];
		unsigned char g[4];
		unsigned char r[4];
		
		a[0] = (unsigned char)0xff;
		a[1] = (unsigned char)0xff;
		a[2] = (unsigned char)0xff;
		a[3] = (unsigned char)0xff;
		
		b[0] = (unsigned char)((c0 & 0x1f) * 255 / 31);
		g[0] = (unsigned char)(((c0 >> 5) & 0x3f) * 255 / 63);
		r[0] = (unsigned char)(((c0 >> 11) & 0x1f) * 255 / 31);
		
		b[1] = (unsigned char)((c1 & 0x1f) * 255 / 31);
		g[1] = (unsigned char)(((c1 >> 5) & 0x3f) * 255 / 63);
		r[1] = (unsigned char)(((c1 >> 11) & 0x1f) * 255 / 31);
		
		if (c0 > c1) {
			b[2] = (2 * b[0] + b[1]) / 3;
			g[2] = (2 * g[0] + g[1]) / 3;
			r[2] = (2 * r[0] + r[1]) / 3;
			
			b[3] = (b[0] + 2 * b[1]) / 3;
			g[3] = (g[0] + 2 * g[1]) / 3;
			r[3] = (r[0] + 2 * r[1]) / 3;
		} else {
			b[2] = (b[0] + b[1]) / 2;
			g[2] = (g[0] + g[1]) / 2;
			r[2] = (r[0] + r[1]) / 2;
			
			b[3] = (unsigned char)0x00;
			g[3] = (unsigned char)0x00;
			r[3] = (unsigned char)0x00;
			
			if (alpha)
				a[3] = (unsigned char)0x00;
		}
		
		unsigned int index, pixel_rgba;
		
		for (unsigned int row = 0; row < 4; row++) {
			for (unsigned int col = 0; col < 4; col++) {
				index = (((y_coord + row) * width) + x_coord + col) << 2;
				
				pixel_rgba = codes_rgba & 0x3;
				
				to[index] = b[pixel_rgba];
				to[index + 1] = g[pixel_rgba];
				to[index + 2] = r[pixel_rgba];
				to[index + 3] = a[pixel_rgba];
				
				codes_rgba >>= 2;
			}
		}
	}
	return true;
}

bool conv_dxt3_to_32(unsigned char* from, unsigned char* to) {
#pragma omp parallel for
	for (int i = 0; i < (int)((width * height) >> 4); i++) {
		unsigned long long vals_a = bufferReadLittleEndianLong(from, i * 16);
		unsigned short c0 = bufferReadLittleEndianShort(from, i * 16 + 8);
		unsigned short c1 = bufferReadLittleEndianShort(from, i * 16 + 10);
		unsigned int codes_rgb = bufferReadLittleEndianInt(from, i * 16 + 12);
		
		unsigned int x_coord = (i % (width >> 2)) << 2;
		unsigned int y_coord = ((i << 2) / width) << 2;
		
		unsigned char b[4];
		unsigned char g[4];
		unsigned char r[4];
		
		b[0] = (unsigned char)((c0 & 0x1f) * 255 / 31);
		g[0] = (unsigned char)(((c0 >> 5) & 0x3f) * 255 / 63);
		r[0] = (unsigned char)(((c0 >> 11) & 0x1f) * 255 / 31);
		
		b[1] = (unsigned char)((c1 & 0x1f) * 255 / 31);
		g[1] = (unsigned char)(((c1 >> 5) & 0x3f) * 255 / 63);
		r[1] = (unsigned char)(((c1 >> 11) & 0x1f) * 255 / 31);
		
		b[2] = (2 * b[0] + b[1]) / 3;
		g[2] = (2 * g[0] + g[1]) / 3;
		r[2] = (2 * r[0] + r[1]) / 3;
		
		b[3] = (b[0] + 2 * b[1]) / 3;
		g[3] = (g[0] + 2 * g[1]) / 3;
		r[3] = (r[0] + 2 * r[1]) / 3;
		
		unsigned int index, pixel_rgb;
		
		for (unsigned int row = 0; row < 4; row++) {
			for (unsigned int col = 0; col < 4; col++) {
				index = (((y_coord + row) * width) + x_coord + col) << 2;
				
				pixel_rgb = codes_rgb & 0x3;
				
				to[index] = b[pixel_rgb];
				to[index + 1] = g[pixel_rgb];
				to[index + 2] = r[pixel_rgb];
				to[index + 3] = (unsigned char)((vals_a & 0xf) * 17);
				
				codes_rgb >>= 2;
				vals_a >>= 4;
			}
		}
	}
	return true;
}

bool conv_dxt5_to_32(unsigned char* from, unsigned char* to) {
#pragma omp parallel for
	for (int i = 0; i < (int)((width * height) >> 4); i++) {
		unsigned char a0 = from[i * 16];
		unsigned char a1 = from[i * 16 + 1];
		unsigned long long codes_a = bufferReadLittleEndianLong(from, i * 16 + 2) & 0x0000ffffffffffffull;
		
		unsigned short c0 = bufferReadLittleEndianShort(from, i * 16 + 8);
		unsigned short c1 = bufferReadLittleEndianShort(from, i * 16 + 10);
		unsigned int codes_rgb = bufferReadLittleEndianInt(from, i * 16 + 12);
		
		unsigned int x_coord = (i % (width >> 2)) << 2;
		unsigned int y_coord = ((i << 2) / width) << 2;
		
		unsigned char a[8];
		
		a[0] = a0;
		a[1] = a1;
		
		if (a0 > a1) {
			a[2] = (6 * a0 + 1 * a1) / 7;
			a[3] = (5 * a0 + 2 * a1) / 7;
			a[4] = (4 * a0 + 3 * a1) / 7;
			a[5] = (3 * a0 + 4 * a1) / 7;
			a[6] = (2 * a0 + 5 * a1) / 7;
			a[7] = (1 * a0 + 6 * a1) / 7;
		} else {
			a[2] = (4 * a0 + 1 * a1) / 5;
			a[3] = (3 * a0 + 2 * a1) / 5;
			a[4] = (2 * a0 + 3 * a1) / 5;
			a[5] = (1 * a0 + 4 * a1) / 5;
			a[6] = (unsigned char)0x00;
			a[7] = (unsigned char)0xff;
		}
		
		unsigned char b[4];
		unsigned char g[4];
		unsigned char r[4];
		
		b[0] = (unsigned char)((c0 & 0x1f) * 255 / 31);
		g[0] = (unsigned char)(((c0 >> 5) & 0x3f) * 255 / 63);
		r[0] = (unsigned char)(((c0 >> 11) & 0x1f) * 255 / 31);
		
		b[1] = (unsigned char)((c1 & 0x1f) * 255 / 31);
		g[1] = (unsigned char)(((c1 >> 5) & 0x3f) * 255 / 63);
		r[1] = (unsigned char)(((c1 >> 11) & 0x1f) * 255 / 31);
		
		b[2] = (2 * b[0] + b[1]) / 3;
		g[2] = (2 * g[0] + g[1]) / 3;
		r[2] = (2 * r[0] + r[1]) / 3;
		
		b[3] = (b[0] + 2 * b[1]) / 3;
		g[3] = (g[0] + 2 * g[1]) / 3;
		r[3] = (r[0] + 2 * r[1]) / 3;
		
		unsigned int index, pixel_rgb, pixel_a;
		
		for (unsigned int row = 0; row < 4; row++) {
			for (unsigned int col = 0; col < 4; col++) {
				index = (((y_coord + row) * width) + x_coord + col) << 2;
				
				pixel_rgb = codes_rgb & 0x3;
				pixel_a = codes_a & 0x7;
				
				to[index] = b[pixel_rgb];
				to[index + 1] = g[pixel_rgb];
				to[index + 2] = r[pixel_rgb];
				to[index + 3] = a[pixel_a];
				
				codes_rgb >>= 2;
				codes_a >>= 3;
			}
		}
	}
	return true;
}

bool conv_32_to_dxt3(unsigned char* from, unsigned char* to) {
#pragma omp parallel for
	for (int i = 0; i < (int)((width * height) >> 4); i++) {
		unsigned int x_coord = (i % (width >> 2)) << 2;
		unsigned int y_coord = ((i << 2) / width) << 2;
		
		unsigned char* uncompressedRGB = (unsigned char*)malloc(16 * 3 * sizeof(unsigned char));
		unsigned char* uncompressedAlpha = (unsigned char*)malloc(16 * sizeof(unsigned char));
		
		unsigned int fullIndex, rgbIndex;
		
		for (unsigned int row = 0; row < 4; row++) {
			for (unsigned int col = 0; col < 4; col++) {
				fullIndex = (((y_coord + row) * width) + x_coord + col) << 2;
				rgbIndex = ((row << 2) + col) * 3;
				
				uncompressedRGB[rgbIndex] = from[fullIndex];
				uncompressedRGB[rgbIndex + 1] = from[fullIndex + 1];
				uncompressedRGB[rgbIndex + 2] = from[fullIndex + 2];
				uncompressedAlpha[((row << 2) + col)] = from[fullIndex + 3];
			}
		}
		
		unsigned char* compressedBlock = compress_dxt3(uncompressedRGB, uncompressedAlpha);
		
		for (unsigned int j = 0; j < 16; j++) {
			to[(i << 4) + j] = compressedBlock[j];
		}
		
		free(uncompressedRGB);
		free(uncompressedAlpha);
		free(compressedBlock);
	}
	return true;
}

bool initialConvertTo32() {
	convertBufferSize = width * height * 4;
	convertFileBuffer = (unsigned char*)malloc(convertBufferSize * sizeof(unsigned char));
	switch (inputFileType) {
	case STD_24:
		return conv_24_to_32(inputFileBuffer, convertFileBuffer);
	case STD_32:
	case FS_32:
		free(convertFileBuffer);
		convertFileBuffer = inputFileBuffer;
		inputFileBuffer = NULL;
		return true;
	case FS_DXT1:
		return conv_dxt1_to_32(inputFileBuffer, convertFileBuffer, false);
	case FS_DXT1A:
		return conv_dxt1_to_32(inputFileBuffer, convertFileBuffer, true);
	case FS_DXT3:
		return conv_dxt3_to_32(inputFileBuffer, convertFileBuffer);
	case FS_DXT5:
		return conv_dxt5_to_32(inputFileBuffer, convertFileBuffer);
	case STD_16:
		return conv_16_to_32(inputFileBuffer, convertFileBuffer);
	case MASK_16:
		return conv_mask16_to_32(inputFileBuffer, convertFileBuffer);
	default:
		return false;
	}
}

void makeOutputHeader_FS_dxt3() {
	outputHeaderSize = 74;
	outputFileSize = outputHeaderSize + outputBufferSize;
	outputHeaderBuffer = (unsigned char*)calloc(outputHeaderSize, sizeof(unsigned char));
	outputHeaderBuffer[0] = 'B';
	outputHeaderBuffer[1] = 'M';
	bufferWriteLittleEndianInt(outputHeaderBuffer, 2, outputFileSize);
	outputHeaderBuffer[10] = (unsigned char)0x4a; // index where image starts 74
	outputHeaderBuffer[14] = (unsigned char)0x28;
	bufferWriteLittleEndianInt(outputHeaderBuffer, 18, width);
	bufferWriteLittleEndianInt(outputHeaderBuffer, 22, height);
	outputHeaderBuffer[26] = (unsigned char)0x1;
	outputHeaderBuffer[28] = (unsigned char)0x10; // bitdepth 16
	outputHeaderBuffer[30] = 'D';
	outputHeaderBuffer[31] = 'X';
	outputHeaderBuffer[32] = 'T';
	outputHeaderBuffer[33] = '3';
	bufferWriteLittleEndianInt(outputHeaderBuffer, 34, outputBufferSize);
	
	// Flight Simulator Compatible header
	outputHeaderBuffer[54] = 'F';
	outputHeaderBuffer[55] = 'S';
	outputHeaderBuffer[56] = '7';
	outputHeaderBuffer[57] = '0';
	outputHeaderBuffer[58] = (unsigned char)0x14;
	outputHeaderBuffer[63] = (unsigned char)0x4; // This is 4 for 32-bit, DXT3, DXT5; 1 for DXT1; 2 for DXT1A
}

void makeOutputHeader_FS_32() {
	outputHeaderSize = 74;
	outputFileSize = outputHeaderSize + outputBufferSize;
	outputHeaderBuffer = (unsigned char*)calloc(outputHeaderSize, sizeof(unsigned char));
	outputHeaderBuffer[0] = 'B';
	outputHeaderBuffer[1] = 'M';
	bufferWriteLittleEndianInt(outputHeaderBuffer, 2, outputFileSize);
	outputHeaderBuffer[10] = (unsigned char)0x4a; // index where image starts 74
	outputHeaderBuffer[14] = (unsigned char)0x28;
	bufferWriteLittleEndianInt(outputHeaderBuffer, 18, width);
	bufferWriteLittleEndianInt(outputHeaderBuffer, 22, height);
	outputHeaderBuffer[26] = (unsigned char)0x1;
	outputHeaderBuffer[28] = (unsigned char)0x20; // bitdepth 32
	bufferWriteLittleEndianInt(outputHeaderBuffer, 34, outputBufferSize);
	
	// Flight Simulator Compatible header
	outputHeaderBuffer[54] = 'F';
	outputHeaderBuffer[55] = 'S';
	outputHeaderBuffer[56] = '7';
	outputHeaderBuffer[57] = '0';
	outputHeaderBuffer[58] = (unsigned char)0x14;
	outputHeaderBuffer[63] = (unsigned char)0x4; // This is 4 for 32-bit, DXT3, DXT5; 1 for DXT1; 2 for DXT1A
}

void makeOutputHeader_STD_24() {
	outputHeaderSize = 54;
	outputFileSize = outputHeaderSize + outputBufferSize;
	outputHeaderBuffer = (unsigned char*)calloc(outputHeaderSize, sizeof(unsigned char));
	outputHeaderBuffer[0] = 'B';
	outputHeaderBuffer[1] = 'M';
	bufferWriteLittleEndianInt(outputHeaderBuffer, 2, outputFileSize);
	outputHeaderBuffer[10] = (unsigned char)0x36; // index where image starts 54
	outputHeaderBuffer[14] = (unsigned char)0x28;
	bufferWriteLittleEndianInt(outputHeaderBuffer, 18, width);
	bufferWriteLittleEndianInt(outputHeaderBuffer, 22, height);
	outputHeaderBuffer[26] = (unsigned char)0x1;
	outputHeaderBuffer[28] = (unsigned char)0x18; // bitdepth 24
	bufferWriteLittleEndianInt(outputHeaderBuffer, 34, outputBufferSize);
}

bool convertToOutput() {
	switch (outputFileType) {
	case STD_24:
		outputBufferSize = width * height * 3;
		outputFileBuffer = (unsigned char*)malloc(outputBufferSize * sizeof(unsigned char));
		makeOutputHeader_STD_24();
		return conv_32_to_24(convertFileBuffer, outputFileBuffer);
	case FS_32:
		outputBufferSize = width * height * 4;
		outputFileBuffer = convertFileBuffer;
		convertFileBuffer = NULL;
		makeOutputHeader_FS_32();
		return true;
	case FS_DXT3:
		outputBufferSize = width * height;
		outputFileBuffer = (unsigned char*)malloc(outputBufferSize * sizeof(unsigned char));
		makeOutputHeader_FS_dxt3();
		return conv_32_to_dxt3(convertFileBuffer, outputFileBuffer);
	default:
		return false;
	}
}

bool writeOutputFile() {
	if (outputHeaderBuffer == NULL || outputFileBuffer == NULL)
		return false;
	for (unsigned int i = 0; i < outputHeaderSize; i++) {
		putc((int)outputHeaderBuffer[i], bmpFile);
	}
	for (unsigned int i = 0; i < outputBufferSize; i++) {
		putc((int)outputFileBuffer[i], bmpFile);
	}
	return true;
}

int main(int argc, char* argv[]) {
	printf("This program converts standard 24-bit bitmaps and Adobe Photoshop\n");
	printf("32-bit bitmaps into a 32-bit format that is recognized by Microsoft\n");
	printf("Flight Simulator.\n\n");
	printf("Copyright (c) 2013 Brian Chau.\n");
	printf("Build %s\n", BUILD_VERSION);
	printf("This is an ALPHA build; as testing is not complete, this program may be harmful\nto your computer. The developer is not responsible for any damage.\n");
	
	if (argc <= 1) {
#if defined(_WIN32) || defined(WIN32)
		printf("Drag files into the program to convert them.\n\n");
		printf("Program terminated.\n");
		system("PAUSE"); // needed for Windows to prevent the program from terminating and the command window to close
#else
		printf("Usage: %s file1 [file2 file3 ...]\n\n", argv[0]);
		printf("Program terminated.\n");
#endif
		return -1;
	}
	
	char* filename;
	
	// local variables
	char selection, sel_buffer;
	int selection_counter;
	int inputReadSuccess;
	
	// initialize some global buffers to NULL first
	inputFileBuffer = NULL;
	convertFileBuffer = NULL;
	outputFileBuffer = NULL;
	outputHeaderBuffer = NULL;
	
	for (int i = 1; i < argc; i++) {
		
		// Free buffers if they aren't freed already
		if (inputFileBuffer != NULL)
			free(inputFileBuffer);
		if (convertFileBuffer != NULL)
			free(convertFileBuffer);
		if (outputFileBuffer != NULL)
			free(outputFileBuffer);
			inputFileBuffer = NULL;
		if (outputHeaderBuffer != NULL)
			free(outputHeaderBuffer);
			inputFileBuffer = NULL;
		
		// assign to NULL
		inputFileBuffer = NULL;
		convertFileBuffer = NULL;
		outputFileBuffer = NULL;
		outputHeaderBuffer = NULL;
		
		printf("\n");
		// Set filename and print to console
		filename = argv[i];
		printf("File %d: %s:\n", i, filename);
		
		// Open the specified file and check existence
#if defined(_WIN32) || defined(WIN32)
		fopen_s(&bmpFile, filename, "rb");
#else
		bmpFile = fopen(filename, "rb");
#endif
		
		if (bmpFile == NULL) {
			// File cannot be opened or does not exist, error.
			printf("\tFile not found.\n");
			continue;
		}
		
		// get filesize
		fseek(bmpFile, 0, SEEK_END);
		inputFileSize = (unsigned int)ftell(bmpFile);
		rewind(bmpFile);
		
		// File size less than 54 (the size of the smallest header) implies corrupt
		if (inputFileSize < 54) {
			printf("\tFile invalid or corrupt.\n");
			fclose(bmpFile);
			continue;
		}
		
		// At this point, we will try to process the file
		inputReadSuccess = processFileInput();
		
		if (inputReadSuccess != 0) {
			// File was not processed properly
			switch (inputReadSuccess) {
			case 1:
				printf("\tFile invalid.\n");
				break;
			case 2:
				printf("\tFile has a Flight Simulator header but is incompatible or corrupt.\n");
				break;
			case 3:
				printf("\tFile may be corrupt.\n");
				break;
			case 4:
				printf("\tFile must have same width and height.\n");
				break;
			case 5:
				printf("\tFile dimensions must be a power of 2 and greater than 4px.\n");
				break;
			default:
				printf("\tUndefined error.\n");
				break;
			}
			fclose(bmpFile);
			continue;
		}
		
		if (inputFileType == UNKN) {
			printf("\tUnsupported filetype.\n");
			fclose(bmpFile);
			continue;
		}
		
		printf("\tRead OK.  File type: %s\n", filetype[inputFileType]);
		if (mips)
			printf("\tWarning: the original file contains mipmaps. Note that the converted image will not have mipmaps.\n");
		
		// Now we ask what file type to convert to
select:
		printf("\tConvert to what file type?\n\t\t1. Flight Simulator 32-bit\n\t\t2. Flight Simulator DXT3\n\t\t3. Standard 24-bit\n\t\t0. Do nothing.\n");
		printf("\t\tType selection then press enter:  ");
		selection_counter = 0;
#if defined(_WIN32) || defined(WIN32)
		scanf_s("%c", &selection, 1);
#else
		scanf("%c", &selection);
#endif
		sel_buffer = selection;
		while (true) {
			selection_counter++;
			if (sel_buffer == '\n' || sel_buffer == EOF)
				break;
#if defined(_WIN32) || defined(WIN32)
			scanf_s("%c", &sel_buffer, 1);
#else
			scanf("%c", &sel_buffer);
#endif
		}
		if (selection_counter != 2) {
			printf("\tError: invalid selection.\n\n");
			goto select;
		}
		
		switch (selection) {
		case '0':
			break;
		case '1':
			outputFileType = FS_32;
			break;
		case '2':
			outputFileType = FS_DXT3;
			break;
		case '3':
			outputFileType = STD_24;
			break;
		default:
			printf("\tError: invalid selection.\n\n");
			goto select;
		}
		
		if (outputFileType == inputFileType || outputFileType == UNKN) {
			printf("\tNo conversion was required.  Original file unchanged.\n");
			fclose(bmpFile);
			continue;
		}
		
		printf("\tOutput to file type: %s\n", filetype[outputFileType]);
		
		// Next we convert the file to 32-bit input
		if (!initialConvertTo32()) {
			printf("\tEncode error. Original file unchanged.\n");
			fclose(bmpFile);
			continue;
		}
		
		// Close original file
		fclose(bmpFile);
		
		// Covert to output
		if (!convertToOutput()) {
			printf("\tEncode error. Original file unchanged.\n");
			continue;
		}
		
		// Open new file for output
#if defined(_WIN32) || defined(WIN32)
		fopen_s(&bmpFile, filename, "wb");
#else
		bmpFile = fopen(filename, "wb");
#endif
		
		// Write to output file
		writeOutputFile();
		printf("\tWrite OK.\n");
		fclose(bmpFile);
	}
	
	printf("\nProgram terminated.\n");
	
#if defined(_WIN32) || defined(WIN32)
	system("PAUSE"); // needed for Windows to prevent the program from terminating and the command window to close
#endif
	
	return 0;
}
