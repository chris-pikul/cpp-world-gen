#include "WGGenerator.h"
#include "FastNoise.h"

#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <vector>

using namespace WG;

//Simple representation of a 2D point
struct Point {
	int x;
	int y;

	Point(int setX, int setY) {
		x = setX;
		y = setY;
	}
};

//Returns a random number between low and high
inline int randomRange(int low, int high) {
	return low + int(high*rand() / (RAND_MAX + 1.0));
}

Generator::Generator(Settings config) {
	this->settings = config;

	this->dataHeight = new FloatData(config.worldSize);
	this->dataTemp = new FloatData(config.worldSize);
	this->dataWater = new ByteData(config.worldSize);
	this->dataBiomes = new ByteData(config.worldSize);
	this->dataMoist = new FloatData(config.worldSize);
}

Generator::~Generator() {
	//ALWAYS DELETE POINTERS
	delete dataHeight;
	delete dataTemp;
	delete dataWater;
	delete dataBiomes;
	delete dataMoist;
}

void Generator::generate() {
	//First set up a perturber from FastNoise
	//Basically this takes input coordinates and moves them randomly to give more of an organic feel
	FastNoise peturber(settings.seed);
	peturber.SetFrequency(0.02f);
	peturber.SetFractalOctaves(5);
	peturber.SetFractalLacunarity(2.0f);
	peturber.SetFractalGain(0.7f);
	peturber.SetGradientPerturbAmp(20.0f);

	//Get base height data
	//This is just simplex fractal noise. Same thing really as Perlin, or any other "cloud" like noise
	FastNoise noise(settings.seed);
	noise.SetNoiseType(FastNoise::NoiseType::SimplexFractal);
	noise.SetFrequency(0.01f);
	noise.SetFractalOctaves(5);
	noise.SetFractalLacunarity(1.5f);
	noise.SetFractalGain(0.6f);

	float ptX = 0.0f, ptY = 0.0f;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			ptX = (float)x;
			ptY = (float)y;
			//Perturb the coordinates for more organic feel
			peturber.GradientPerturbFractal(ptX, ptY);

			//Set the height data at the x,y coordinate with perlin/simplex noise
			dataHeight->data[x * settings.worldSize + y] = noise.GetSimplexFractal(ptX, ptY);
		}
	}
	//The noise functions return float values from -1...1, so make them units between 0...1
	dataHeight->normalize();

	//Get cellular noise
	//Cellular noise is basically Veronoi cell noise. Produces less cloud like and more shappely figures
	FloatData* cellData = new FloatData(settings.worldSize);
	FastNoise cellNoise(settings.seed);
	cellNoise.SetNoiseType(FastNoise::NoiseType::Cellular);
	cellNoise.SetFrequency(0.008f);
	cellNoise.SetCellularReturnType(FastNoise::CellularReturnType::Distance2);

	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			ptX = (float)x;
			ptY = (float)y;
			peturber.GradientPerturbFractal(ptX, ptY);

			//Again perturb the coordinates. This is very important for cellular noise
			//as it usually (always) has straight edges
			cellData->data[x * settings.worldSize + y] = cellNoise.GetCellular(ptX, ptY);
		}
	}
	cellData->normalize();

	//Blend simplex+cellular
	float samp = 0.0f;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			// Take 30% of the perlin/simplex and layer over 70% cellular noise
			//The cellular noise produces better mountains and mountain range type structures.
			//Where the perlin produces better randomization and height noise
			//Together they come out organic, yet structured looking
			samp = (dataHeight->getValue(x,y)*0.3f) + (cellData->getValue(x,y)*0.7f);
			dataHeight->setValue(samp, x, y);
		}
	}

	//To be safe, I normalize again in-case something went over
	dataHeight->normalize();
	delete cellData;

	//If the settings want it, run thermal erosion
	if (settings.thermalErosionIterations > 0)
		erosionThermal();

	//Run height modifier
	switch (settings.heightModifier) {
	case PANGAEA:
		hmPangaea();
		break;
	case INV_PANGAEA:
		hmInvPangaea();
		break;
	case STRAIGHT:
		hmStraight();
		break;
	}

	//Claim the ocean tiles
	calculateSaltwater();

	//Calculate the moisture map
	calculateMoisture();

	//If settings want it, do hydraulic erosion
	//This algorithm isn't very good, so I didn't use it in my tests
	if (settings.hydraulicErosionIterations > 0)
		erosionHydraulic();

	//This just takes forever and isn't producing good results yet
	//calculateFreshwater();

	//Calculate temperature for climate
	calculateTemperature();

	//With the ready data, get the biome data
	calculateBiomes();
}

// Pangea filter puts ocean around the whole map leaving the bulk land in the center
void Generator::hmPangaea() {
	std::cout << "Running pangaea modifier" << std::endl;
	float modX = 0.0f, modY = 0.0f;
	float halfSize = (float)settings.worldSize / 2.0f;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			modX = 1.0f - powf((abs(x - halfSize) / halfSize), 2.0f);
			modY = 1.0f - powf((abs(y - halfSize) / halfSize), 2.0f);
			dataHeight->data[x * settings.worldSize + y] *= (modX * modY);
		}
	}
}

// Inverse Pangea does the oposite. Drops the height in the center to generate a large central sea
void Generator::hmInvPangaea() {
	std::cout << "Running inverse-pangaea modifier" << std::endl;
	float modX = 0.0f, modY = 0.0f;
	float halfSize = (float)settings.worldSize / 2.0f;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			modX = powf((abs(x - halfSize) / halfSize), 0.3f);
			modY = powf((abs(y - halfSize) / halfSize), 0.3f);
			dataHeight->data[x * settings.worldSize + y] *= (modX * modY);
		}
	}
}


// Straight modifier drops the height data in the center in a strip or band across the width.
// Basically producing a mediterranian type land mass with a large river running through the center
void Generator::hmStraight() {
	std::cout << "Running straight modifier" << std::endl;
	float modY = 0.0f;
	float halfSize = (float)settings.worldSize / 2.0f;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			modY = powf((abs(y - halfSize) / halfSize), 0.3f);
			dataHeight->data[x * settings.worldSize + y] *= modY;
		}
	}
}

//Thermal erosion adjusts height data based on it's neighbors and the difference between
//them and the sampled point. Good for smoothing out the height information
void Generator::erosionThermal() {
	int32 size = settings.worldSize;
	float thresh = settings.thermalErosionThreshold; //Threshold is the delta-difference needed before changing
	float coeff = settings.thermalErosionCoefficient; //Coefficient specifies how much of the change to actually use
	int32 iters = settings.thermalErosionIterations; //How many times will this run

	float samp = 0.0f, delta=0.0f, deltaMax=-1.0f;
	float pnt[4];
	float dif[4];

	for (int i = 0; i < iters; i++) {
		std::cout << "Running Thermal Erosion - Iteration: " << i << endl;
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				samp = dataHeight->getValue(x, y); //Get the targetted sample point from the height data

				//Retrieve the neighbors to North/South/East/West
				pnt[0] = dataHeight->getValueClamped(x - 1, y);
				pnt[1] = dataHeight->getValueClamped(x, y - 1);
				pnt[2] = dataHeight->getValueClamped(x + 1, y);
				pnt[3] = dataHeight->getValueClamped(x, y + 1);

				//Calculate the difference between the target sample and each neighbor point
				dif[0] = samp - pnt[0];
				dif[1] = samp - pnt[1];
				dif[2] = samp - pnt[2];
				dif[3] = samp - pnt[3];

				//Figure out the total differences between sample and neighbors
				delta = 0.0f;
				deltaMax = -1.0f;
				for (int j = 0; j < 4; j++) {
					if (dif[j] > thresh) {
						delta += dif[j];
						if (dif[j] > deltaMax)
							deltaMax = dif[j];
					}
				}

				//Fill the neighbors with the scaled difference
				//Essentially "blurs" the height data
				for (int j = 0; j < 4; j++) {
					if (delta > 0.0f)
						pnt[j] += coeff * (deltaMax - thresh) * (dif[j] / delta);
				}

				//Update the height data with the new values
				dataHeight->setValueClamped(pnt[0], x - 1, y);
				dataHeight->setValueClamped(pnt[1], x, y - 1);
				dataHeight->setValueClamped(pnt[2], x + 1, y);
				dataHeight->setValueClamped(pnt[3], x, y + 1);
			}
		}
	}
}

//Hyrdaulic erosion is... complicated
//Essentially the process invloves tracking a "water" map.
//Iterating over the contents, water is added to the tiles in the form of rain
//Based on the water contained on one tile, it picks up sediment from the heightmap.
//Using the slope data from neighbor points it "flows" that water into those tiles,
//and with it bringing some sediment.
//During the evaporation phase, water is dried from the tiles and the suspended sediment
//is added to the height data at that tile. Effectively carrying dirt from one tile to another
void Generator::erosionHydraulic() {
	int32 size = settings.worldSize;

	waterCell* water = new waterCell[size * size];
	for (int y = 0; y < size; y++)
		for (int x = 0; x < size; x++)
			water[x * size + y].waterAmount = dataMoist->getValue(x,y) * 0.1f; //Seed buckets

	for (int i = 0; i < settings.hydraulicErosionIterations; i++) {
		cout << "Running hydraulic erosion - Iteration: " << i << endl;
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				//Add water (Raining still)
				waterCell* wc = &water[x * size + y];
				if (wc->waterAmount < 0.9f)
					wc->waterAmount += dataMoist->getValue(x, y) * 0.01f;

				float c = dataHeight->getValue(x, y);
				float tl = dataHeight->getValueWrapped(x - 1, y - 1),
					t = dataHeight->getValueWrapped(x, y - 1),
					tr = dataHeight->getValueWrapped(x + 1, y - 1);
				float r = dataHeight->getValueWrapped(x + 1, y),
					l = dataHeight->getValueWrapped(x - 1, y);
				float bl = dataHeight->getValueWrapped(x - 1, y + 1),
					b = dataHeight->getValueWrapped(x, y + 1),
					br = dataHeight->getValueWrapped(x + 1, y + 1);

				float dt = t - c, dtl = tl - c, dtr = tr - c,
					db = b - c, dbl = bl - c, dbr = br - c,
					dr = r - c, dl = l - c;

				float totalDelta = (dt < 0 ? dt : 0) + (dtl < 0 ? dtl : 0) + (dtr < 0 ? dtr : 0) +
					(db < 0 ? db : 0) + (dbl < 0 ? dbl : 0) + (dbr< 0 ? dbr : 0) +
					(dr < 0 ? dr : 0) + (dl < 0 ? dl : 0);
				totalDelta *= -1;

				if (totalDelta > 0.001f) {
					float deltMax = std::min(totalDelta, 0.9f);

					//Pick-up sediment
					float sediment = std::min(c*0.5f, (wc->waterAmount * (deltMax * 0.25f)));
					dataHeight->setValue(c - sediment, x, y); //Reduction in current spot

					float wpick = wc->waterAmount * deltMax;
					wc->waterAmount -= wpick;

					if (dt < 0) {
						water[x * size + (y == 0 ? size-1 : y - 1)].sedimentAmount += sediment * (-dt / totalDelta);
						water[x * size + (y == 0 ? size-1 : y - 1)].waterAmount += sediment * (-dt / totalDelta);
					}
					if (dtl < 0) {
						water[(x == 0 ? size-1 : x - 1) * size + (y == 0 ? size-1 : y - 1)].sedimentAmount += sediment * (-dtl / totalDelta);
						water[(x == 0 ? size-1 : x - 1) * size + (y == 0 ? size-1 : y - 1)].waterAmount += sediment * (-dtl / totalDelta);
					}
					if (dtr < 0) {
						water[(x == size-1 ? 0 : x + 1) * size + (y == 0 ? size-1 : y - 1)].sedimentAmount += sediment * (-dtr / totalDelta);
						water[(x == size-1 ? 0 : x + 1) * size + (y == 0 ? size-1 : y - 1)].waterAmount += sediment * (-dtr / totalDelta);
					}

					if (db < 0) {
						water[x * size + (y == size-1 ? 0 : y + 1)].sedimentAmount += sediment * (-db / totalDelta);
						water[x * size + (y == size-1 ? 0 : y + 1)].waterAmount += sediment * (-db / totalDelta);
					}
					if (dbl < 0) {
						water[(x == 0 ? size-1 : x - 1) * size + (y == size-1 ? 0 : y + 1)].sedimentAmount += sediment * (-dbl / totalDelta);
						water[(x == 0 ? size-1 : x - 1) * size + (y == size-1 ? 0 : y + 1)].waterAmount += sediment * (-dbl / totalDelta);
					}
					if (dbr < 0) {
						water[(x == size-1 ? 0 : x + 1) * size + (y == size-1 ? 0 : y + 1)].sedimentAmount += sediment * (-dbr / totalDelta);
						water[(x == size-1 ? 0 : x + 1) * size + (y == size-1 ? 0 : y + 1)].waterAmount += sediment * (-dbr / totalDelta);
					}

					if (dl < 0) {
						water[(x == 0 ? size-1 : x - 1) * size + y].sedimentAmount += sediment * (-dl / totalDelta);
						water[(x == 0 ? size-1 : x - 1) * size + y].waterAmount += sediment * (-dl / totalDelta);
					}
					if (dr < 0) {
						water[(x == size-1 ? 0 : x + 1) * size + y].sedimentAmount += sediment * (-dr / totalDelta);
						water[(x == size-1 ? 0 : x + 1) * size + y].waterAmount += sediment * (-dr / totalDelta);
					}
				}

				//Evaporate
				wc->waterAmount *= 0.1f;
				if (wc->sedimentAmount > 0.1f) {
					dataHeight->setValue(dataHeight->getValue(x, y) + (wc->sedimentAmount - 0.1f), x, y);
					wc->sedimentAmount = 0.1f;
				}
			}
		}
	}

	waterCell wsamp;
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			dataHeight->setValue(dataHeight->getValue(x, y) + water[x * size + y].sedimentAmount, x, y);

			wsamp = water[x * size + y];
			if (wsamp.waterAmount > 0.2f) {
				cout << "Found full bucket " << x << ", " << y << endl;
				dataWater->setValue(2, x, y);
			}
		}
	}

	delete[] water;
}

void Generator::erosionHydrailicImproved() {
	//Do this later
}

//Build the ocean data
void Generator::calculateSaltwater() {
	cout << "Claiming saltwater ocean..." << endl;
	int size = settings.worldSize;

	//Set all tiles under the sea-level to ocean
	float samp = 0.0f;
	for (int y = 0; y < size; y++) {
		for (int x = 0; x < size; x++) {
			samp = dataHeight->getValue(x, y);

			if (samp <= settings.seaLevel)
				dataWater->setValue(1, x, y);
			else
				dataWater->setValue(0, x, y);
		}
	}

	//Dry up small seas by checking the neighbors 3 tiles away.
	//If none of the tiles 3 points away are ocean, then remove this one as well.
	//Essentially any lonely single ocean tiles get removed.
	for (int i = 0; i < 25; i++) {
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				if (dataWater->getValueClamped(x - 3, y) == 0 &&
					dataWater->getValueClamped(x + 3, y) == 0 &&
					dataWater->getValueClamped(x, y - 3) == 0 &&
					dataWater->getValueClamped(x, y + 3) == 0 )
					dataWater->setValue(0, x, y);
			}
		}
	}
}

//This is a very cheap and simple moisture calculation
//It doesn't take into consideration anything.
//It's basically just a posterized cellular noise map
void Generator::calculateMoisture() {
	cout << "Calculating moisture..." << endl;
	FastNoise peturber(settings.seed);
	peturber.SetFrequency(0.02f);
	peturber.SetFractalOctaves(5);
	peturber.SetFractalLacunarity(2.0f);
	peturber.SetFractalGain(0.7f);
	peturber.SetGradientPerturbAmp(20.0f);

	FastNoise noise(settings.seed);
	noise.SetNoiseType(FastNoise::NoiseType::Cellular);
	noise.SetFrequency(0.01f);
	noise.SetCellularReturnType(FastNoise::CellularReturnType::CellValue);

	float ptX = 0.0f, ptY = 0.0f;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			ptX = (float)x;
			ptY = (float)y;
			peturber.GradientPerturb(ptX, ptY);
			dataMoist->setValue(noise.GetCellular(ptX, ptY), x, y);
		}
	}
	dataMoist->normalize();
}

//Calculate the temperature.
//This is a double function in a way.
//It maintains a heat index from both latitude and height data.
//So anything half-way down the map is considered hottest (equator), where north is cooler (poles)
//While combining with the height data, heigher mountains "cool" the temperature
//I've chosen to ignore the ocean tiles, since their height data should not influence this
//
// Temperature data is 1=HOT ... 0=COLD
void Generator::calculateTemperature() {
	cout << "Calculating climate temperature..." << endl;
	int halfSize = (settings.worldSize / 2);
	float samp = 0.0f, band = 0.0f, out = 0.0f, htInfl;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			samp = dataHeight->getValue(x, y); // Height of the tile
			band = 1.0f - ((float)(abs(y - halfSize) / (float)halfSize)); //Equator/Poles "band" temperature 1 at center, 0 at top

			if (dataWater->getValue(x, y) == 1) //If ocean, ignore height data
				htInfl = 0;
			else {
				//Inverse the height minus sea-level.
				//I subtract sea-level so that anything at sea-level should effectively not change the latitudal influence
				//Where height mountains will subtract from the latitudal influence
				//Also, any land under sea-level becomes hotter by adding to the latitudal influence
				htInfl = (samp - settings.seaLevel) * -1.0f; 
			}

			//Mix the height influence and latitudal band and clamp the value between 0...1
			out = std::max(0.0f, std::min(band+htInfl, 1.0f));

			//Set temperature data
			dataTemp->setValue(out, x, y);
		}
	}

	//I didn't like the very ridgidness of the resulting map.
	//While it is correct, it is tile specific which makes for lot's of noise
	//In reality this wouldn't happen, a kind of gradient or blur would be needed.
	//So I do just that. Average the target tile with it's neightbors with a given "coefficent"
	float t = 0.0f, r = 0.0f, b = 0.0f, l = 0.0f, avg = 0.0f;
	for (int i = 0; i < 100; i++) {
		for (int y = 0; y < settings.worldSize; y++) {
			for (int x = 0; x < settings.worldSize; x++) {
				samp = dataTemp->getValue(x, y);

				//Get neighbor values from NSWE
				t = dataTemp->getValueWrapped(x, y - 1);
				r = dataTemp->getValueWrapped(x + 1, y);
				b = dataTemp->getValueWrapped(x, y - 1);
				l = dataTemp->getValueWrapped(x - 1, y);

				//Average them together
				avg = (samp + t + r + b + l) / 5.0f;

				//Only use 50% influence. Honestly this is probably overkill, could just do full influence
				//And have less need for iterations, but doing this to be safe so the whole thing doesn't go
				//from un-touched, to completely average
				dataTemp->setValue(((samp*0.5f) + (avg * 0.5f)), x, y);
			}
		}
	}
}

//This is a work-in-progress freshwater algorithm.
//It doesn't work so good.
//The idea so far is to "seed" rivers at random and only above a certain altitude.
//As usually mountains are the start of rivers.
//Then it was supposed to iterate over those river points and claim any neighbor point
//which was lower then itself (downhill).
//So far it just does a crazy flood-fill thing which is not useful.
//
//My idea to improve is to use velocity with random nudges to hopefully get it to snake along in a semi-straight path.
void Generator::calculateFreshwater() {
	cout << "Forming freshwater..." << endl;
	int size = settings.worldSize;

	vector<Point> water;
	vector<Point> tmp;

	srand(settings.seed);

	float samp, msamp;
	int swt;
	//Seed the rivers by height
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			samp = dataHeight->getValue(x, y);

			if (dataWater->getValue(x, y) > 0)
				continue;

			if (samp >= 0.3f) {
				msamp = dataMoist->getValue(x, y);
				swt = randomRange(0, (size*2) + (size - (int)(msamp * size)));
				if (swt <= 1) {
					dataWater->setValue(2, x, y);
					dataHeight->setValue(dataHeight->getValue(x, y) - (1.0f / 255.0f), x, y);
					water.push_back(Point(x, y));
					cout << "Seeded river" << endl;
				}
			}
		}
	}

	tmp.swap(water);
	float samps[8];
	int lowest = 0;
	float delta;
	for (int i = 0; i < 100; i++) {
		//Using a vector instead of the whole map so we only iterate what we know is rivers
		for (std::vector<Point>::iterator it = tmp.begin(); it != tmp.end(); ++it) {
			Point pnt = *it;

			//Get the height of the current river point and ALL neighbors
			//North/North East/East/South East/South/South West/West/North West
			samp = dataHeight->getValue(pnt.x, pnt.y);
			samps[0] = dataHeight->getValueClamped(pnt.x, pnt.y - 1);
			samps[1] = dataHeight->getValueClamped(pnt.x + 1, pnt.y - 1);
			samps[2] = dataHeight->getValueClamped(pnt.x+1, pnt.y);
			samps[3] = dataHeight->getValueClamped(pnt.x+1, pnt.y-1);
			samps[4] = dataHeight->getValueClamped(pnt.x, pnt.y-1);
			samps[5] = dataHeight->getValueClamped(pnt.x-1, pnt.y-1);
			samps[6] = dataHeight->getValueClamped(pnt.x-1, pnt.y);
			samps[7] = dataHeight->getValueClamped(pnt.x-1, pnt.y+1);

			//Find the lowest instead of just spreading to all (which would really flood-fill)
			lowest = -1;
			delta = samp;
			for (int j = 0; j < 8; j++) {
				if (samps[j] < delta) {
					delta = samps[j];
					lowest = j;
				} else if (samps[j] == delta) {
					if (randomRange(0, 8) == 1)
						lowest = j;
				}
			}

			//From the chosen index, birth a new river tile
			Point newPnt(-1, -1);
			switch (lowest){
			case 0:
				newPnt.x = pnt.x;
				newPnt.y = pnt.y - 1;
				break;
			case 1:
				newPnt.x = pnt.x + 1;
				newPnt.y = pnt.y - 1;
				break;
			case 2:
				newPnt.x = pnt.x + 1;
				newPnt.y = pnt.y;
				break;
			case 3:
				newPnt.x = pnt.x + 1;
				newPnt.y = pnt.y + 1;
				break;
			case 4:
				newPnt.x = pnt.x;
				newPnt.y = pnt.y + 1;
				break;
			case 5:
				newPnt.x = pnt.x - 1;
				newPnt.y = pnt.y + 1;
				break;
			case 6:
				newPnt.x = pnt.x - 1;
				newPnt.y = pnt.y;
				break;
			case 7:
				newPnt.x = pnt.x - 1;
				newPnt.y = pnt.y - 1;
				break;
			default:
				continue;
			}

			if (lowest >= 0) {
				//Set the actual tile data
				if (dataWater->getValue(newPnt.x, newPnt.y) > 0)
					continue;

				dataHeight->setValue(dataHeight->getValue(newPnt.x, newPnt.y) - (1.0f / 255.0f), newPnt.x, newPnt.y);
				dataWater->setValue(2, newPnt.x, newPnt.y);
				water.push_back(newPnt);
			}
		}

		//Cannot modify the same vector you are iterating so copy over the water data to the temp vector for iteration
		tmp.swap(water);
	}

	//Old algorithm, even worse (but actually didn't flood fill, just did straight lines
	/*
	float t, r, b, l;
	for (int i = 0; i < 100; i++) {
		for (std::vector<Point>::iterator it = tmp.begin(); it != tmp.end(); ++it) {
			Point pnt = *it;
			samp = dataHeight->getValue(pnt.x, pnt.y);
			t = dataHeight->getValueClamped(pnt.x, pnt.y - 1);
			r = dataHeight->getValueClamped(pnt.x - 1, pnt.y);
			b = dataHeight->getValueClamped(pnt.x, pnt.y + 1);
			l = dataHeight->getValueClamped(pnt.x+1, pnt.y);

			if (t<r && t<b && t<l && t < samp) {
				Point newPnt(pnt.x, pnt.y - 1);

				if (dataWater->getValue(newPnt.x, newPnt.y) > 0)
					continue;

				dataHeight->setValue(dataHeight->getValue(newPnt.x, newPnt.y) - (1.0f / 255.0f), newPnt.x, newPnt.y);
				dataWater->setValue(2, newPnt.x, newPnt.y);
				water.push_back(newPnt);
			}
			if (b<t && b<r && b<l && b < samp) {
				Point newPnt(pnt.x, pnt.y + 1);

				if (dataWater->getValue(newPnt.x, newPnt.y) > 0)
					continue;

				dataHeight->setValue(dataHeight->getValue(newPnt.x, newPnt.y) - (1.0f / 255.0f), newPnt.x, newPnt.y);
				dataWater->setValue(2, newPnt.x, newPnt.y);
				water.push_back(newPnt);
			}
			if (r<t && r<b && r<l && r < samp) {
				Point newPnt(pnt.x-1, pnt.y);

				if (dataWater->getValue(newPnt.x, newPnt.y) > 0)
					continue;

				dataHeight->setValue(dataHeight->getValue(newPnt.x, newPnt.y) - (1.0f / 255.0f), newPnt.x, newPnt.y);
				dataWater->setValue(2, newPnt.x, newPnt.y);
				water.push_back(newPnt);
			}
			if (l<t && l<b && l<r && l < samp) {
				Point newPnt(pnt.x+1, pnt.y);

				if (dataWater->getValue(newPnt.x, newPnt.y) > 0)
					continue;

				dataHeight->setValue(dataHeight->getValue(newPnt.x, newPnt.y) - (1.0f / 255.0f), newPnt.x, newPnt.y);
				dataWater->setValue(2, newPnt.x, newPnt.y);
				water.push_back(newPnt);
			}
		}

		tmp.swap(water);
		cout << "River iteration complete. Water tiles = " << tmp.size() << endl;
	}
	*/

	//Delete the temperary data
	tmp.clear();
	water.clear();
}

//Biome calculation is easiest part so far.
//Takes the temperature and moisture data from their respective arrays and matches them to a biome switch
//It's just a byte/uint8_t number matching to a constant in the Generator class
void Generator::calculateBiomes() {
	cout << "Calculating biome data..." << endl;

	float msamp, tsamp;
	int tempScale, moistScale;
	for (int y = 0; y < settings.worldSize; y++) {
		for (int x = 0; x < settings.worldSize; x++) {
			if (dataWater->getValue(x, y) == 1) //If ocean, set to ocean biome
				dataBiomes->setValue(BIOME_OCEAN, x, y);
			else {
				msamp = dataMoist->getValue(x, y);
				tsamp = dataTemp->getValue(x, y);
				tempScale = (int)(3.0f - roundf(tsamp * 3)); //Inverse the temperature and make it 0...3 (4 levels)
				moistScale = (int)(roundf(msamp * 5)); //Make the moisture to scale 0...5 (6 levels)

				dataBiomes->setValue(getBiome(tempScale, moistScale), x, y);
			}
		}
	}
}

uint8_t Generator::getBiome(int temp, int moist) {
	switch (moist) {
	case 0: //If no moisutre
		switch (temp) {
		case 0: //And super hot (remember inverse temperature
			return BIOME_SUBTROPICAL_DESERT; //Then make sahara desert
		case 1:
		case 2:
			return BIOME_TEMPERATE_DESERT;
		case 3: //If super cold
			return BIOME_SCORCHED; //Scorched earth (super frosty dead-lands
		}
		break;
	case 1:
		switch (temp) {
		case 0:
		case 1:
			return BIOME_GRASSLAND;
		case 2:
			return BIOME_TEMPERATE_DESERT;
		case 3:
			return BIOME_BARE;
		}
		break;
	case 2:
		switch (temp) {
		case 0:
			return BIOME_TROPICAL_FOREST;
		case 1:
			return BIOME_GRASSLAND;
		case 2:
			return BIOME_SHRUBLAND;
		case 3:
			return BIOME_TUNDRA;
		}
		break;
	case 3: //On the middle side of moisture
		switch (temp) {
		case  0: //But super hot make it tropical forest
			return BIOME_TROPICAL_FOREST;
		case 1:
			return BIOME_DECIDEOUS_FOREST;
		case 2:
			return BIOME_SHRUBLAND;
		case 3: //Or if super cold make it arctic snow lands
			return BIOME_ARCTIC;
		}
		break;
	case 4:
		switch (temp) {
		case 0:
			return BIOME_TROPICAL_RAIN_FOREST;
		case 1:
			return BIOME_DECIDEOUS_FOREST;
		case 2:
			return BIOME_TAIGA;
		case 3:
			return BIOME_ARCTIC;
		}
		break;
	case 5: //TONS of moisture, maxed out, like raining all the time
		switch (temp) {
		case 0: //And super hot, obviously rain forest
			return BIOME_TROPICAL_RAIN_FOREST;
		case 1:
			return BIOME_TEMPERATE_RAIN_FOREST;
		case 2:
			return BIOME_TAIGA;
		case 3: //Or super cold, then go to snowy blizzard arctic
			return BIOME_ARCTIC;
		}
		break;
	}
	cout << "Warning, biome scale fell out of range: Temp=" << temp << " Moisture=" << moist << endl;
	return BIOME_SCORCHED;
}
