#!/bin/python3

import xmlrpc.client

proxy = xmlrpc.client.ServerProxy("http://localhost:2305/XmlRPC")

try:
    proxy.test({'test': 111})
except xmlrpc.client.ProtocolError as err:
    print("A protocol error occurred")
    print("URL: %s" % err.url)
    print("HTTP/HTTPS headers: %s" % err.headers)
    print("Error code: %d" % err.errcode)
    print("Error message: %s" % err.errmsg)
