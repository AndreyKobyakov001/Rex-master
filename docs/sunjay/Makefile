# Run `make` to build all the PDFs.
# You need to have pandoc and texlive-full installed.

all: s5varma.log.pdf

%.pdf: %.md
	# In later versions of pandoc, --latex-engine is changed to --pdf-engine
	pandoc $< --toc --latex-engine=xelatex -o $@ -V geometry:margin=1in --template templates/work-log.latex

.PHONY: all clean

clean:
	$(RM) *.pdf
