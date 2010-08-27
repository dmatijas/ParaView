print "This test is used to verify that piece lists can be generated on"
print "the server transfered to the client, and back"
print "To run: start up a one process pvserver, then run pvpython and"
print "execfile(\"this script\")"

from paraview import servermanager
connection = servermanager.Connect("localhost")
servermanager.LoadPlugin("/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib", 0)
servermanager.LoadPlugin("/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib", 1)

pxm = servermanager.ProxyManager()

#generate some piecelists on the server
pl1 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl1.UpdateProperty("DummyFill", 1)
pl2 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl2.UpdateProperty("DummyFill", 1)

#add them to an SUS that lives on the server and print them
sus1 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
apl = sus1.GetProperty("AddPieceList")
apl.AddProxy(pl1)
sus1.UpdateProperty("AddPieceList",1)
apl.RemoveAllProxies()
apl.AddProxy(pl2)
sus1.UpdateProperty("AddPieceList",1)
sus1.UpdateProperty("PrintPieceLists", 1)
print "the server should now print its SUS, which has no local piecelist and "
print "two remote piecelists, which has res of 0 and 1 respectively"

#now serialize that remote SUS's piece lists.
sus1.UpdateProperty("Serialize", 1)
sus1.UpdateProperty("PrintSerialized", 1)
print "The remote just printed its contents, serialized."

#print now bring them back to the client as a string
sprop = sus1.GetProperty("SerializedLists")
sus1.UpdatePropertyInformation(sprop)
sprop.GetNumberOfElements()
thelist = sprop.GetElement(0)
print "remote has "
print thelist

#transfer them (from root node of server) to client and print them
sus2 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
#if not otherwise specified proxy's are instantiated on data server
#here I am specifying that this proxy should be instantiated on the client
sus2.SetServers(0x10)
psprop = sus2.GetProperty("UnSerialize")
psprop.SetElement(0, thelist)
sus2.UpdateProperty("UnSerialize", 1)
sus2.UpdateProperty("PrintPieceLists", 1)
print "Now the client should have the same content as the server (except addresses)"

#grab them off of the client
sus2.UpdateProperty("Serialize", 1)
sprop = sus2.GetProperty("SerializedLists")
sus2.UpdatePropertyInformation(sprop)
sprop.GetNumberOfElements()
thelist2 = sprop.GetElement(0)
print "client now has "
print thelist2

#send them to another SUS that lives on the (root node of the) server
sus3 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
#here I am specifying that the proxy should only be instantiated on the root node
sus3.SetServers(0x02)
psprop = sus3.GetProperty("UnSerialize")
psprop.SetElement(0, thelist)
sus3.UpdateProperty("UnSerialize", 1)
sus3.UpdateProperty("PrintPieceLists", 1)
print "This should be the same as before, except a new address"
