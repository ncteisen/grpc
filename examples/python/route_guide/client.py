from __future__ import print_function
import time
import grpc
import route_guide_pb2
import route_guide_pb2_grpc

def guide_route_chat(stub):
    req = route_guide_pb2.RouteNote(message="msg")
    i = 0
    for response in stub.RouteChat(iter([req])):
        print("%d: %d" % (time.time(), i))
        time.sleep(10)
        i += 1

def run():
    channel = grpc.insecure_channel('localhost:50051')
    stub = route_guide_pb2_grpc.RouteGuideStub(channel)
    guide_route_chat(stub)

if __name__ == '__main__':
    run()
