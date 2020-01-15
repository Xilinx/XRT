OTHERDIR=../../../src/python

run:
	@python main.py -k $(filter-out $@, $(MAKECMDGOALS))

clean:
	@find . -name '*.pyc' -delete
	@find $(OTHERDIR) -name '*.pyc' -delete

help:
	@python -c 'from main import printHelp; print printHelp();'

%:
	@true