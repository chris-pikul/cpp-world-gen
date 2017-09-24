#pragma once

#include "WGGeneratorSettings.h"
#include "WGFloatData.h"
#include "WGByteData.h"

struct vector3 {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;

	float length() {
		return sqrtf((x*x) + (y*y) + (z*z));
	}

	void normalize() {
		float len = length();
		x /= len;
		y /= len;
		z /= len;
	}
};

struct waterCell {
	float waterAmount = 0.0f;
	float sedimentAmount = 0.0f;
};

namespace WG {
	class PerlinNoise;

	// Holds the generator parent information and connects everything up
	class Generator {
	public:
		static const uint8_t BIOME_OCEAN = 0;
		static const uint8_t BIOME_SUBTROPICAL_DESERT = 1;
		static const uint8_t BIOME_GRASSLAND = 2;
		static const uint8_t BIOME_TROPICAL_FOREST = 3;
		static const uint8_t BIOME_TROPICAL_RAIN_FOREST = 4;
		static const uint8_t BIOME_TEMPERATE_DESERT = 5;
		static const uint8_t BIOME_DECIDEOUS_FOREST = 6;
		static const uint8_t BIOME_TEMPERATE_RAIN_FOREST = 7;
		static const uint8_t BIOME_SHRUBLAND = 8;
		static const uint8_t BIOME_TAIGA = 9;
		static const uint8_t BIOME_SCORCHED = 10;
		static const uint8_t BIOME_BARE = 11;
		static const uint8_t BIOME_TUNDRA = 12;
		static const uint8_t BIOME_ARCTIC = 13;

		Generator(Settings config);
		~Generator();

		void generate();

		inline FloatData* getHeightData() { return this->dataHeight; }
		inline FloatData* getTemperatureData() { return this->dataTemp; }
		inline ByteData* getWaterData() { return this->dataWater; }
		inline ByteData* getBiomeData() { return this->dataBiomes; }
		inline FloatData* getMoistureData() { return this->dataMoist; }
	private:
		Settings settings;

		FloatData* dataHeight;
		FloatData* dataTemp;
		ByteData* dataWater;
		ByteData* dataBiomes;

		FloatData* dataMoist;

		void hmPangaea();
		void hmInvPangaea();
		void hmStraight();

		void erosionThermal();
		void erosionHydraulic();
		void erosionHydrailicImproved();

		void calculateSaltwater();
		void calculateMoisture();
		void calculateTemperature();
		void calculateFreshwater();

		void calculateBiomes();

		uint8_t getBiome(int temp, int moist);
	};
}

