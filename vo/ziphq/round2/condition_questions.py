from enum import Enum
from typing import *

class QuestionType(Enum):
    MULTIPLE_CHOICE = "multiple_choice"
    TEXT = "text"
    NUMERIC = "numeric"

class ConditionCheck(Enum):
    EQUAL = "equal"
    NOT_EQUAL = "not_equal"
    GREATER_THAN = "greater_than"

class LogicalOp(Enum):
    AND = "and"
    OR = "or"

class Question:
    def __init__(self, qid: str, qtype: QuestionType, prompt: str, value=None):
        self.qid = qid
        self.qtype = qtype
        self.prompt = prompt
        self.value = value
    
    def __repr__(self):
        return f"Question({self.qid}, {self.qtype}, {self.prompt}, {self.value})"

class Answer:
    def __init__(self, value):
        self.value = value
    
    def __repr__(self):
        return f"Answer({self.value})"

class Condition:
    def __init__(self, question: Question, answer: Answer, check: ConditionCheck):
        self.question = question
        self.answer = answer
        self.check = check

    def is_valid(self) -> bool:
        qval = self.question.value
        aval = self.answer.value

        if self.check == ConditionCheck.EQUAL:
            return qval == aval
        elif self.check == ConditionCheck.NOT_EQUAL:
            return qval != aval
        elif self.check == ConditionCheck.GREATER_THAN:
            try:
                return float(aval) > float(qval)
            except ValueError:
                return False
        else:
            raise ValueError(f"Unknown check type: {self.check}")
    
class CompositeCondition:
    def __init__(self, conditions: List['Condition | CompositeCondition'], op: LogicalOp):
        self.conditions = conditions
        self.op = op

    def evaluate(self) -> bool:
        results = []
        for c in self.conditions:
            if isinstance(c, CompositeCondition):
                results.append(c.evaluate())
            else:
                results.append(c.is_valid())
        if self.op == LogicalOp.AND:
            return all(results)
        elif self.op == LogicalOp.OR:
            return any(results)
        else:
            raise ValueError(f"Unknown logical operator: {self.op}")

def test_eqaul_condition_valid():
    q = Question("q1", QuestionType.TEXT, "fruit?", "apple")
    a = Answer("apple")
    cond = Condition(q, a, ConditionCheck.EQUAL)
    assert cond.is_valid() is True

def test_not_equal_condition_valid():
    q = Question("q1", QuestionType.TEXT, "fruit?", "apple")
    a = Answer("pineapple")
    cond = Condition(q, a, ConditionCheck.NOT_EQUAL)
    assert cond.is_valid() is True

def test_greater_than_condition_valid():
    q = Question("q1", QuestionType.NUMERIC, "age?", "25")
    a = Answer("30")
    cond = Condition(q, a, ConditionCheck.GREATER_THAN)
    assert cond.is_valid() is True

def test_composite_condition():
    q1 = Question("Q1", QuestionType.TEXT, "Fav fruit?", value="apple")
    q2 = Question("Q2", QuestionType.NUMERIC, "Age?", value=25)
    q3 = Question("Q3", QuestionType.TEXT, "Drink coffee?", value="yes")

    a1, a2, a3 = Answer("apple"), Answer("25"), Answer("yes")

    cond1 = Condition(q1, a1, ConditionCheck.EQUAL, "apple")
    cond2 = Condition(q2, a2, ConditionCheck.GREATER_THAN, 18)
    cond3 = Condition(q3, a3, ConditionCheck.NOT_EQUAL, "no")

    group1 = CompositeCondition([cond1, cond2], LogicalOp.AND)
    final = CompositeCondition([group1, cond3], LogicalOp.OR)

    assert final.evaluate() is True