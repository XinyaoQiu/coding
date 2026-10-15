from enum import Enum
from abc import ABC, abstractmethod
from typing import *

class QuestionType(Enum):
    TEXT = 'TEXT'
    CHOICES = 'CHOICES'
    NUMERIC = 'NUMERIC'

class Question:
    def __init__(self, type: QuestionType, prompt: str, options=None):
        self.type = type
        self.prompt = prompt
        self.options = options

class Answer:
    def __init__(self, value=None):
        self.value = value

class ConditionCheck(Enum):
    EQUAL = 'EQUAL'
    NOT_EQUAL = 'NOT_EQUAL'
    GREATER_THAN = 'GREATER_THAN'
    LESS_THAN = 'LESS_THAN'

class Condition:
    def __init__(self, question: Question, answer: Answer, condition_check: ConditionCheck, value=None):
        self.question = question
        self.answer = answer
        self.condition_check = condition_check
        self.value = value

def is_valid(condition: Condition) -> bool:
    q, a, cond_c, v = condition.question, condition.answer, condition.condition_check, condition.value
    if cond_c == ConditionCheck.EQUAL:
        return v == a.value
    elif cond_c == ConditionCheck.NOT_EQUAL:
        return v != a.value
    elif q.type == QuestionType.NUMERIC and cond_c == ConditionCheck.GREATER_THAN:
        return v > a.value
    elif q.type == QuestionType.NUMERIC and cond_c == ConditionCheck.LESS_THAN:
        return v < a.value

def test_numeric_equal():
    q = Question(QuestionType.NUMERIC, "2 + 2 = ?")
    a1, a2 = Answer(4), Answer(5)
    cond1 = Condition(q, a1, ConditionCheck.EQUAL, 4)
    cond2 = Condition(q, a2, ConditionCheck.EQUAL, 4)
    
    assert is_valid(cond1) is True
    assert is_valid(cond2) is False

def test_numeric_not_equal():
    q = Question(QuestionType.NUMERIC, "2 + 2 != ?")
    a1, a2 = Answer(5), Answer(4)
    cond1 = Condition(q, a1, ConditionCheck.NOT_EQUAL, 4)
    cond2 = Condition(q, a2, ConditionCheck.NOT_EQUAL, 4)
    
    assert is_valid(cond1) is True
    assert is_valid(cond2) is False

def test_numeric_greater_than():
    q = Question(QuestionType.NUMERIC, "2 + 2 > ?")
    a1, a2 = Answer(1), Answer(5)
    cond1 = Condition(q, a1, ConditionCheck.GREATER_THAN, 4)
    cond2 = Condition(q, a2, ConditionCheck.GREATER_THAN, 4)
    
    assert is_valid(cond1) is True
    assert is_valid(cond2) is False

def test_numeric_less_than():
    q = Question(QuestionType.NUMERIC, "2 + 2 < ?")
    a1, a2 = Answer(5), Answer(1)
    cond1 = Condition(q, a1, ConditionCheck.LESS_THAN, 4)
    cond2 = Condition(q, a2, ConditionCheck.LESS_THAN, 4)
    
    assert is_valid(cond1) is True
    assert is_valid(cond2) is False


class LogicalOp(Enum):
    AND = "AND"
    OR = "OR"

class ConditionBase(ABC):
    @abstractmethod
    def evaluate(self) -> bool:
        ...

class LeafCondition(ConditionBase):
    def __init__(self, condition: Condition):
        self.condition = condition
    
    def evaluate(self) -> bool:
        return is_valid(self.condition)

class CompositeCondition(ConditionBase):
    def __init__(self, children: List[ConditionBase], op: LogicalOp):
        self.children = children
        self.op = op

    def evaluate(self) -> bool:
        results = [child.evaluate() for child in self.children]
        if self.op == LogicalOp.AND:
            return all(results)
        elif self.op == LogicalOp.OR:
            return any(results)
    
def test_composite_condition_and():
    q1 = Question(QuestionType.NUMERIC, "2 + 2 = ?")
    q2 = Question(QuestionType.TEXT, "fruit?")
    q3 = Question(QuestionType.CHOICES, "Which one is best?", [1, 2, 3, 4])
    
    a1 = Answer(4)
    a2 = Answer("apple")
    a3 = Answer(3)
    
    leaf_cond1 = LeafCondition(Condition(q1, a1, ConditionCheck.EQUAL, 4))
    leaf_cond2 = LeafCondition(Condition(q2, a2, ConditionCheck.EQUAL, "apple"))
    leaf_cond3 = LeafCondition(Condition(q3, a3, ConditionCheck.EQUAL, 2))

    composite_cond1 = CompositeCondition([leaf_cond1, leaf_cond2, leaf_cond3], LogicalOp.OR)
    assert composite_cond1.evaluate() is True