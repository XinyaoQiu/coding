#include <iostream>
#include <vector>
#include <unordered_map>

using namespace std;


long long getNumTrips(vector<int>& capacity, long long desiredCapacity) {
    if (capacity.size() <= 2) {
        return 0;
    }
    unordered_map<long long, long long> cap_map;
    cap_map[capacity[0]]++;
    cap_map[capacity[1]]++;
    long long result = 0;
    for (int i = 2; i < capacity.size(); i++) {
        cap_map[capacity[i]]++;
        if (capacity[i - 2] * capacity[i - 1] * capacity[i] == desiredCapacity) {
            result++;
        }
    }
    for (int i = 1; i < capacity.size(); i++) {
        long long mulp = capacity[i - 1] * capacity[i];
        if (desiredCapacity % mulp == 0) {
            long long third_num = desiredCapacity / mulp;
            if (cap_map.count(third_num)) {
                int count_1 = i >= 2 && capacity[i - 2] == third_num ? 1 : 0;
                int count_2 = capacity[i - 1] == third_num ? 1 : 0;
                int count_3 = capacity[i] == third_num ? 1 : 0;
                int count_4 = i < capacity.size() - 1 && capacity[i + 1] == third_num ? 1 : 0;
                result += cap_map[third_num] - count_1 - count_2 - count_3 - count_4;
            }
        }
    }
    return result;
}


int main() {

}