#include <iostream>
#include <vector>
using namespace std;

int solution(vector<int>& arr) {
    int n = arr.size();
    if (n <= 1) {
        return -1;
    }
    for (int i = 1; i < n; ++i) {
        if (arr[i] % 2 == arr[i - 1] % 2) {
            return i;
        }
    }
    return -1;
}

int main() {
    vector<int> arr1({1, 2, 5, 3, 6});
    vector<int> arr2({1, 4, 7, 2, 5, 6});
    cout << solution(arr1) << endl;
    cout << solution(arr2) << endl;
}