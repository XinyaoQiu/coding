#include <iostream>
#include <vector>

using namespace std;

int maxValue(vector<int>& values, vector<int>& weights, int maxWeight) {
    vector<int> dp(maxWeight + 1, 0);
    for (int i = 0; i < values.size(); i++) {
        for (int j = maxWeight; j >= weights[i]; j--) {
            dp[j] = max(dp[j], dp[j - weights[i]] + values[i]);
        }
    }
    return dp[maxWeight];
}