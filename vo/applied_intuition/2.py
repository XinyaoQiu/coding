import requests

url = ""

resp = requests.get(url)
data = resp.json()

number = data["number"]