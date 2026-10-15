#include <iostream>
#include <vector>
using namespace std;

int googleCountWithSumS(int S) {
    vector<vector<int>> dp(5, vector<int>(S + 1, 0));
    dp[0][0] = 1;

    for (int d = 1; d <= 4; d++) {
        for (int s = 0; s <= S; s++) {
            for (int i = 0; i <= min(s, 9); i++) {
                dp[d][s] += dp[d - 1][s - i];
            }
        }
    }

    return dp[4][S];
}

int main() {
    int S;
    cin >> S;
    cout << googleCountWithSumS(S) << endl;
    return 0;
}