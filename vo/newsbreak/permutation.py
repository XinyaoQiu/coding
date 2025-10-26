def permute(nums):
    path = nums.copy()
    ans = set()

    def backtrack(i):
        if i == len(nums):
            ans.add(tuple(path))
            return
        for j in range(i, len(nums)):
            path[i], path[j] = path[j], path[i]
            backtrack(i + 1)
            path[i], path[j] = path[j], path[i]
    
    backtrack(0)
    return ans

nums = [1, 3, 2, 1]
print(permute(nums))