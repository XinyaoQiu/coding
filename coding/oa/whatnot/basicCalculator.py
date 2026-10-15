def basicCalculator(expression):
    ops = [1]
    sign, ans, i, n = 1, 0, 0, len(expression)
    while i < n:
        if expression[i] == ' ':
            i += 1
        elif expression[i] == '+':
            sign = ops[-1]
            i += 1
        elif expression[i] == '-':
            sign = -ops[-1]
            i += 1
        elif expression[i] == '(':
            ops.append(sign)
            i += 1
        elif expression[i] == ')':
            ops.pop()
            i += 1
        else:
            s = ''
            while i < n and expression[i].isdigit():
                s += expression[i]
                i += 1
            ans += sign * int(s)
    return ans

print(basicCalculator('1 - (-1 - 3) + 4'))
