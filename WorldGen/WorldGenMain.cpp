#include <iostream>
#include <Windows.h>
#include <algorithm>
#include <memory>
#include <math.h>

#include "WGGeneratorSettings.h"
#include "WGGenerator.h"
#include "WGFloatData.h"

#include "FastNoise.h"

using namespace std;

void HSVtoRGB(float& fR, float& fG, float& fB, float& fH, float& fS, float& fV) {
	float fC = fV * fS; // Chroma
	float fHPrime = fmod(fH / 60.0f, 6);
	float fX = fC * (1 - fabs(fmod(fHPrime, 2.0f) - 1.0f));
	float fM = fV - fC;

	if (0 <= fHPrime && fHPrime < 1) {
		fR = fC;
		fG = fX;
		fB = 0;
	} else if (1 <= fHPrime && fHPrime < 2) {
		fR = fX;
		fG = fC;
		fB = 0;
	} else if (2 <= fHPrime && fHPrime < 3) {
		fR = 0;
		fG = fC;
		fB = fX;
	} else if (3 <= fHPrime && fHPrime < 4) {
		fR = 0;
		fG = fX;
		fB = fC;
	} else if (4 <= fHPrime && fHPrime < 5) {
		fR = fX;
		fG = 0;
		fB = fC;
	} else if (5 <= fHPrime && fHPrime < 6) {
		fR = fC;
		fG = 0;
		fB = fX;
	} else {
		fR = 0;
		fG = 0;
		fB = 0;
	}

	fR += fM;
	fG += fM;
	fB += fM;
}

void SaveBitmapToFile(BYTE* pBitmapBits,
	LONG lWidth,
	LONG lHeight,
	WORD wBitsPerPixel,
	const unsigned long& padding_size,
	LPCTSTR lpszFileName)
{
	// Some basic bitmap parameters  
	unsigned long headers_size = sizeof(BITMAPFILEHEADER) +
		sizeof(BITMAPINFOHEADER);

	unsigned long pixel_data_size = lHeight * ((lWidth * (wBitsPerPixel / 8)) + padding_size);

	BITMAPINFOHEADER bmpInfoHeader = { 0 };

	// Set the size  
	bmpInfoHeader.biSize = sizeof(BITMAPINFOHEADER);

	// Bit count  
	bmpInfoHeader.biBitCount = wBitsPerPixel;

	// Use all colors  
	bmpInfoHeader.biClrImportant = 0;

	// Use as many colors according to bits per pixel  
	bmpInfoHeader.biClrUsed = 0;

	// Store as un Compressed  
	bmpInfoHeader.biCompression = BI_RGB;

	// Set the height in pixels  
	bmpInfoHeader.biHeight = lHeight;

	// Width of the Image in pixels  
	bmpInfoHeader.biWidth = lWidth;

	// Default number of planes  
	bmpInfoHeader.biPlanes = 1;

	// Calculate the image size in bytes  
	bmpInfoHeader.biSizeImage = pixel_data_size;

	BITMAPFILEHEADER bfh = { 0 };

	// This value should be values of BM letters i.e 0x4D42  
	// 0x4D = M 0×42 = B storing in reverse order to match with endian  
	bfh.bfType = 0x4D42;
	//bfh.bfType = 'B'+('M' << 8); 

	// <<8 used to shift ‘M’ to end  */  

	// Offset to the RGBQUAD  
	bfh.bfOffBits = headers_size;

	// Total size of image including size of headers  
	bfh.bfSize = headers_size + pixel_data_size;

	// Create the file in disk to write  
	HANDLE hFile = CreateFile(lpszFileName,
		GENERIC_WRITE,
		0,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	// Return if error opening file  
	if (!hFile) return;

	DWORD dwWritten = 0;

	// Write the File header  
	WriteFile(hFile,
		&bfh,
		sizeof(bfh),
		&dwWritten,
		NULL);

	// Write the bitmap info header  
	WriteFile(hFile,
		&bmpInfoHeader,
		sizeof(bmpInfoHeader),
		&dwWritten,
		NULL);

	// Write the RGB Data  
	WriteFile(hFile,
		pBitmapBits,
		bmpInfoHeader.biSizeImage,
		&dwWritten,
		NULL);

	// Close the file handle  
	CloseHandle(hFile);
}

void SaveHeightmapData(WG::FloatData* data) {
	BYTE* buffer = new BYTE[data->size * 3 * data->size];
	int bOff = 0;

	float samp = 0.0f;
	for (int y = (data->size - 1); y >= 0; y--) {
		for (int x = 0; x < data->size; x++) {
			samp = data->getValue(x,y);

			buffer[bOff] = (BYTE)(std::floorf(samp * 255.0f)); //B
			buffer[bOff + 1] = (BYTE)(std::floorf(samp * 255.0f)); //G
			buffer[bOff + 2] = (BYTE)(std::floorf(samp * 255.0f)); //R
			bOff += 3;
		}
	}

	SaveBitmapToFile((BYTE*)buffer, data->size, data->size, 24, 0, ".\\height.bmp");
}

void SaveNormalData(vector3* data) {
	BYTE* buffer = new BYTE[512 * 3 * 512];
	int bOff = 0;

	vector3 samp;
	for (int y = (512 - 1); y >= 0; y--) {
		for (int x = 0; x < 512; x++) {
			samp = data[x * 512 + y];

			buffer[bOff] = (BYTE) (samp.z * 255.0f); //B
			buffer[bOff + 1] = (BYTE)(samp.y * 255.0f); //G
			buffer[bOff + 2] = (BYTE)(samp.x * 255.0f); //R
			bOff += 3;
		}
	}

	SaveBitmapToFile((BYTE*)buffer, 512, 512, 24, 0, ".\\normals.bmp");
}

void SaveWaterData(WG::ByteData* data) {
	int size = data->size;

	BYTE* buffer = new BYTE[size * 3 * size];
	int bOff = 0;

	uint8_t samp = 0;
	for (int y = (size - 1); y >= 0; y--) {
		for (int x = 0; x < size; x++) {
			samp = data->getValue(x,y);
			if (samp == 1)
				buffer[bOff] = (BYTE)128; //B
			else if (samp == 2)
				buffer[bOff] = (BYTE)255; //B
			else
				buffer[bOff] = (BYTE)0; //B
			
			buffer[bOff + 1] = (BYTE)0; //G
			buffer[bOff + 2] = (BYTE)0; //R
			
			bOff += 3;
		}
	}

	SaveBitmapToFile((BYTE*)buffer, size, size, 24, 0, ".\\water.bmp");
}

void SaveTemperatureData(WG::FloatData* data, WG::FloatData* height) {
	int size = data->size;
	BYTE* buffer = new BYTE[size * 3 * size];
	int bOff = 0;

	float samp = 0.0f;
	float r = 0.0f, g = 0.0f, b = 0.0f, h = 0.0f, s = 0.0f, v=0.0f;
	for (int y = (size - 1); y >= 0; y--) {
		for (int x = 0; x < size; x++) {
			samp = data->getValue(x,y);

			s = 1.0f;
			v = (0.5f + (height->getValue(x,y) * 0.5f));
			h = ((1.0f - samp) * 250.0f);
			HSVtoRGB(r, g, b, h, s, v);

			buffer[bOff] = (BYTE)(std::floorf(b * 255.0f)); //B
			buffer[bOff + 1] = (BYTE)(std::floorf(g * 255.0f)); //G
			buffer[bOff + 2] = (BYTE)(std::floorf(r * 255.0f)); //R
			bOff += 3;
		}
	}

	SaveBitmapToFile((BYTE*)buffer, size, size, 24, 0, ".\\temperature.bmp");
}

void SaveMoistureData(WG::FloatData* data) {
	BYTE* buffer = new BYTE[data->size * 3 * data->size];
	int bOff = 0;

	float samp = 0.0f;
	for (int y = (data->size - 1); y >= 0; y--) {
		for (int x = 0; x < data->size; x++) {
			samp = data->getValue(x, y);

			buffer[bOff] = (BYTE)(std::floorf(samp * 255.0f)); //B
			buffer[bOff + 1] = (BYTE)0; //G
			buffer[bOff + 2] = (BYTE)0; //R
			bOff += 3;
		}
	}

	SaveBitmapToFile((BYTE*)buffer, data->size, data->size, 24, 0, ".\\moisture.bmp");
}

void SaveBiomeData(WG::ByteData* data) {
	BYTE* buffer = new BYTE[data->size * 3 * data->size];
	int bOff = 0;

	uint8_t samp ;
	for (int y = (data->size - 1); y >= 0; y--) {
		for (int x = 0; x < data->size; x++) {
			samp = data->getValue(x,y);
			switch (samp) {
			case WG::Generator::BIOME_OCEAN:
				buffer[bOff] = (BYTE)255; //B
				buffer[bOff + 1] = (BYTE)64; //G
				buffer[bOff + 2] = (BYTE)0; //R
				break;
			case WG::Generator::BIOME_SUBTROPICAL_DESERT:
				buffer[bOff] = (BYTE)5; //B
				buffer[bOff + 1] = (BYTE)99; //G
				buffer[bOff + 2] = (BYTE)226; //R
				break;
			case WG::Generator::BIOME_GRASSLAND:
				buffer[bOff] = (BYTE)64; //B
				buffer[bOff + 1] = (BYTE)210; //G
				buffer[bOff + 2] = (BYTE)99; //R
				break;
			case WG::Generator::BIOME_TROPICAL_FOREST:
				buffer[bOff] = (BYTE)111; //B
				buffer[bOff + 1] = (BYTE)211; //G
				buffer[bOff + 2] = (BYTE)99; //R
				break;
			case WG::Generator::BIOME_TROPICAL_RAIN_FOREST:
				buffer[bOff] = (BYTE)114; //B
				buffer[bOff + 1] = (BYTE)196; //G
				buffer[bOff + 2] = (BYTE)2; //R
				break;
			case WG::Generator::BIOME_TEMPERATE_DESERT:
				buffer[bOff] = (BYTE)23; //B
				buffer[bOff + 1] = (BYTE)185; //G
				buffer[bOff + 2] = (BYTE)232; //R
				break;
			case WG::Generator::BIOME_DECIDEOUS_FOREST:
				buffer[bOff] = (BYTE)26; //B
				buffer[bOff + 1] = (BYTE)176; //G
				buffer[bOff + 2] = (BYTE)40; //R
				break;
			case WG::Generator::BIOME_TEMPERATE_RAIN_FOREST:
				buffer[bOff] = (BYTE)14; //B
				buffer[bOff + 1] = (BYTE)222; //G
				buffer[bOff + 2] = (BYTE)3; //R
				break;
			case WG::Generator::BIOME_SHRUBLAND:
				buffer[bOff] = (BYTE)131; //B
				buffer[bOff + 1] = (BYTE)193; //G
				buffer[bOff + 2] = (BYTE)85; //R
				break;
			case WG::Generator::BIOME_TAIGA:
				buffer[bOff] = (BYTE)33; //B
				buffer[bOff + 1] = (BYTE)88; //G
				buffer[bOff + 2] = (BYTE)40; //R
				break;
			case WG::Generator::BIOME_SCORCHED:
				buffer[bOff] = (BYTE)72; //B
				buffer[bOff + 1] = (BYTE)75; //G
				buffer[bOff + 2] = (BYTE)46; //R
				break;
			case WG::Generator::BIOME_BARE:
				buffer[bOff] = (BYTE)82; //B
				buffer[bOff + 1] = (BYTE)139; //G
				buffer[bOff + 2] = (BYTE)105; //R
				break;
			case WG::Generator::BIOME_TUNDRA:
				buffer[bOff] = (BYTE)164; //B
				buffer[bOff + 1] = (BYTE)176; //G
				buffer[bOff + 2] = (BYTE)149; //R
				break;
			case WG::Generator::BIOME_ARCTIC:
				buffer[bOff] = (BYTE)255; //B
				buffer[bOff + 1] = (BYTE)255; //G
				buffer[bOff + 2] = (BYTE)255; //R
				break;
			default:
				buffer[bOff] = (BYTE)0; //B
				buffer[bOff + 1] = (BYTE)0; //G
				buffer[bOff + 2] = (BYTE)0; //R
			}
			bOff += 3;
		}
	}

	SaveBitmapToFile((BYTE*)buffer, data->size, data->size, 24, 0, ".\\biomes.bmp");
}

void SaveCompoundData(WG::Generator* gen, int size) {
	BYTE* buffer = new BYTE[size * 3 * size];
	int bOff = 0;

	float sampH, sampT;
	float r = 0.0f, g = 0.0f, b = 0.0f, h = 0.0f, s = 0.0f, v = 0.0f;
	uint8_t sampW;
	for (int y = (size - 1); y >= 0; y--) {
		for (int x = 0; x < size; x++) {
			sampH = gen->getHeightData()->getValue(x, y);
			sampT = gen->getTemperatureData()->getValue(x, y);
			sampW = gen->getWaterData()->getValue(x, y);

			if (sampW == 1) {
				buffer[bOff] = (BYTE)128; //B
				buffer[bOff + 1] = (BYTE)0; //G
				buffer[bOff + 2] = (BYTE)0; //R
			} else if (sampW == 2) {
				buffer[bOff] = (BYTE)255; //B
				buffer[bOff + 1] = (BYTE)64; //G
				buffer[bOff + 2] = (BYTE)0; //R
			} else {
				s = 1.0f;
				v = sampH;
				h = ((1.0f - sampT) * 250.0f);
				HSVtoRGB(r, g, b, h, s, v);

				buffer[bOff] = (BYTE)(std::floorf(b * 255.0f)); //B
				buffer[bOff + 1] = (BYTE)(std::floorf(g * 255.0f)); //G
				buffer[bOff + 2] = (BYTE)(std::floorf(r * 255.0f)); //R
			}

			bOff += 3;
		}
	}

	SaveBitmapToFile((BYTE*)buffer, size, size, 24, 0, ".\\compound.bmp");
}

int main() {
	//Set up the config for the generator
	WG::Settings config;
	config.worldSize = 512;
	config.seed = 1337;
	config.seaLevel = 0.15f;

	//Height modifier just does a global multiply on the height data to lower the edges into the sea
	config.heightModifier = WG::HeightModifier::PANGAEA;

	config.hydraulicErosionIterations = 0;

	config.thermalErosionIterations = 5;
	config.thermalErosionThreshold = 0.0005f;
	config.thermalErosionCoefficient = 0.5f;
	
	WG::Generator generator(config);
	generator.generate();

	//Save the data
	SaveHeightmapData(generator.getHeightData());
	SaveWaterData(generator.getWaterData());
	SaveTemperatureData(generator.getTemperatureData(), generator.getHeightData());
	SaveMoistureData(generator.getMoistureData());
	SaveBiomeData(generator.getBiomeData());
	SaveCompoundData(&generator, config.worldSize);
	
	/*
	//Calculate normals (This doesn't produce very good normal maps, so I commented it out)
	vector3* norm = new vector3[512 * 512];
	for (int y = 0; y < 512; y++) {
		for (int x = 0; x < 512; x++) {
			float tl = data->getValueWrapped(x - 1, y + 1),
				t = data->getValueWrapped(x, y + 1),
				tr = data->getValueWrapped(x + 1, y + 1);
			float r = data->getValueWrapped(x + 1, y),
				l = data->getValueWrapped(x - 1, y);
			float bl = data->getValueWrapped(x - 1, y - 1),
				b = data->getValueWrapped(x, y - 1),
				br = data->getValueWrapped(x + 1, y - 1);

			float dx = (tr + 2.0f * r + br) - (tl + 2.0f * l + bl);
			float dy = (bl + 2.0f * b + br) - (tl + 2.0f * l + tr);
			float dz = 1.0f / 0.5f;
			vector3 normal;
			normal.x = dx;
			normal.y = dy;
			normal.z = dz;
			normal.normalize();

			norm[x * 512 + y] = normal;
		}
	}
	SaveNormalData(norm);
	delete[] norm;
	*/

	system("pause");
	return 0;
}
