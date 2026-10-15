#include <iostream>
#include <vector>
using namespace std;

int lower_bound(vector<int>& arr, int target) {
    int L = 0, R = arr.size() - 1;
    while (L <= R) {
        int M = L + (R - L) / 2;
        if (arr[M] >= target) {
            R = M - 1;
        } else {
            L = M + 1;
        }
    }
    return arr[L];
}

int solution(vector<int>& alpha2beta, vector<int>& beta2alpha, int missions) {
    int time_unit = 0;
    while (missions > 0) {
        --missions;
        time_unit = lower_bound(alpha2beta, time_unit);
        time_unit += 100;
        time_unit = lower_bound(beta2alpha, time_unit);
        time_unit += 100;
    }
    return time_unit;
}

int main() {
    vector<int> alpha2beta({5, 206});
    vector<int> beta2alpha({105, 306});
    cout << solution(alpha2beta, beta2alpha, 2) << endl;
}