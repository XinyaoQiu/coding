def ip_to_int(ip):
    parts = list(map(int, ip.split('.')))
    res = 0
    for part in parts:
        res = (res << 8) + part
    return res

def cidr_to_range(cidr):
    ip, mask_len = cidr.split('/')
    mask_len = int(mask_len)
    ip_int = ip_to_int(ip)
    mask = ((1 << 32) - 1) ^ ((1 << (32 - mask_len)) - 1)
    start = ip_int & mask
    end = start + (1 << (32 - mask_len)) - 1
    return start, end

def check_ip_access(rules, ip):
    ip_int = ip_to_int(ip)
    for action, cidr in rules:
        start, end = cidr_to_range(cidr)
        if start <= ip_int <= end:
            return action
    return ""

def check_cidr_access(rules, target_cidr):
    target_start, target_end = cidr_to_range(target_cidr)
    for action, cidr in rules:
        start, end = cidr_to_range(cidr)
        if target_start <= start and end <= target_end:
            return action
    return ""

rules = [
    ("allow", "192.168.1.0/24"),
    ("deny", "192.168.0.0/16"),
    ("deny", "10.0.0.0/8"),
]

print(check_ip_access(rules, "192.168.1.100"))  # allow
print(check_ip_access(rules, "10.0.0.3"))       # deny
print(check_ip_access(rules, "8.8.8.8"))        # ""

print(check_cidr_access(rules, "192.168.1.0/23"))  # allow
print(check_cidr_access(rules, "10.0.24.1/5"))     # deny
print(check_cidr_access(rules, "8.8.8.0/24"))      # ""