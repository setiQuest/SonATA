################################################################################
#
# File:    Makefile.am
# Project: OpenSonATA
# Authors: The OpenSonATA code is the result of many programmers
#          over many years
#
# Copyright 2011 The SETI Institute
#
# OpenSonATA is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# OpenSonATA is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with OpenSonATA.  If not, see<http://www.gnu.org/licenses/>.
# 
# Implementers of this code are requested to include the caption
# "Licensed through SETI" with a link to setiQuest.org.
# 
# For alternate licensing arrangements, please contact
# The SETI Institute at www.seti.org or setiquest.org. 
#
################################################################################

## compile with -DACE_ENABLE_TIMEPROBES to use timeprobes
## Process this file with automake to produce Makefile.in

# DEBUGFLAGS is a hook for easily adding flags on the make command line.

DEFS = -DHAVE_CONFIG_H -I. -I$(srcdir) -I$(top_builddir) \
	 -DSSECOMMUTIL_VERBOSE_OVERRIDE @DEFS@

#For gcov (coverage) profiling:
#GCOV_FLAGS = -fprofile-arcs -ftest-coverage
#DEBUGFLAGS=$(GCOV_FLAGS)

# for gprof (time) profiling
#GPROF_FLAGS = -pg
#DEBUGFLAGS=$(GPROF_FLAGS)
# for ggdb
GGBD_FLAGS = -ggdb
DEBUGFLAGS=$(GGBD_FLAGS)

CXXFLAGS = @CXXFLAGS@ $(DEBUGFLAGS) 

AUTOMAKE_OPTIONS = foreign

bin_PROGRAMS = seeker validateExpectedNssComponentsTree \
	printSseSetupDir \
	printSseMsgDoc gseToRadec \
	testOffPosAgainstObsList findNearbyTargets \
	findtargets \
	testOrderedTargets 

#observed
# testOrderedTargetsEclip


# TBD: update to match Target/OrderedTargets changes
# StarFinder

EXTRA_DIST = NssParameters.i \
		NssParameterstcl.i \
		tclsh.i 

#EXTRA_PROGRAMS = 

BUILT_SOURCES = NssParameterstcl_wrap.cpp

check_PROGRAMS = testUnitSse \
	testCondition \
	RangeTest

TESTS = $(check_PROGRAMS)


LIB_DEPENDS = \
	$(DOPPLER_LIB) $(SSEUTIL_LIB) \
	$(SSE_INTERFACE_LIB) \
	$(SSECOMMUTIL_LIB) \
	$(SSE_DX_INTERFACE_LIB) \
	$(SSE_DX_ARCHIVER_INTERFACE_LIB) \
	$(SSE_CHANNELIZER_INTERFACE_LIB) \
        $(SSE_TSCOPE_INTERFACE_LIB)

testUnitSse_DEPENDENCIES = $(LIB_DEPENDS)
seeker_DEPENDENCIES = $(LIB_DEPENDS)

INCLUDES= \
	-I$(ACE_ROOT) -I$(SSECOMMUTIL_INC_DIR)  -I$(SSEUTIL_INC_DIR) \
         -I$(SSE_INTERFACE_INC_DIR) \
         -I$(SSE_TSCOPE_INTERFACE_INC_DIR) \
         -I$(SSE_DX_INTERFACE_INC_DIR)	\
         -I$(SSE_DX_ARCHIVER_INTERFACE_INC_DIR) \
         -I$(SSE_CHANNELIZER_INTERFACE_INC_DIR) \
	 -I$(CPPUNIT_ROOT)/include -I$(top_srcdir)/include \
	 -I$(DOPPLER_INC_DIR) \
         -I$(MYSQL_ROOT)/include \
	 -I/usr/include/mysql

SSE_LIBS = \
  $(SSECOMMUTIL_LIB) \
  $(SSEUTIL_LIB) \
  $(SSE_DX_INTERFACE_LIB) \
  $(SSE_DX_ARCHIVER_INTERFACE_LIB) \
  $(SSE_CHANNELIZER_INTERFACE_LIB) \
  $(SSE_INTERFACE_LIB) \
  $(SSE_TSCOPE_INTERFACE_LIB) \
  $(DOPPLER_LIB) \
  -L$(ACE_ROOT)/ace  -lACE \
  -L$(MYSQL_ROOT)/lib \
  -lmysqlclient_r \
  $(AAPLUS_LIB) \
  $(WCS_LIB) \
  $(NOVAS_LIB) \
  -lz \
  -lnsl \
  -lrt

# Note: -lz (zlib compression library) is needed for
# some builds of the mysqlclient_r library

LDFLAGS = -R$(ACE_ROOT)/ace -R$(MYSQL_ROOT)/lib
LDADD = $(SSE_LIBS)

NssParameterstcl_wrap.cpp: NssParameterstcl.i NssParameters.i tclsh.i
	swig -nodefault -tcl8 -c++ $(DEFS) -o NssParameterstcl_wrap.cpp $(srcdir)/NssParameterstcl.i


# This list is meant to be in roughly dependency order, with
# the latter entries dependent on the earlier ones.

seeker_MOST_SOURCES = \
	DebugLog.h \
	DebugLog.cpp \
	AtaInformation.h \
	AtaInformation.cpp \
	MsgSender.h \
	MysqlResultSet.h \
	MysqlResultSet.cpp \
	MysqlQuery.h \
	MysqlQuery.cpp \
	MutexBool.h \
	MutexBool.cpp \
	readMask.h \
	readMask.cpp \
	SignalMask.h \
	SignalMask.cpp \
	SignalReport.h \
	SignalReport.cpp \
	CondensedSignalReport.h \
	CondensedSignalReport.cpp \
	ExpandedSignalReport.h \
	ExpandedSignalReport.cpp \
	ExpectedNssComponentsTree.h \
	ExpectedNssComponentsTree.cpp \
	NssComponentTree.h \
	NssComponentTree.cpp \
	ObsSummaryStats.h \
	ObsSummaryStats.cpp \
	ObserveActivityStatus.h \
	ObserveActivityStatus.cpp \
	ObserveActivityOps.h \
	ActivityId.h \
	Id.cpp \
	Id.h \
	IdDb.cpp \
	IdDb.h \
	IdNumberFactory.cpp \
	IdNumberFactory.h \
	SharedProxy.h \
	SharedProxy.cpp \
	SharedTclProxy.h \
	SharedTclProxy.cpp \
	DxList.h \
	DxProxy.h \
	DxProxy.cpp \
	DxArchiverList.h \
	DxArchiverProxy.h \
	DxArchiverProxy.cpp \
	ChannelizerList.h \
	ChannelizerProxy.h \
	ChannelizerProxy.cpp \
	IfcStatus.h \
	IfcStatus.cpp \
	IfcIntrinsics.h \
	IfcIntrinsics.cpp \
	IfcNames.h \
	IfcList.h \
	IfcProxy.cpp \
	IfcProxy.h \
	TestSigStatus.h \
	TestSigStatus.cpp \
	TestSigIntrinsics.h \
	TestSigIntrinsics.cpp \
	TestSigNames.h \
	TscopeList.h \
	TscopeProxy.cpp \
	TscopeProxy.h \
	TestSigList.h \
	TestSigProxy.cpp \
	TestSigProxy.h \
	ComponentControlList.h \
	ComponentControlProxy.cpp \
	ComponentControlProxy.h \
	Scheduler.cpp \
	Scheduler.h \
	SeekerParameterGroup.h \
	SeekerParameterGroup.cpp \
	ActParameters.h \
	ActParameters.cpp \
	DxParameters.h \
	DxParameters.cpp \
	DxArchiverParameters.h \
	DxArchiverParameters.cpp \
	ChannelizerParameters.h \
	ChannelizerParameters.cpp \
	IfcParameters.cpp \
	IfcParameters.h \
	IfcImmedCmds.cpp \
	IfcImmedCmds.h \
	TscopeParameters.cpp \
	TscopeParameters.h \
	DbParameters.cpp \
	DbParameters.h \
	TestSigParameters.cpp \
	TestSigParameters.h \
	TestSigImmedCmds.cpp \
	TestSigImmedCmds.h \
	ComponentControlImmedCmds.cpp \
	ComponentControlImmedCmds.h \
	Range.cpp \
	Range.h \
	TargetId.h \
	TargetIdSet.h \
	Target.cpp \
	Target.h \
	TargetMerit.h \
	TargetMerit.cpp \
	SiteView.h \
	SiteView.cpp \
	TargetPosition.cpp \
	TargetPosition.h \
	OrderedTargets.cpp \
	OrderedTargets.h \
	SchedulerParameters.cpp \
	SchedulerParameters.h \
	NssParameters.cpp \
	NssParameters.h \
	Activity.cpp \
	Activity.h \
	WriteScienceData.h \
	WriteScienceData.cpp \
	ScienceDataArchive.h \
	ScienceDataArchive.cpp \
	RecordInDatabase.h \
	RecordInDatabase.cpp \
	FollowUpSignalInfo.h \
	FollowUpSignalInfo.cpp \
	RecordDxInfoInDb.h \
	RecordDxInfoInDb.cpp \
	DbQuery.h \
	DbQuery.cpp \
	RecentRfiMask.h \
	RecentRfiMask.cpp \
	ActivityUnit.h \
	ActivityUnit.cpp \
	ActivityUnitImp.h \
	ActivityUnitImp.cpp \
	ActUnitList.h \
	ActUnitListMutexWrapper.h \
	ActUnitListMutexWrapper.cpp \
	SiteSubscriber.h \
	DxComponentManager.h \
	DxComponentManager.cpp \
	Site.h \
	Site.cpp \
	BoolString.h \
	BoolString.cpp \
	MinMaxDxSkyFreqMhz.h \
	MinMaxDxSkyFreqMhz.cpp \
	OffPositions.h \
	OffPositions.cpp \
	Spacecraft.h \
	Spacecraft.cpp \
	ObserveActivity.cpp \
	ObserveActivity.h \
	ObserveActivityImp.cpp \
	ObserveActivityImp.h \
	ActivityWrappers.h \
	ActivityWrappers.cpp \
	ActivityStrategy.cpp \
	ActivityStrategy.h \
	ObsActStrategy.cpp \
	ObsActStrategy.h \
	CalTargets.h \
	CalTargets.cpp \
	AutoselectAntsActStrategy.h \
	AutoselectAntsActStrategy.cpp \
	TscopeSetupActStrategy.h \
	TscopeSetupActStrategy.cpp \
	PrepAntsActStrategy.h \
	PrepAntsActStrategy.cpp \
	FreeAntsActStrategy.h \
	FreeAntsActStrategy.cpp \
	BfActStrategy.h \
	BfActStrategy.cpp \
	BfResetActStrategy.h \
	BfResetActStrategy.cpp \
	BfInitActStrategy.h \
	BfInitActStrategy.cpp \
	BfAutoattenActStrategy.h \
	BfAutoattenActStrategy.cpp \
	CalActStrategy.h \
	CalActStrategy.cpp \
	CalDelayActStrategy.h \
	CalDelayActStrategy.cpp \
	CalPhaseActStrategy.h \
	CalPhaseActStrategy.cpp \
	CalFreqActStrategy.h \
	CalFreqActStrategy.cpp \
	ActStratWrappers.h \
	ActStratWrappers.cpp \
	ReceivedDxMsgBitset.h \
	Followup.cpp \
	Followup.h \
	Condition.h \
	mld.cpp \
	mld.h \
	UnitOfWork.h \
	MessageBlock.h \
	DataBlock.h \
	TuneDxs.cpp \
	TuneDxs.h \
	SseComponentManager.h \
	ActivityException.h    \
	MinMaxBandwidth.h \
	PermRfiMaskFilename.h \
	SeekerCmdLineParser.h \
	SeekerCmdLineParser.cpp \
	SetiLiveSignalInfo.h \
	ThisFileDoesNotExist.cpp \
	SetiLiveSignalInfo.cpp

seeker_SOURCES = $(seeker_MOST_SOURCES) \
			seeker.cpp \
			seeker.h \
			NssParameterstcl_wrap.cpp


seeker_LDADD = $(SSE_LIBS)

testUnitSse_SOURCES = \
  testUnitSse.cpp \
  TestSite.cpp \
  TestSite.h \
  TestMisc.cpp \
  TestMisc.h \
  TestOffPositions.h \
  TestOffPositions.cpp \
  TestTuneDxs.cpp \
  TestTuneDxs.h \
  TestRecentRfiMask.h \
  TestRecentRfiMask.cpp \
  TestPosition.h \
  TestPosition.cpp \
  TestSiteView.h \
  TestSiteView.cpp \
  TestTarget.cpp \
  TestTarget.h \
  $(seeker_MOST_SOURCES)



testUnitSse_LDADD = -L$(CPPUNIT_ROOT)/lib -lcutextui -lcu $(SSE_LIBS)

testCondition_SOURCES = \
	testCondition.cpp \
	Condition.h 

testCondition_LDADD = \
	 -L$(ACE_ROOT)/ace -lACE \
	-L$(CPPUNIT_ROOT)/lib -lcutextui -lcu -lrt


RangeTest_SOURCES = RangeTest.cpp \
		Range.cpp \
		Range.h

RangeTest_LDADD = \
		$(SSE_DX_INTERFACE_LIB) \
		-L$(CPPUNIT_ROOT)/lib -lcutextui -lcu

validateExpectedNssComponentsTree_SOURCES = \
  ExpectedNssComponentsTree.cpp \
  validateExpectedNssComponentsTree.cpp

validateExpectedNssComponentsTree_LDADD = $(SSEUTIL_LIB)

printSseSetupDir_SOURCES = \
  printSseSetupDir.cpp

printSseSetupDir_LDADD = $(SSECOMMUTIL_LIB) $(SSEUTIL_LIB)

printSseMsgDoc_SOURCES = printSseMsgDoc.cpp

gseToRadec_SOURCES = gseToRadec.cpp

findNearbyTargets_SOURCES = findNearbyTargets.cpp MysqlQuery.cpp
findNearbyTargets_LIBS = $(SSECOMMUTIL_LIB) $(AAPLUS_LIB) $(WCS_LIB) $(NOVAS_LIB) $(SSEUTIL_LIB)
findNearbyTargets_LDADD = $(findNearbyTargets_LIBS) \
    -L$(MYSQL_ROOT)/lib -lmysqlclient -lz

findNearbyTargets_DEPENDENCIES = $(findNearbyTargets_LIBS)

findtargets_SOURCES = \
	findtargets.cpp \
	MysqlQuery.h \
	MysqlQuery.cpp \
	Spacecraft.h \
	Spacecraft.cpp \
	CalTargets.h \
	CalTargets.cpp

findtargets_LIBS = $(SSECOMMUTIL_LIB) $(AAPLUS_LIB) $(WCS_LIB) \
	 $(NOVAS_LIB) $(SSEUTIL_LIB) $(DOPPLER_LIB)
findtargets_LDADD = $(findtargets_LIBS) \
     -L$(MYSQL_ROOT)/lib -lmysqlclient -lz

findtargets_DEPENDENCIES = $(findtargets_LIBS)

#StarFinder_SOURCES = StarFinder.cpp \
		StarFinder.h \
		 Target.h \
		 Target.cpp \
		 TargetPosition.h \
		 TargetPosition.cpp \
		 Range.h \
		 Range.cpp \
		 MysqlResultSet.h \
		 MysqlResultSet.cpp


#observed_SOURCES = observed.cpp \
		DebugLog.h \
		DebugLog.cpp \
		Target.h \
		Target.cpp \
		TargetPosition.h \
		TargetPosition.cpp \
		Range.h \
		Range.cpp \
		OrderedTargets.cpp \
		OrderedTargets.h \
		SignalMask.cpp \
		SignalMask.h \
		readMask.h \
		readMask.cpp \
		DxProxy.h \
		DxProxy.cpp \
		SharedProxy.h \
		SharedProxy.cpp \
		DxComponentManager.h \
		DxComponentManager.cpp \
		PermRfiMaskFilename.h \
		ExpectedNssComponentsTree.h \
		ExpectedNssComponentsTree.cpp \
		MysqlResultSet.h \
		MysqlResultSet.cpp \
		MysqlQuery.h \
		MysqlQuery.cpp \
		AtaInformation.h \
		AtaInformation.cpp \
		OffPositions.h \
		OffPositions.cpp

testOrderedTargets_SOURCES = testOrderedTargets.cpp \
		DebugLog.h \
		DebugLog.cpp \
		AtaInformation.h \
		AtaInformation.cpp \
		SiteView.h \
		SiteView.cpp \
		Target.h \
		Target.cpp \
		TargetMerit.h \
		TargetMerit.cpp \
		TargetPosition.h \
		TargetPosition.cpp \
		Range.h \
		Range.cpp \
		OrderedTargets.cpp \
		OrderedTargets.h \
		MysqlResultSet.h \
		MysqlResultSet.cpp \
		MysqlQuery.h \
		MysqlQuery.cpp \
		OffPositions.h \
		OffPositions.cpp

#testOrderedTargetsEclip_SOURCES = testOrderedTargetsEclip.cpp \
		Target.h \
		Target.cpp \
		TargetPosition.h \
		TargetPosition.cpp \
		Range.h \
		Range.cpp \
		OrderedTargets.cpp \
		OrderedTargets.h \
		MysqlResultSet.h \
		MysqlResultSet.cpp \
		MysqlQuery.h \
		MysqlQuery.cpp


testOffPosAgainstObsList_SOURCES = testOffPosAgainstObsList.cpp \
		AtaInformation.h \
		AtaInformation.cpp \
		OffPositions.h \
		OffPositions.cpp

#observed_LDADD = -L$(MYSQL_ROOT)/lib -lmysqlclient $(SSE_LIBS)

#StarFinder_LDADD = -L$(MYSQL_ROOT)/lib -lmysqlclient $(SSE_LIBS)

testOrderedTargets_LDADD = -L$(MYSQL_ROOT)/lib -lmysqlclient $(SSE_LIBS)

#testOrderedTargetsEclip_LDADD = -L$(MYSQL_ROOT)/lib -lmysqlclient $(SSE_LIBS)

gseToRadec_LDADD = $(SSE_LIBS)

testOffPosAgainstObsList_LDADD = $(SSE_LIBS)

# Create a package dependency report using the Lakos idep software.

REPORT=lakos-depend.txt
DEPFILE=/tmp/depfile
IDEP_BINDIR = $(top_builddir)/idep

lakosdepend:
	rm -f $(REPORT)
# put out the report header
	echo "Lakos dependency check for" >> $(REPORT)
	pwd >> $(REPORT)
	date >> $(REPORT)
	echo "" >> $(REPORT)

# extract the dependencies
	$(IDEP_BINDIR)/cdep -I. $(INCLUDES) $(seeker_SOURCES)  > $(DEPFILE)

# analyze the dependencies
	$(IDEP_BINDIR)/ldep -d$(DEPFILE) -l >> $(REPORT)
	rm $(DEPFILE)


clean-gcov:
	rm -f *.bb *.bbg *.gcov *.da
