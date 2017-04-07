all:
	docker build -t bitcoind-build .
	rm -rf output && mkdir output
	docker run -t -v `pwd`/output:/output bitcoind-build /bin/sh -c 'cp /bitcoin/*.tar.gz /output'
