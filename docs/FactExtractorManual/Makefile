# Run `make` to build all the PDFs.
# You need to have texlive-full installed.

all: FactExtractorManual.pdf

FactExtractorManual.pdf: FactExtractorManual.tex StylePreamble.tex $(wildcard Images/*.pdf)
	pdflatex FactExtractorManual.tex

.PHONY: all clean

clean:
	$(RM) *.pdf *.toc *.aux *.log *.out
