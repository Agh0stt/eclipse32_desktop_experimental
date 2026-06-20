Sample layout for an .INS package:

  install.cfg    — manifest (DEST, APP, VERSION)
  HELLO.E32      — copy your built app here before packing

Build the package from the Eclipse32 tree:

  python3 tools/pack_ins.py tools/sample_ins build/DEMO.INS

Then copy build/DEMO.INS onto the disk image and run:

  install /DEMO.INS

The installer extracts files under DEST/, removes the .INS file, and leaves the app on disk.
