#include <iostream>
#include <vector>

using namespace std;

vector<int> checkSamePoints(vector<int>& arr, int N) {
    vector<int> result;
    for (int i = 0; i < N - 1; i++) {
        if (arr[i] == arr[i+1]) {
            result.push_back(i);
        }
    }
    return result;
}

long calculate(vector<int>& arr, int L, int R) {
    if (L >= R) {
        return 0;
    }
    long result = 0;
    for (int i = L; i <= R; i++) {
        for (int j = i + 1; j <= R; j++) {
            long sum = 0;
            for (int k = i; k <= j; k++) {
                sum += arr[k];
            }
            result += sum;
        }
    }
    return result;
}

int main() {
    int N;
    cin >> N;
    vector<int> arr;
    for (auto i = 0; i < N; i++) {
        int num;
        cin >> num;
        arr.push_back(num);
    }
    long result = 0;
    vector<int> samePoints = checkSamePoints(arr, N);
    vector<vector<int>> arrs;
    int index = 0;
    if (samePoints.empty()) {
        result = calculate(arr, 0, N - 1);
    } else {
        for (auto i : samePoints) {
            result += calculate(arr, index, i);
            index = i + 1;
        }
    }
    cout << result % 10000007 << endl;
}