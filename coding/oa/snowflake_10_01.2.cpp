#include <iostream>
#include <vector>
#include <bitset>
#include <algorithm>
#include <cmath>

using namespace std;

string toBinaryString(int num, int length) {
    string binaryStr = bitset<32>(num).to_string();
    return binaryStr.substr(32 - length); 
}

int superBitstrings(int n, vector<int>& bitStrings) {
    int num = bitStrings[0];
    for (int i = 1; i < bitStrings.size(); i++) {
        num &= bitStrings[i];
    }
    string binaryStr = toBinaryString(num, n);
    int zero_count = count(binaryStr.begin(), binaryStr.end(), '0');
    return pow(2, zero_count);
}

int main() {
    vector<int> bitStrings({10, 26});
    cout << superBitstrings(5, bitStrings) << endl;
}