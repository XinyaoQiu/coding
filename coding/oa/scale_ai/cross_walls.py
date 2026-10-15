from bisect import bisect_left

wallXCoordinates = [-3.5, -2.5, -1.5, -0.5, 0.5, 1.5, 2.5, 3.5]
wallThicknesses = [10, 2, 1, 3, 1, 1, 2, 10]
# [1, 2, 4]
# [3, 4, 6]

def numDistinctWalls(wallXCoordinates, wallThicknesses, E):
    posi_walls, nega_walls = [], []
    for x, t in zip(wallXCoordinates, wallThicknesses):
        if x > 0:
            posi_walls.append((x, t))
        else:
            nega_walls.append((-x, t))
    posi_walls.sort()
    nega_walls.sort()
    posi_sums, nega_sums = [], []
    def cal_sums(arr, walls):
        curr_x, curr_sum = 0, 0
        for x, t in walls:
            curr_sum += int(x - 0.5) - curr_x + t
            curr_x = int(x + 0.5)
            if curr_sum > E:
                break
            arr.append(curr_sum)
    cal_sums(posi_sums, posi_walls)
    cal_sums(nega_sums, nega_walls)
    print(f"posi_sums = {posi_sums}, nega_sums = {nega_sums}")
    ans = max(len(posi_sums), len(nega_sums))
    def get_dist_walls(a, b):
        ret = 0
        for i in range(len(a)):
            if 2 * a[i] > E:
                break
            j = bisect_left(b, E - 2 * a[i] + 1) - 1
            ret = max(ret, i + j + 2)
        return ret
    ret1 = get_dist_walls(posi_sums, nega_sums)
    ret2 = get_dist_walls(nega_sums, posi_sums)
    ans = max(max(ret1, ret2), ans)
    return ans

print(numDistinctWalls(wallXCoordinates, wallThicknesses, 8))