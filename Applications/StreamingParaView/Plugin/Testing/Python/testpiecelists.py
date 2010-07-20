#tests that piece lists can be serialized and transfered within a process
#execfile("/Source/ParaView/gitDevel/CSStreaming/Applications/StreamingParaView/Plugin/Testing/Python/testpiecelists.py")
#execfile("C:\Documents and Settings\demarle\Desktop\Source\CSStreaming\CSStreaming\Applications\StreamingParaView\Plugin\Testing\Python\testpiecelists.py")

from paraview import servermanager
connection = servermanager.Connect()
fname = "C:/Documents and Settings/demarle/Desktop/Builds/ParaView/gitDevel/CSStreaming/bin/Debug/StreamingPlugin.dll"
#fname = "/Builds/ParaView/CSStreaming/buildAll/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(fname)
pxm = servermanager.ProxyManager()

#generate a piece list
pl1 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl1.UpdateProperty("Print",1)
pl1.UpdateProperty("DummyFill", 1)
pl1.UpdateProperty("Print", 1)

#verify that it piece list can be serialized and transfered to another piecelist
pl1.UpdateProperty("Serialize", 1)
pl1.UpdateProperty("PrintSerialized", 1)
pl2 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
cpb = pl2.GetProperty("CopyBuddy")
cpb.AddProxy(pl1)
pl2.UpdateProperty("CopyBuddy", 1)

#make some more piece lists that differ from the above
pl3 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl3.UpdateProperty("DummyFill", 1)
pl4 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl4.UpdateProperty("DummyFill", 1)

#make an SUS, and add the piece lists to it
sus1 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
sus1.UpdateProperty("PrintPieceLists", 1)

apl = sus1.GetProperty("AddPieceList")
apl.AddProxy(pl1)
sus1.UpdateProperty("AddPieceList",1)

apl.RemoveAllProxies()
apl.AddProxy(pl2)
sus1.UpdateProperty("AddPieceList",1)

apl.RemoveAllProxies()
apl.AddProxy(pl3)
sus1.UpdateProperty("AddPieceList",1)

#inspect them
sus1.UpdateProperty("PrintPieceLists", 1)

#serialize the piecelists and copy them to another SUS
sus1.UpdateProperty("Serialize", 1)
sus1.UpdateProperty("PrintSerialized", 1)

sus2 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
sus2.UpdateProperty("PrintPieceLists", 1)
cpb = sus2.GetProperty("CopyBuddy")
cpb.AddProxy(sus1)
sus2.UpdateProperty("CopyBuddy", 1)

#add another piecelist and redo the copy to make sure nothing gets lost
apl.RemoveAllProxies()
apl.AddProxy(pl4)
sus1.UpdateProperty("AddPieceList",1)
sus2.UpdateProperty("CopyBuddy", 1)
