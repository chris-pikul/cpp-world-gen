#pragma once

#ifndef int32
#define int32 int
#endif

namespace WG {
	// Modifies the starting height by a given function
	enum HeightModifier {
		NONE, PANGAEA, INV_PANGAEA, STRAIGHT
	};

	//Sets up and contains the settings for the generation
	struct Settings {
		int32 worldSize;
		unsigned int seed;
		HeightModifier heightModifier;
		float seaLevel;

		int32 thermalErosionIterations;
		float thermalErosionThreshold;
		float thermalErosionCoefficient;

		int32 hydraulicErosionIterations;
	};
}