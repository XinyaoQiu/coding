class Solution {
public:
    bool isPalindrome(int x) {
        long long num1 = x, num2 = 0;
        while (x > 0) {
            num2 = num2 * 10 + x % 10;
            x /= 10;
        }
        return num1 == num2;
    }
};