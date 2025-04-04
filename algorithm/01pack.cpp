#include <iostream>
#include <vector>

using namespace std;


int zeroOnePackMethod(vector<int>& values, vector<int>& weights, int W) {
    vector<int> dp(W + 1, 0);
    for (int i = 0; i < weights.size(); i++) {
        for (int j = W; j >= weights[i]; j--) {
            dp[j] = max(dp[j], dp[j - weights[i]] + values[i]);
        }
    }
    return dp[W];
}

int main() {

}