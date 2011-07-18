# Create UML sequence diagrams in pdf format from
# 'pic' files.  See the umlgraph package for details:
# http://www.spinellis.gr/sw/umlgraph/

targets = dxSseDataCollMsgs.pdf \
	dxSseSigDetMsgs.pdf \
	dxSseSigDetMsgsMultibeamPart1.pdf \
	dxSseSigDetMsgsMultibeamPart2.pdf \
	beamformerCalAct.pdf \
	beamformerObsAct.pdf \
	schedulerRunActSeq.pdf

all: $(targets)

$(targets): %.pdf: %.pic
	pic2plot -Tps $< | ps2pdf - > $@

clean:
	rm $(targets)
