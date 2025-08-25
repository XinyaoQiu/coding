#include <vector>
using namespace std;

class Solution {
public:
    vector<int> asteroidCollision(vector<int>& asteroids) {
        vector<int> stk;
        for (int asteroid : asteroids) {
            bool alive = true;
            while (alive && !stk.empty() && stk.back() > 0 && asteroid < 0) {
                alive = stk.back() < -asteroid;
                if (stk.back() <= -asteroid) {
                    stk.pop_back();
                }
            }
            if (alive) {
                stk.push_back(asteroid);
            }
        }
        return stk;
    }
};