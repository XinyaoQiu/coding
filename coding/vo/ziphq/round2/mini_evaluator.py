import ast

class Evaluator(ast.NodeVisitor):
    def __init__(self, variables):
        self.variables = variables

    def visit_BinOp(self, node):
        left = self.visit(node.left)
        right = self.visit(node.right)

        if isinstance(node.op, ast.Add):
            return left + right
        if isinstance(node.op, ast.Sub):
            return left - right
        if isinstance(node.op, ast.Mult):
            return left * right
        if isinstance(node.op, ast.Div):
            return left / right
        raise Exception("Unsupported operator")

    def visit_Name(self, node):
        if node.id not in self.variables:
            raise Exception(f"Undefined variable {node.id}")
        return self.visit(self.variables[node.id])

    def visit_Constant(self, node):
        return node.value

    def visit_Dict(self, node):
        result = {}
        for key_node, val_node in zip(node.keys, node.values):
            key = self.visit(key_node)
            val = self.visit(val_node)
            result[key] = val
            self.variables[key] = val 
        return result

    def generic_visit(self, node):
        raise Exception(f"Unsupported syntax: {type(node).__name__}")


def evaluate_expr(expr):
    tree = ast.parse(expr, mode="eval")
    variables = {k.value: v for k, v in zip(tree.body.keys, tree.body.values)}
    print(variables)
    evaluator = Evaluator(variables)
    return evaluator.visit(tree.body)

expr = '{"a": b + 1, "b": 1}'
print(evaluate_expr(expr))