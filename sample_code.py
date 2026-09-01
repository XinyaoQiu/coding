import time as _time
import uuid
from collections import OrderedDict, defaultdict, deque

import redis

WINDOW = 60
LIMIT = 5
BURST = 5
REFILL = LIMIT / WINDOW


# ============ coding round: in-memory, single process ============

_history = defaultdict(deque)
_bucket = {}
_dedup = {}


def sliding_window(user_id, t):
    q = _history[user_id]
    while q and q[0] <= t - WINDOW:
        q.popleft()
    if len(q) >= LIMIT:
        return False
    q.append(t)
    return True


def token_bucket(user_id, t):
    tokens, last = _bucket.get(user_id, (BURST, t))
    tokens = min(BURST, tokens + (t - last) * REFILL)
    if tokens < 1:
        _bucket[user_id] = (tokens, t)
        return False
    _bucket[user_id] = (tokens - 1, t)
    return True


def sliding_window_dedup(user_id, req_id, t):
    d = _dedup.setdefault(user_id, OrderedDict())
    while d and next(iter(d.values())) <= t - WINDOW:
        d.popitem(last=False)
    if req_id in d:
        return True
    if len(d) >= LIMIT:
        return False
    d[req_id] = t
    return True


# ============ system design round: Redis, shared across instances ============

r = redis.Redis(host="localhost", port=6379, decode_responses=True)


def sliding_window_redis(user_id, now=None):
    now = _time.time() if now is None else now
    key = f"rl:sw:{user_id}"
    member = f"{now}:{uuid.uuid4().hex}"

    p = r.pipeline()
    p.zremrangebyscore(key, "-inf", now - WINDOW)  # ZREMRANGEBYSCORE key -inf <now-W>
    p.zadd(key, {member: now})                     # ZADD key <now> <member>
    p.zcard(key)                                   # ZCARD key
    p.expire(key, WINDOW)                          # EXPIRE key <W>
    count = p.execute()[2]

    if count > LIMIT:
        r.zrem(key, member)                        # ZREM key <member>
        return False
    return True


def token_bucket_redis(now=None):
    now = _time.time() if now is None else now
    key = "rl:tb:global"

    tokens, last = r.hmget(key, "tokens", "last")  # HMGET key tokens last
    tokens = BURST if tokens is None else float(tokens)
    last = now if last is None else float(last)

    tokens = min(BURST, tokens + (now - last) * REFILL)
    if tokens < 1:
        r.hset(key, mapping={"tokens": tokens, "last": now})
        return False
    r.hset(key, mapping={"tokens": tokens - 1, "last": now})
    return True


SLIDING_WINDOW_LUA = """
local key, limit, window = KEYS[1], tonumber(ARGV[1]), tonumber(ARGV[2])
local t = redis.call('TIME')
local now = tonumber(t[1]) * 1000 + math.floor(tonumber(t[2]) / 1000)

redis.call('ZREMRANGEBYSCORE', key, '-inf', now - window)
if redis.call('ZCARD', key) >= limit then return 0 end
redis.call('ZADD', key, now, ARGV[3])
redis.call('PEXPIRE', key, window)
return 1
"""
