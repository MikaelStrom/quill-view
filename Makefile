quill-view: quill-view.c
	gcc -funsigned-char -o quill-view quill-view.c

clean:
	rm -f quill-view
