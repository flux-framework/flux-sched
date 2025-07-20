install(PROGRAMS ./rc1.d/01-sched-fluxion
        DESTINATION ${FLUX_RC1_DIR})
install(PROGRAMS ./rc3.d/01-sched-fluxion
        DESTINATION ${FLUX_RC3_DIR})
install(FILES ./modprobe/rc1.d/fluxion.py
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/flux/modprobe/rc1.d)
install(FILES ./modprobe/modprobe.d/fluxion.toml
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/flux/modprobe/modprobe.d)
