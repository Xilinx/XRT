do_install:append(){
  echo "UL0:12345:respawn:/bin/start_getty 115200 ttyUL0 vt102" >> ${D}${sysconfdir}/inittab
}
