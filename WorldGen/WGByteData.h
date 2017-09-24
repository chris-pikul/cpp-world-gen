#pragma once
#include <memory>
#include <algorithm>

using namespace std;

namespace WG {
	//Basically just a container(wrapper) for the byte array with a couple helpers
	struct ByteData {
		uint8_t* data;
		int size;

		ByteData(int size) {
			this->data = new uint8_t[size * size];
			this->size = size;
		}
		ByteData(ByteData &copy) {
			this->size = copy.size;
			this->data = new uint8_t[size * size];

			for (int y = 0; y < size; y++) {
				for (int x = 0; x < size; x++) {
					this->data[x*size + y] = copy.data[x*size*y];
				}
			}
		}

		~ByteData() {
			if (data != NULL)
				delete[] data;
		}

		uint8_t getValue(int x, int y) {
			return this->data[x * size + y];
		}
		uint8_t getValueClamped(int x, int y) {
			return this->data[max(0, min(x, size - 1)) * size + max(0, min(y, size - 1))];
		}
		uint8_t getValueWrapped(int x, int y) {
			int tx = x < 0 ? size + x : (x >= size ? x % (size - 1) : x);
			int ty = y < 0 ? size + y : (y >= size ? y % (size - 1) : y);
			return this->data[tx * size + ty];
		}

		void setValue(uint8_t val, int x, int y) {
			this->data[x * size + y] = val;
		}
		void setValueClamped(uint8_t val, int x, int y) {
			this->data[max(0, min(x, size - 1)) * size + max(0, min(y, size - 1))] = val;
		}
		void setValueWrapped(uint8_t val, int x, int y) {
			x = x < 0 ? size - x : (x >= size ? x % (size - 1) : x);
			y = y < 0 ? size - y : (y >= size ? y % (size - 1) : y);
			this->data[x * size + y] = val;
		}

		void normalize() {
			uint8_t min = 255,
				max = 0;
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