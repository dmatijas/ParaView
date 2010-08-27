print "Tests that piecelists can be serialized and transfered within a process."
print "To run: pvpython, then execfile(\"this script\")"

from paraview import servermanager
connection = servermanager.Connect()
fname = "/Builds/ParaView/devel/streaming_clientside_cache/bin/libStreamingPlugin.dylib"
servermanager.LoadPlugin(fname)
pxm = servermanager.ProxyManager()

#generate a piece list
#DummyFill increments the resolution so the lists are identifiable
#this one's resolution will be zero
pl1 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl1.UpdateProperty("Print",1)
pl1.UpdateProperty("DummyFill", 1)
pl1.UpdateProperty("Print", 1)
print "Output should be a sane looking piecelist"

#verify that it piece list can be serialized
pl1.UpdateProperty("Serialize", 1)
pl1.UpdateProperty("PrintSerialized", 1)
print "Output should be a linearized version of above"

#verify that a list can be copied
pl2 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
cpb = pl2.GetProperty("CopyBuddy")
cpb.AddProxy(pl1)
pl2.UpdateProperty("CopyBuddy", 1)
pl2.UpdateProperty("Print",1)
print "other than the fact that this new piecelist has a different address, its "
print "printed contents should be the same as the original, in particular the "
print "resolution should still be zero"

#make some more piece lists that differ from the above
pl3 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl3.UpdateProperty("DummyFill", 1)
#this one's resolution will be one
pl3.UpdateProperty("Print",1)

pl4 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl4.UpdateProperty("DummyFill", 1)
#this one's resolution will be two
pl4.UpdateProperty("Print",1)

#make an SUS, and add the piece lists to it
sus1 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
sus1.UpdateProperty("PrintPieceLists", 1)
#should have no local or remote piece lists yet

apl = sus1.GetProperty("AddPieceList")
apl.AddProxy(pl1)
sus1.UpdateProperty("AddPieceList",1)

apl.RemoveAllProxies()
apl.AddProxy(pl3)
sus1.UpdateProperty("AddPieceList",1)

apl.RemoveAllProxies()
apl.AddProxy(pl4)
sus1.UpdateProperty("AddPieceList",1)

#inspect them
sus1.UpdateProperty("PrintPieceLists", 1)
print "The SUS should now have three remote piece lsts, with res 0,1 and 2"

#serialize the piecelists in the SUS en masse
sus1.UpdateProperty("Serialize", 1)
sus1.UpdateProperty("PrintSerialized", 1)
print "The output should be a linearized version of all of the above"

#make a new SUS
sus2 = pxm.SMProxyManager.NewProxy("filters", "StreamingUpdateSuppressor")
sus2.UpdateProperty("PrintPieceLists", 1)
print "This new SUS should have no local or remote piece lists yet"

#copy all of the first SUS's piece lists to this one
cpb = sus2.GetProperty("CopyBuddy")
cpb.AddProxy(sus1)
sus2.UpdateProperty("CopyBuddy", 1)
sus2.UpdateProperty("PrintPieceLists", 1)
print "The new SUS should now have the same content (but with all different"
print "addresses)"
print "I do not remember what my intent was but copying has a side effect"
print "of making the local piece list a copy of the first parallel piece list"

#add another piecelist and redo the copy to make sure nothing gets lost
pl5 = pxm.SMProxyManager.NewProxy("helpers", "PieceList")
pl5.UpdateProperty("DummyFill", 1)
#this one's resolution will be three
pl5.UpdateProperty("Print",1)
print "This is a new piece list with resolution three"

apl.RemoveAllProxies()
apl.AddProxy(pl5)
sus1.UpdateProperty("AddPieceList",1)
sus2.UpdateProperty("CopyBuddy", 1)
sus2.UpdateProperty("PrintPieceLists", 1)
print "The SUS should have the same content as before, with one additional "
print "piecelist, this one's resolution is 3"
