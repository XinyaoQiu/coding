from collections import defaultdict, deque

def evaluate_statements(statements):
    edges = defaultdict(list)
    indegs = defaultdict(int)
    expr_map = {}
    variables = set()
    
    for stmt in statements:
        val, expr = stmt.split("=")
        val = val.strip()
        expr = expr.strip()
        variables.add(val)
        expr_map[val] = expr
        
        token = ""
        for c in expr + "+":
            if c.isalpha():
                token += c
            elif token:
                edges[token].append(val)
                indegs[val] += 1
                variables.add(token)
                token = ""

    queue = deque([v for v in variables if indegs[v] == 0])
    values = {}

    while queue:
        u = queue.popleft()
        values[u] = eval(expr_map[u], {}, values)
        for v in edges[u]:
            indegs[v] -= 1
            if indegs[v] == 0:
                queue.append(v)    
    
    return values

statements = ["a = br + c", "br = 4", "c = br - 1", "d = a - br"]
print(evaluate_statements(statements))