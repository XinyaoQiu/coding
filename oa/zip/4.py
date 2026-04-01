def solution(queries):
    parents = {}
    sizes = {}
    ans = []

    def find(x):
        while x != parents[x]:
            parents[x] = parents[parents[x]]
            x = parents[x]
        return x
    
    def union(x, y):
        root_x, root_y = find(x), find(y)
        if root_x == root_y:
            return
        parents[root_x] = root_y
        sizes[root_y] += sizes[root_x]
    
    def max_size():
        sz = 0
        for _, root in parents.items():
            sz = max(sz, sizes[root])
        return sz

    for x in queries:
        parents[x] = x
        sizes[x] = 1
        if x - 1 in parents:
            union(find(x - 1), x)
        if x + 1 in parents:
            union(find(x + 1), x)
        ans.append(max_size())
    return ans

print(solution([1, 4, 2, 5, 6, 3]))