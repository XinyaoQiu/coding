#include <iostream>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;

int minOperationsToBalance(const string& content) {
    unordered_map<char, int> freqMap;
    for (char c : content) {
        freqMap[c]++;
    }
    vector<int> freqs;
    for (auto& entry : freqMap) {
        freqs.push_back(entry.second);
    }
    sort(freqs.begin(), freqs.end());
    int numUnique = freqs.size();
    int targetFrequency = 0;
    if (numUnique % 2 == 0) {
        targetFrequency = (freqs[numUnique / 2 - 1] + freqs[numUnique / 2]) / 2;
    } else {
        targetFrequency = freqs[numUnique / 2];
    }
    int operations = 0;
    for (int i = 0; i < numUnique; i++) {
        operations += abs(freqs[i] - targetFrequency);
    }
    return operations;
}

int main() {
    string content;
    cout << "Enter the content string: ";
    cin >> content;

    int result = minOperationsToBalance(content);
    cout << "Minimum operations to balance the string: " << result << endl;

    return 0;
}
