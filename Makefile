
hubble: hubble.ml
	ocamlfind opt -linkpkg -package 'curl, zarith' hubble.ml -o hubble
