#include <vector>
using namespace std;

/*
    -3, 4, -6, 2, 3
    -2, -2, -2, 3, 3
*/

class Solution {
  public:
    int canCompleteCircuit(vector<int>& gas, vector<int>& cost) {
        int n = gas.size();
        if (n == 1) {
            return gas[0] >= cost[0] ? 0 : -1;
        }
        for (int i = 0; i < n; i++) {
            int last = gas[i] - cost[i];
            int curr = gas[(i + 1) % n] - cost[(i + 1) % n];
            if (last <= 0 && curr > 0) {
                int count = 0, j = 0;
                for (; j < n; j++) {
                    count += gas[(i + 1 + j) % n] - cost[(i + 1 + j) % n];
                    if (count < 0) {
                        i += j;
                        break;
                    }
                }
                if (j == n) return (i + 1) % n;
            }
        }
        return -1;
    }
};