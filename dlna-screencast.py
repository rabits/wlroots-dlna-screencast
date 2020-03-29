#!/usr/bin/env python3
import upnpclient

import atexit
from urllib.request import pathname2url
from multiprocessing import Process, Queue

# --- HTTP SERVER ---

import os
import posixpath
import socket
import subprocess
import sys

from http.server import BaseHTTPRequestHandler
import socketserver as SocketServer
from urllib.parse import unquote

import subprocess

class RangeHTTPServer(BaseHTTPRequestHandler):
    @classmethod
    def start(cls, filename, allowed_host=None, queue=None):
        os.chdir(os.path.dirname(filename))

        httpd = SocketServer.TCPServer(('', 0), cls)
        httpd.allowed_filename = os.path.realpath(filename)
        httpd.allowed_host = allowed_host

        if queue:
            queue.put(httpd.server_address)

        try:
            httpd.serve_forever()
        except:  # NOQA
            # Stop DLNA play on exception
            d.AVTransport.Stop(InstanceID=0)
            pass

    def handle(self):   # pragma: no cover
        """Handle requests.
        We override this because we need to work around a bug in some
        versions of Python's SocketServer :(
        See http://bugs.python.org/issue14574
        """

        self.close_connection = 1

        try:
            self.handle_one_request()
        except socket.error as exc:
            if exc.errno == 32:
                pass

    def do_HEAD(self):
        """Handle a HEAD request"""
        try:
            #path, stats = self.check_path(self.path)
            pass
        except ValueError:
            return

        self.send_response(200)
        self.send_header("Accept-Ranges", "bytes")
        self.end_headers()

    def do_GET(self):
        """Handle a GET request with some support for the Range header"""
        global _http_server
        try:
            #path, stats = self.check_path(self.path)
            pass
        except ValueError:
            return

        # assume we are sending the whole file first
        ranges = None

        # but see if a Range: header tell us differently
        try:
            ranges = self.headers.get('range')
        except ValueError:
            # this can get raised if the Range request is weird like bytes=2-1
            # not sure why this doesn't raise as a ParseError, but whatevs
            self.send_error(400, "Bad Request")
            return

        try:
            if ranges is None:
                self.send_response(200)
            else:
                self.send_response(206)
                self.send_header("Content-Range", ranges)
                pass

            #self.send_header("Transfer-Encoding", "chunked")
            #self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            print("Ranges:", ranges)

            while True:
                cmd = ("./wlroots-screen-record", "-s", "-c", "-o", "2", "-d", "3")

                process = subprocess.Popen(
                    cmd,
                    stdout=subprocess.PIPE,
                    #stderr=subprocess.PIPE,
                    shell=False,
                    #env=env,
                    #cwd=tempfile.gettempdir()
                )

                print('Capturing running')

                retcode = process.poll()
                while retcode is None:
                    retcode = process.poll()

                    #print("Reading chunk...")
                    chunk = process.stdout.read(512)
                    #print(len(chunk))

                    if not chunk:
                        continue

                    try:
                        self.wfile.write(chunk)
                    except Exception as e:
                        print("Exception during sending the chunk: %s" % (e,))
                        process.kill()
                        process.communicate()
                        print("End stream")
                        return

                print('Capturing stoped')
        except EnvironmentError:
            self.send_error(500, "Internal Server Error")
            return

    def check_path(self, path):
        """Verify that the client and server are allowed to access `path`
        Args:
            path(str): The path from an HTTP rqeuest, it will be joined to os.getcwd()
        Returns:
            (str, stats):    An abosolute path to the file on disk, and the result of os.stat()
        Raises:
            ValueError:     The path could not be accessed (exception will say why)
        """

        # get full path to file requested
        path = posixpath.normpath(unquote(path))
        path = os.path.join(os.getcwd(), path.lstrip('/'))

        # if we have an allowed host, then only allow access from it
        if self.server.allowed_host and self.client_address[0] != self.server.allowed_host:
            self.send_error(400, "Bad Request")
            raise ValueError('Client is not allowed')

        # don't do directory indexing
        if os.path.isdir(path):
            self.send_error(400, "Bad Request")
            raise ValueError("Requested path is a directory")

        # if they try to request something else, don't serve it
        if path != self.server.allowed_filename:
            self.send_error(400, "Bad Request")
            raise ValueError("Requested path was not in the allowed list")

        # make sure we can stat and open the file
        try:
            stats = os.stat(path)
            fh = open(path, 'rb')
        except (EnvironmentError) as exc:
            self.send_error(500, "Internal Server Error")
            raise ValueError("Unable to access the path: {0}".format(exc))
        finally:
            try:
                fh.close()
            except NameError:
                pass

        return path, stats

# --- HTTP SERVER ---

_http_server = None

def serve(path):
    global _http_server
    q = Queue()
    _http_server = Process(target=RangeHTTPServer.start, args=(path, None, q))
    _http_server.start()
    #atexit.register(lambda: _http_server.terminate())
    # TODO: replace to the actual ip address
    server_address = ('workstation-1', q.get(True)[1])

    return 'http://{0}:{1}/{2}'.format(
        server_address[0],
        server_address[1],
        pathname2url(os.path.basename(path))
    )

d = upnpclient.Device("http://miracast-sidescreen:60099/")

d.AVTransport.Stop(InstanceID=0)
d.AVTransport.Stop(InstanceID=0)
print("Stopped previous")
uri = serve("./stream.mkv")
d.AVTransport.SetAVTransportURI(InstanceID=0, CurrentURI=uri, CurrentURIMetaData="")
d.AVTransport.Play(InstanceID=0, Speed="1")
print("Play started %s" % uri)
