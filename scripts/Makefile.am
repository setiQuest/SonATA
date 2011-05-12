
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
	cleanup-chan-dx.tcsh \
	1beam-dualpol-800KHz-24dx-env-vars.tcsh \
	2beam-dualpol-800KHz-48dx-env-vars.tcsh \
	3beam-dualpol-800KHz-72dx-env-vars-batch.sh \
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
	setiquest-12dxs.tcl \
	setiquest-env-vars.tcsh \
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
	sonata-start-rfiscan-1400-1750-full-array.tcl \
	sonata-start-rfiscan-1750-1950-full-array.tcl \
	sonata-start-rfiscan-1950-2150-full-array.tcl \
	sonata-start-rfiscan-2150-2320-full-array.tcl \
	sonata-start-rfiscan-2350-2550-full-array.tcl \
	sonata-start-rfiscan-2550-2750-full-array.tcl \
	sonata-start-rfiscan-3040-3240-full-array.tcl \
	sonata-start-rosetta-4l-obs.tcl \
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
	data-collect-2beam-env-vars.tcsh \
	data-collect-3beam-env-vars.tcsh \
	data-collect-3beam-Kepler-HZ-env-vars.tcsh \
	data-capture-2beam-1500-1520MHz.tcl \
	data-capture-3beam-1500-1520MHz.tcl \
	data-capture-kepler-2beam.tcl \
	data-capture-kepler-3beam.tcl \
	data-capture-replay-beam1-kepler.tcl \
	data-capture-replay-beam1-1500-1520MHz.tcl \
	data-capture-replay-env-vars.tcsh




