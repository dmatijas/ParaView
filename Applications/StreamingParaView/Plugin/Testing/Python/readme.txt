OK testpiecelists.py
  verifies that you can be serialized and copied between SUSs in the same process
  client leaks

OK testparpiecelists.py
  verifies that piecelists can me moved from parallel server to client and back
  client leaks

OK testparpiece_merging.py
  test that the parallel piece cache filter can take a multipiecedataset
  client leaks

OK testparpiece_caching.py
  test that the client can reuse pieces it has cached without involving the server
  client does not leak

OK testmovedata
  test that the server can parallel data to the client and so that the client can reconstruct it
  client does not leak
  changes to MPIMoveData were incompatible (my virtual functions were
  no longer overrides of ones in MPIMove Data since those ones
  changed args from dataset to dataobject)
  now works modulo the fact that it makes only 1/nth of the total data
  set where N is controlled by StreamedPasses in vtkStreamingOptions.cxx

OK testserialflow.py
  test how meta information should be evaluated and data should flow in builtin mode

OK test remoterenderflow
  test how meta information should be evaluated and data should flow
  when connected to a parallel server which is doing rendering
 clietn mem leaks

OK testdeliveredflow
  test how meta information should be evaluated and data should flow
  when connected to a parallel server which is sending geometry to
  client to render
  the client leaks and crashes on shutdown, I have a feeling this is
  because of the funky things I am doing with proxies in python so I
  am not going to worry about it now
