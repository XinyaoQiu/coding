import re
from collections import Counter

log_example = """
192.168.1.1 - - [25/Oct/2025:10:12:34] "GET /index.html"
10.0.0.2 - - [25/Oct/2025:10:12:40] "POST /login"
192.168.1.1 - - [25/Oct/2025:10:13:00] "GET /home"
172.16.0.3 - - [25/Oct/2025:10:13:10] "GET /profile"
192.168.1.1 - - [25/Oct/2025:10:13:50] "GET /dashboard"
10.0.0.2 - - [25/Oct/2025:10:14:00] "GET /logout"
172.16.0.3 - - [25/Oct/2025:10:14:05] "GET /home"
"""

ip_pattern = r'\b(?:[0-9]{1,3}\.){3}[0-9]{1,3}\b'
ips = re.findall(ip_pattern, log_example)
counter = Counter(ips)
print(counter.most_common(3))