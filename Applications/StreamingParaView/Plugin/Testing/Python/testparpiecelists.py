#execfile("/Source/ParaView/gitDevel/CSStreaming/Applications/StreamingParaView/Plugin/Testing/Python/testparpiecelists.py")

#This test is used to verify that piece lists can be generated on the server,
#transfered to the client, and back

from paraview import servermanager
connection = servermanager.Connect("localhost")
servermanager.LoadPlugin("/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib")
servermanager.LoadPlugin("/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib", 0)

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
sus1.UpdateProperty("Serialize", 1)
sprop = sus1.GetProperty("SerializedLists")
sus1.UpdatePropertyInformation(sprop)
sprop.GetNumberOfElements()
thelist = sprop.GetElement(0)

#transfer them (from root node of server) to client and print them
sus2 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
sus2.SetServers(0x10)
psprop = sus2.GetProperty("UnSerialize")
psprop.SetElement(0, thelist)
sus2.UpdateProperty("UnSerialize", 1)
sus2.UpdateProperty("PrintPieceLists", 1)

#grab them off of the client
sus2.UpdateProperty("Serialize", 1)
sprop = sus2.GetProperty("SerializedLists")
sus2.UpdatePropertyInformation(sprop)
sprop.GetNumberOfElements()
thelist2 = sprop.GetElement(0)

#send them to another SUS that lives on the (root node of the) server
sus3 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
sus3.SetServers(0x02)
psprop = sus3.GetProperty("UnSerialize")
psprop.SetElement(0, thelist)
sus3.UpdateProperty("UnSerialize", 1)
sus3.UpdateProperty("PrintPieceLists", 1)
