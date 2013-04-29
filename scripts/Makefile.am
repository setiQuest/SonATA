
# Scripts for running SonATA
#
# Environmental Variable files *env-vars.tcsh or *env-vars-batch.sh
#	Assignment of logical channelizers and dxs to specific hardware
#	and command line parameters
# SwitchConfigFile* loads the corresponding internal configuration file.
#
# Tcl files
#	Parameter definition and commands for the seeker
#
# Cron session files *session.sh
#	Files executed by cron to run the system in batch mode.

AUTOMAKE_OPTIONS = foreign

SUBDIRS = \
	jane \
	kes

localinstall:
	make install prefix=`pwd`/scripts

noinst_SCRIPTS = reconfig
EXTRA_DIST = reconfig

# Force creation of the scripts directory
scriptsdir = $(prefix)/scripts/

scripts_SCRIPTS = \
	jane-test-env-vars.tcsh \
	cleanup-chan-dx.tcsh \
	1beam-dualpol-800KHz-24dx-env-vars.tcsh \
	1beam-beam2-dualpol-800KHz-24dx-env-vars.tcsh \
	tscope-setup-1beam-beam2-1a.tcl \
	mars-express-1beam-beam2-8419-8439.tcl \
	switchConfigFile-1beam-beam2-800KHz-24dxs.tcsh \
	1beam-beam2-seti001-800KHz-24dx-env-vars.tcsh \
	2beam-dualpol-800KHz-48dx-env-vars.tcsh \
	3beam-dualpol-800KHz-72dx-env-vars-batch.sh \
	2beam-beam1-beam3-dualpol-800KHz-48dx-env-vars-batch.sh \
	3beam-dualpol-800KHz-72dx-env-vars.tcsh \
	3beam-xpol-800KHz-72dx-env-vars.tcsh \
	data-collect-2beam-env-vars.tcsh \
	bandwidth-test.tcsh \
	getdelaytargets.tcsh \
	getphasetargets.tcsh \
	gettargets.tcsh \
	jane-3beam-env-vars.tcsh \
	jane-test-pulse.tcl \
	jane-beam2-xpol-800KHz-24dx-env-vars.tcsh \
	jane-kepler-1beam-20MHz.tcl \
	jane-test.tcl \
	test.tcl \
	jon-kepler1.tcl \
	jon-kepler2.tcl \
	kepler-demo-dualpol.tcl \
	kes-deep-impact-2beam-500MHz.tcl \
	kes-deep-impact-3beam-200MHz.tcl \
	kes-deep-impact-3beam-500MHz.tcl \
	kes-gettargets.tcsh \
	kes-kepler-3beam-350MHz.tcl \
	kes-mars-express-1beam-500MHz.tcl \
	kes-mars-express-2beam-500MHz.tcl \
	kes_rosetta-1beam-500MHz.tcl \
	kes-rosetta-2beam-500MHz.tcl \
	kes-rosetta-3beam-200MHz.tcl \
	kes-rosetta-3beam-350MHz.tcl \
	kes-rosetta-3beam-500MHz.tcl \
	kes-sirocco-1beam-dualpol-400KHz-64f-env-vars.tcsh \
	kes-sirocco-1beam-xpol-1_6MHz_32f_env_vars.tcsh \
	kes-sirocco-1beam-xpol-3_2MHz_32f_env_vars.tcsh \
	kes-sirocco-1beam-xpol-400KHz-64f-env-vars.tcsh \
	kes-sirocco-1beam-xpol-800KHz-64f-env-vars.tcsh \
	kes-sirocco-1beam-xpol-800KHz-env-vars.tcsh \
	kes-sirocco-2beam-dualpol-400KHz-64f-env-vars.tcsh \
	kes-sirocco-2beam-dualpol-800KHz-64f-env-vars.tcsh \
	kes-sirocco-2beam-test-64f.tcl \
	kes-sirocco-test-400KHz-500MHz.tcl \
	kes-sirocco-test-64f.tcl \
	kes-sirocco-test-800KHz-500MHz.tcl \
	kes-sirocco-test.tcl \
	kes-sirocco-xpol-400KHz-64f-env-vars.tcsh \
	kes-bedlam-1beam-xpol-800KHz-1dx-1zx-env-vars.tcsh \
	kes-bedlam-1beam-xpol-800KHz-env-vars.tcsh \
	kes-switchConfigFile-1beam-800KHz-1dx-1zx.tcsh \
	kes-switchConfigFile-2beam-2dx.tcsh \
	kes-tscope-vger-full-array-2.tcl \
	kes-tscope-vger-full-array-beam1.tcl \
	kes-tscope-vger-full-array.tcl \
	kes-vger-1beam-20MHz.tcl \
	kes-vger-3beam-20MHz.tcl \
	rebuild-sig-pkg.tcsh \
	rebuild-sse.tcsh \
	rosetta-demo-xpol.tcl \
	runpacketgen-chanx-8-pulse.tcsh \
	runpacketgen-chanx-no-signal.tcsh \
	runpacketgen-dx-pulse.tcsh \
	runpacketgen-dx.tcsh \
	runpacketgen-dx-xonly.tcsh \
	runpacketread-chan1x.tcsh \
	runpacketsend-dx-demo-test.tcsh \
	runpacketsend-dx-kepler-dualpol.tcsh \
	runpacketsend-dx-rosetta-xpol.tcsh \
	runpacketsend-dx-vger-dualpol.tcsh \
	runpacketsend-dx-vger-xpol.tcsh \
	runpacketsend-setiquest.tcsh \
	runpacketsend-data-capture-replay.tcsh \
	lab-runpacketsend-data-capture-replay.tcsh \
	setiquest-12dxs.tcl \
	setiquest-env-vars.tcsh \
	lab-setiquest-env-vars.tcsh \
	sonata-cron-begin-cygnus-exoplanets-session.sh \
	sonata-cron-begin-cygnusx3-session.sh \
	sonata-cron-begin-exoplanets-session.sh \
	sonata-cron-begin-waterhole2x-2840-3040MHz-session.sh \
	sonata-cron-begin-waterhole2x-3040-3240MHz-session.sh \
	sonata-cron-begin-waterhole2x-3240-3440MHz-session.sh \
	sonata-cron-end-obs-session.sh \
	sonata-start-cygnus-exoplanets-obs-noncommensal.tcl \
	sonata-start-cygnusx3-obs.tcl \
	sonata-start-exoplanets-obs.tcl \
	sonata-start-exoplanets-3beam-8500-8700-obs.tcl \
	sonata-start-exoplanets-beam1-beam3-obs.tcl \
	sonata-start-rfiscan-2beams-1000-1200-obs.tcl \
	sonata-stop-rfiscan.tcl \
	sonata-start-rfiscan-1700-2000-obs.tcl \
	sonata-start-rfiscan-3700-4020-obs.tcl \
	sonata-start-rfiscan-4020-4340-obs.tcl \
	sonata-start-rfiscan-1400-1750-full-array.tcl \
	sonata-start-rfiscan-1750-1950-full-array.tcl \
	sonata-start-rfiscan-1950-2150-full-array.tcl \
	sonata-start-rfiscan-2150-2320-full-array.tcl \
	sonata-start-rfiscan-2350-2550-full-array.tcl \
	sonata-start-rfiscan-2550-2750-full-array.tcl \
	sonata-start-rfiscan-3040-3240-full-array.tcl \
	sonata-start-rosetta-4l-obs.tcl \
	sonata-start-rosetta-1a-beam2-obs.tcl \
	sonata-start-rosetta-full-array-obs.tcl \
	sonata-start-waterhole2x-2840-3040MHz-obs.tcl \
	sonata-start-waterhole2x-3040-3240MHz-obs.tcl \
	sonata-start-waterhole2x-3240-3440MHz-obs.tcl \
	sonata-stop-obs.tcl \
	spacecraft-demo-dualpol-env-vars.tcsh \
	spacecraft-demo-xpol-env-vars.tcsh \
	sserc-auto.tcl \
	sserc.tcl \
	switchConfigFile-1beam-800KHz-1dx.tcsh \
	switchConfigFile-1beam-1dx.tcsh \
	switchConfigFile-2beam-data-collect-2dx.tcsh \
	switchConfigFile-3beam-data-collect-3dx.tcsh \
	switchConfigFile-1beam-800KHz-24dxs.tcsh \
	switchConfigFile-3beam-800KHz-72dxs.tcsh \
	switchConfigFile-beam2-800KHz-24dxs.tcsh \
	switchConfigFile-jane-test.tcsh \
	switchConfigFile-setiquest.tcsh \
	tscope-setup-1beam-4l.tcl \
	tscope-setup-2beam-4l.tcl \
	tscope-setup-3beam-4l.tcl \
	tscope-setup-3beam-1a.tcl \
	tscope-setup-2beam-1a.tcl \
	3beam-dualpol-amc-7.tcl \
	rosetta-3beam-1a.tcl \
	tscope-setup-3beam-good-ants.tcl \
	tscope-setup-3beam-good-ants-8405-8445.tcl \
	exoplanets-1420-1750.tcl \
	exoplanets-1beam-1420-1450.tcl \
	tscope-setup-3beam-1410-1471-good-ants.tcl \
	switch-beams-env-vars.tcsh \
	mars-3beam-8405-8445.tcl \
	mars-2beam-8405-8445.tcl \
	sonata-start-rosetta-beam2-good-ants.obs.tcl \
	1beam-beam2-rosetta.tcl \
	tscope-setup-1beam-vger-full.tcl \
	tscope-setup-beam2-4l.tcl \
	vger-demo-dualpol.tcl \
	vger-demo.tcl \
	vger-demo-xpol.tcl \
	sonata-wrapup.tcl \
	sonata-cron-switch-to-kepler-targets.sh* \
	sonata-switch-to-kepler-targets.tcl \
	sonata-cron-begin-gal-survey-session.sh* \
	sonata-start-gal-survey-obs.tcl \
	runpacketrelay-beam1.tcsh* \
	runpacketrelay-beam2.tcsh* \
	runpacketrelay-beam3.tcsh* \
	tscope-setup-2beam-4l-data-collect.tcl \
	tscope-setup-2beam-4l-1500-1520MHz.tcl \
	tscope-setup-2beam-data-collect.tcl \
	tscope-setup-3beam-data-capture-1500-1520MHz.tcl \
	tscope-setup-3beam-data-capture-1500-1540MHz.tcl \
	tscope-setup-3beam-data-capture-8420-8430MHz.tcl \
	tscope-setup-2beam-data-capture-8420-8430MHz.tcl \
	data-collect-kepler-2beam-800KHz.tcl \
	data-collect-3beam-env-vars.tcsh \
	data-collect-3beam-Kepler-HZ-env-vars.tcsh \
	data-capture-2beam-1500-1520MHz.tcl \
	data-capture-3beam-1500-1520MHz.tcl \
	data-capture-kepler-2beam.tcl \
	data-capture-kepler-3beam.tcl \
	data-capture-replay-beam1-kepler.tcl \
	data-capture-replay-beam1-1500-1520MHz.tcl \
	runpacketsend-beam3-data-capture-replay.tcsh \
	runpacketsend-beam2-data-capture-replay.tcsh \
	runpacketsend-beam1-data-capture-replay.tcsh \
	switchConfigFile-dualbeam-replay.tcsh \
	data-replay-dualbeam-xpol-env-vars.tcsh \
	replay-2011-03-31-23.45.1700.5168.tcl \
	replay-2010-11-06-dorothy_tauceti_1420_1.tcl \
	replay-2011-03-31-20.49.1513.5168.tcl \
	replay-2011-03-31-21.11.1523.5168.tcl \
	replay-2011-03-31-21.40.1523.5168.tcl \
	replay-2011-03-31-22.01.1528.5168.tcl \
	replay-2011-03-31-22.22.1518.5168.tcl \
	replay-2011-03-31-23.27.1695.5168.tcl \
	3beam-dualpol-rosetta.tcl \
	2beam-dualpol-beam1-beam3-800KHz-48dx-env-vars.tcsh \
	tscope-setup-2beam-beam1-beam3-1a.tcl \
	tscope-setup-2beam-beam1-beam3-4l.tcl \
	tscope-setup-1beam-1a.tcl \
	exoplanets-2beam-beam1-beam3-1420-1450.tcl \
	exoplanets-2beam-beam1-beam3-1410-1650.tcl \
	mars-2beam-beam1-beam3-8405-8445.tcl \
	mars-2beam-beam1-beam3-8414-8440.tcl \
	switchConfigFile-1beam-beam3-800KHz-24dxs.tcsh \
	1beam-beam3-dualpol-800KHz-24dx-env-vars.tcsh \
	exoplanets-1beam-beam3-1420-1450.tcl \
	tscope-setup-1beam-beam3-4l.tcl \
	switchConfigFile-2beam-beam1-beam3-800KHz-48dxs.tcsh \
	2beam-dualpol-beam1-seti001-beam3-seti002-800KHz-48dx-env-vars.tcsh \
	vger-2beam-beam1-beam3.tcl \
	kepler-2beam-beam1-beam3.tcl \
	tscope-setup-2beam-beam1-beam3-good-ants-1410-1650.tcl \
	exoplanets-3beam-1520-1540.tcl \
	tscope-setup-3beam-good-ants-1520-1540.tcl \
	sonata-cron-begin-kepler-session.sh \
	sonata-start-kepler-3beam-8500-8700-obs.tcl \
	sonata-start-kepler-3beams-1400-1750-obs.tcl \
	sonata-start-exoplanets-3beams-1400-1750-obs.tcl \
	sonata-start-kepler-3beams-1152-1352-obs.tcl \
	sonata-start-exoplanets-3beams-1152-1352-obs.tcl \
	sonata-start-vger-3beam-8412-8432-obs.tcl \
	sonata-start-vger-3beams-8414-8440-obs.tcl \
	sonata-cron-switch-to-exoplanets.sh \
	sonata-switch-to-exoplanets.tcl \
	sonata-switch-to-kepler.tcl \
	sonata-cron-switch-to-kepler.sh \
	sonata-start-exoplanets-2beams-beam1-beam2-8500-8700-obs.tcl \
	sonata-start-kepler-2beams-beam1-beam2-8500-8700-obs.tcl \
	sonata-start-exoplanets-2beams-beam1-beam2-2840-3040-obs.tcl \
	sonata-start-exoplanets-2beams-beam1-beam2-1400-1750-obs.tcl \
	sonata-start-kepler-2beams-beam1-beam2-2840-3040-obs.tcl \
	sonata-start-kepler-2beams-beam1-beam2-1400-1750-obs.tcl \
	switchConfigFile-3beam-800KHz-69dxs-3zx.tcsh \
	3beam-dualpol-800KHz-69dx-3zx-env-vars.tcsh \
	3beam-dualpol-800KHz-69dx-3zx-env-vars-batch.sh \
	sonata-start-rfiscan-1700-5000-4g-obs.tcl \
	sonata-start-rfiscan-1400-1750-4g-obs.tcl \
	antenna-list-master.tcl \
	antenna-list-master-8400mhz.tcl \
	run-rosetta-demo.tcsh \
	run-voyager-demo.tcsh \
	run-kepler-demo.tcsh \
	run-mars-express-demo.tcsh \
	sonata-start-rfiscan-3800-4200-obs.tcl \
	sonata-start-kepler-3beams-3800-4200-obs.tcl \
	sonata-start-exoplanets-3beams-3800-4200-obs.tcl \
	switchConfigFile-2beam-800KHz-46dxs-2zx.tcsh \
	2beam-dualpol-800KHz-46dx-2zx-env-vars-batch.sh \
	sonata-start-kepler-2beams-3800-4200-obs.tcl \
	sonata-start-exoplanets-2beams-3800-4200-obs.tcl \
	sonata-start-vger-2beams-8410-8430-obs.tcl \
	sonata-start-vger-2beams-8414-8424-obs.tcl \
	sonata-start-vger-1beam-8414-8424-obs.tcl \
	sonata-cron-begin-vger-session.sh \
	sonata-start-vger-2beams-8400-8440-obs.tcl \
	3beam-dualpol-800KHz-72dx-512f-env-vars-batch.sh \
	sonata-cron-restart-obs-session.sh \
	sonata-restart-obs.tcl \
	sonata-cron-begin-casa-grid-session.sh \
	sonata-start-casa-grid-2beams-6567-6767-obs.tcl \
	sonata-cron-switch-to-casa-grid.sh \
	sonata-switch-to-casa-grid.tcl \
	sonata-start-exoplanets-2beams-6567-6767-obs.tcl \
	sonata-start-exoplanets-3beams-6567-6767-obs.tcl \
	sonata-start-kepler-2beams-6567-6767-obs.tcl \
	sonata-start-da14-2beam-8400-8850-obs.tcl \
	sonata-cron-begin-da14-session.sh \
	data-capture-replay-env-vars.tcsh \
        sonata-start-perseusa-grid-2900-3100-obs.tcl \
        sonata-start-perseusa-grid-1400-1600-obs.tcl \
        sonata-start-ldn1622-grid-2900-3100-obs.tcl \
        sonata-start-ldn1622-grid-1400-1600-obs.tcl \
        sonata-cron-begin-perseusa-grid-session.sh \
        sonata-cron-begin-ldn1622-grid-session.sh  \
	sonata-start-rfiscan-1480.tcl  \
	sonata-cron-begin-rfiscan.sh \
	sonata-start-rfiscan-1430.tcl \
	sonata-switch-to-perseusa.tcl \
	switchConfigFile-1beam-beam3-800KHz-1dx-1zx.tcsh \
	switchConfigFile-1beam-beam3-800KHz-2dx-1zx.tcsh \
	1beam-beam3-dualpol-2dx-1zx-512F-env-vars.tcsh \
	1beam-beam3-dualpol-800KHz-9dx-1zx-256F-env-vars-batch.sh \
	switchConfigFile-1beam-beam3-800KHz-9dxs-1zx.tcsh \
	testgrid.tcl \
	sonata-switch-to-galcenter.tcl \
	sonata-cron-switch-to-galcenter.sh \
	1beam-dualpol-1dx-1zx-512F-env-vars.tcsh
