MD_FILES=$(wildcard *.text)
GRAPH_FILES=$(patsubst %.gv,%.png,$(wildcard *.gv))
TEX_FILES=$(wildcard *.tex)
PDF_FILES=$(patsubst %.tex,%.pdf,$(TEX_FILES))

all: $(PDF_FILES)

# Make a PDF from our tex file.  This requires the graphs.
%.pdf: %.tex graphs
	pdflatex -interaction batchmode $<
	bibtex $(patsubst %.tex,%.aux,$<)
	pdflatex -interaction batchmode $<

# Make a tex file from a markdown file.
protocol.tex: $(MD_FILES)
	pandoc -s -f markdown -t latex -o protocol.tex protocol.text

%.png: %.gv
	dot -Tpng -o$@ $<

graphs: $(GRAPH_FILES)

clean:
	rm -f $(PDF_FILES) $(patsubst %.text,%.html,$(MD_FILES)) \
		$(patsubst %.pdf,%.log,$(PDF_FILES)) \
		$(patsubst %.pdf,%.aux,$(PDF_FILES)) \
		$(GRAPH_FILES)


