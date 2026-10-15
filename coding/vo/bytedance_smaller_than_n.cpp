#include <iostream>
#include <queue>
#include <algorithm>

using namespace std;

// int lower_bound(deque<int>& nums, int target) {
//     int L = 0, R = nums.size() - 1;
//     while (L <= R) {
//         int M = L + (R - L) / 2;
//         if (nums[M] >= target) {
//             R = M - 1;
//         } else {
//             L = M + 1;
//         }
//     }
//     return L;
// }


// int deq_to_num(deque<int>& res) {
//     int n = 0;
//     for (int x : res) {
//         n *= 10;
//         n += x;
//     }
//     return n;
// }

// deque<int> num_to_deq(int n) {
//     deque<int> path;
//     while (n > 0) {
//         path.push_back(n % 10);
//         n /= 10;
//     }
//     reverse(path.begin(), path.end());
//     return path;
// }

// int findAns(deque<int>& nums, int limit) {
//     deque<int> path = num_to_deq(limit);
//     deque<int> res;
//     for (int i = 0; i < path.size(); i++) {
//         int index = lower_bound(nums, path[i] + 1) - 1;
//         res.push_back(nums[index]);
//         if (nums[index] != path[i]) {
//             for (int j = i; j >= 0; j--) {
//                 index = lower_bound(nums, path[j]) - 1;
//                 res[j] = nums[index];
//                 if (index > 0) {
//                     break;
//                 }
//             }
//             if (res[0] == -1) {
//                 res.pop_front();
//             }
//             for (int& x : res) {
//                 if (x == -1) {
//                     x = nums.back();
//                 }
//             }
//             for (int j = i + 1; j < path.size(); j++) {
//                 res.push_back(nums.back());
//             }
//             break;
//         }

//     }
//     return deq_to_num(res);
// }

int lower_bound(deque<int>& nums, int target) {
    int L = 0, R = nums.size() - 1;
    while (L <= R) {
        int M = L + (R - L) / 2;
        if (nums[M] >= target) {
            R = M - 1;
        } else {
            L = M + 1;
        }
    }
    return L;
}

deque<int> num_to_deq(int limit) {
    deque<int> dq;
    while (limit > 0) {
        dq.push_front(limit % 10);
        limit /= 10;
    }
    return dq;
}

int deq_to_num(deque<int>& dq) {
    int n = 0;
    for (int x : dq) {
        n *= 10;
        n += x;
    }
    return n;
}

// nums = [-1, 2, 4, 6, 7]
// limit = [5, 2, 2]
int findAns(deque<int>& nums, int limit) {
    deque<int> dq = num_to_deq(limit);
    deque<int> ans;
    for (int i = 0; i < dq.size(); i++) {
        int index = lower_bound(nums, dq[i] + 1) - 1;
        ans.push_back(nums[index]);
        if (nums[index] != dq[i]) {
            for (int j = i - 1; j >= 0 && index == 0; j--) {
                index = lower_bound(nums, dq[j]) - 1;
                ans[j] = nums[index];
            }
            if (ans[0] == -1) {
                ans.pop_front();
            }
            for (int& x : ans) {
                if (x == -1) {
                    x = nums.back();
                }
            }
            for (int j = i + 1; j < dq.size(); j++) {
                ans.push_back(nums.back());
            }
            break;
        }
    }
    return deq_to_num(ans);
}


int main() {
    deque<int> nums({ 4, 2, 6, 7 });
    sort(nums.begin(), nums.end());
    nums.push_front(-1);
    for (int i = 1; i <= 80000; i++) {
        cout << "i = " << i << ", ans = " << findAns(nums, i) << endl;
    }
}