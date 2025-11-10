all:
	$(MAKE) -C part1
	$(MAKE) -C part2
	$(MAKE) -C part3

clean:
	$(MAKE) -C part1 clean
	$(MAKE) -C part2 clean
	$(MAKE) -C part3 clean

