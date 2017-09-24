#pragma once
#include <memory>
#include <algorithm>

using namespace std;

namespace WG {
	//Basically just a container(wrapper) for the float array with a couple helpers
	struct FloatData {
		float* data;
		int size;

		FloatData(int size) {
			this->data = new float[size * size];
			this->size = size;
		}
		FloatData(FloatData &copy) {
			this->size = copy.size;
			this->data = new float[size * size];

			for (int y = 0; y < size; y++) {
				for (int x = 0; x < size; x++) {
					this->data[x*size+y] = copy.data[x*size*y];
				}
			}
		}

		~FloatData() {
			if(data != NULL)
				delete [] data;
		}

		float getValue(int x, int y) {
			return this->data[x * size + y];
		}
		float getValueClamped(int x, int y) {
			return this->data[max(0, min(x, size-1)) * size + max(0, min(y, size-1))];
		}
		float getValueWrapped(int x, int y) {
			int tx = x < 0 ? size + x : (x >= size ? x%(size-1) : x);
			int ty = y < 0 ? size + y : (y >= size ? y % (size - 1) : y);
			return this->data[tx * size + ty];
		}

		void setValue(float val, int x, int y) {
			this->data[x * size + y] = val;
		}
		void setValueClamped(float val, int x, int y) {
			this->data[max(0, min(x, size-1)) * size + max(0, min(y, size-1))] = val;
		}
		void setValueWrapped(float val, int x, int y) {
			x = x < 0 ? size - x : (x >= size ? x % (size - 1) : x);
			y = y < 0 ? size - y : (y >= size ? y % (size - 1) : y);
			this->data[x * size + y] = val;
		}

		void normalize() {
			float min = FLT_MAX,
				max = -FLT_MAX;
			for (int i = 0; i < (size*size); i++) {
				if (data[i] < min)
					min = data[i];
				else if (data[i] > max)
					max = data[i];
			}

			for (int i = 0; i < (size*size); i++)
				data[i] = (data[i] - min) / (max - min);
		}
	};
}