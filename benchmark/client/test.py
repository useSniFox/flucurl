import httpx
import time

def test():
    print("Test python")
    client = httpx.Client()
    url = "http://localhost:8080/size/1"
    t = time.time()
    for i in range(10000):
        response = client.get(url)
        data = response.content
        assert response.status_code == 200
    print("Small File Time: ", time.time() - t)
    url = "http://localhost:8080/size/10000"
    t = time.time()
    for i in range(100):
        response = client.get(url)
        data = response.content
        assert response.status_code == 200
    print("Large File Time: ", time.time() - t)

if __name__ == "__main__":
    test()
