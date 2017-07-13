MAKE = make
n ?= 29

all: 
	@gzip -d gzip_input_files/*.gz
	@cd gzip_input_files; \
	n=$(n); \
	while [ $${n} -gt 1 ];\
	do \
		cat 01_bible.txt >> 02_bigBible.txt ; \
		n=`expr $$n - 1`; \
	done
	@cd gzip_input_files; \
	cat 02_bigBible.txt >> 03_hugeBible.txt ; \
	cat 02_bigBible.txt >> 03_hugeBible.txt ; \
	cat 02_bigBible.txt >> 03_hugeBible.txt
	@rm -rf gzip_input_files/*.gz
	
clean: ; 

.PHONY: all
.PHONY: clean
