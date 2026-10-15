#include <iostream>
#include <vector>
#include <sstream>

using namespace std;

int process(vector<int>& arr) {
    int left = 0;
    int right = arr.size() - 1;
    while (left < right) {
        int mid = left + (right - left) / 2;
        if (arr[mid] < arr[mid + 1]) {
            left = mid + 1;
        } 
        else {
            right = mid;
        }
    }
    return arr[left];
}

int main() {
    string line;
    getline(cin, line);
    vector<int> arr;
    istringstream iss(line);
    int num;
    while (iss >> num) {
        arr.push_back(num);
    }
    cout << process(arr) << endl;
}