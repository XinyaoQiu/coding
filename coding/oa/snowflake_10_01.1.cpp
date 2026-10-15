#include <iostream>
#include <vector>
#include <cmath>

using namespace std;

long long calculateWays(int wordLen, int maxVowels) {
    vector<vector<long long>> combinations(wordLen + 1, vector<long long>(maxVowels + 2, 1));
    for (int i = 1; i <= wordLen; i++) {
        for (int j = 1; j < i; j++) {
            combinations[i][j] = combinations[i - 1][j - 1] + combinations[i - 1][j];
        }
    }
    long long result = 0;
    for (int i = 0; i <= maxVowels + 1; i++) {
        result += combinations[wordLen][i] * pow(5, i) * pow(21, wordLen - i);
    }
    result -= (wordLen - maxVowels) * pow(5, maxVowels + 1) * pow(21, wordLen - maxVowels - 1);
    return result;
}

int main() {
    cout << calculateWays(4, 2) << endl;
}