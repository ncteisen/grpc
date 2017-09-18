from __future__ import print_function
from concurrent import futures
import time
import grpc
import route_guide_pb2
import route_guide_pb2_grpc

_ONE_DAY_IN_SECONDS = 60 * 60 * 24

class RouteGuideServicer(route_guide_pb2_grpc.RouteGuideServicer):

    def RouteChat(self, request_iterator, context):
        i = 0
        while True:
            print("%d: %d" % (time.time(), i))
            yield route_guide_pb2.RouteNote(
                message = "X" * (32*1024),
            )
            i += 1

def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    route_guide_pb2_grpc.add_RouteGuideServicer_to_server(
            RouteGuideServicer(), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    try:
        while True:
            time.sleep(_ONE_DAY_IN_SECONDS)
    except KeyboardInterrupt:
        server.stop(0)

if __name__ == '__main__':
    serve()
